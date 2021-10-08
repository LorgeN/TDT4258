#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

// Macro for computing how many bits are required to store unsigned
// values up the the given value
#define BIT_WIDTH(value) log2(value)

typedef enum
{
    dm,
    fa
} cache_map_t;
typedef enum
{
    uc,
    sc
} cache_org_t;
typedef enum
{
    instruction,
    data
} access_t;

typedef struct
{
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct
{
    uint64_t accesses;
    uint64_t hits;
    // You can declare additional statistics if
    // you like, however you are now allowed to
    // remove the accesses or hits
} cache_stat_t;

typedef struct
{
    bool valid;
    uint32_t tag;
    uint64_t inserted_at; // To implement FIFO for FA
} cache_line_t;

/**
 * A struct for a simulated cache. Does not specify type or 
 * mapping. Simply intended to keep track of different values.
 */
typedef struct
{
    uint32_t blocks;
    // We could potentially store the bit masks here since they
    // wont change during runtime
    uint32_t bits_offset;
    uint32_t bits_index;
    uint32_t bits_tag;
    cache_stat_t statistics;
    cache_line_t lines[];
} cache_t;

/**
 * Utility struct for simplifying the transition between unified
 * and split cache.
 */
typedef struct
{
    cache_t *instructions;
    cache_t *data;
} cache_total_t;

/**
 * Gets the tag for the address of the given memory access in the
 * given cache
 */
uint32_t get_tag(cache_t cache, mem_access_t access)
{
    // We compute the bit mask here, and then use a bitwise AND.
    // Sets every bit that we don't care about to 0. We could shift
    // also shift this to the right so we only keep data in the least
    // significant bits but for our usecase this is unnecessary.
    return access.address & ~(0xFFFFFFFF >> cache.bits_tag);
}

/**
 * Allocates a new cache and initializes the values
 */
cache_t *make_cache(uint32_t size, uint32_t block_size, cache_map_t map)
{
    uint32_t blocks = size / block_size;
    // Making sure we don't end up in a situation where we have 0 blocks,
    // which will give weird results later.
    if (blocks == 0)
    {
        printf("Invalid cache size %d! Needs to be larger than %d", size, block_size);
        exit(1);
    }

    // Compute the amount of memory we need to allocate, allocate it
    // and then make sure that the memory doesn't contain any old values
    size_t mem_size = sizeof(cache_t) + (sizeof(cache_line_t) * blocks);
    cache_t *cache = malloc(mem_size);
    memset(cache, 0, mem_size);

    cache->blocks = blocks;
    cache->bits_offset = BIT_WIDTH(block_size);
    cache->bits_index = (map == dm) ? BIT_WIDTH(cache->blocks) : 0;
    cache->bits_tag = 32 - cache->bits_offset - cache->bits_index;

    return cache;
}

cache_total_t *make_total_cache(uint32_t size, uint32_t block_size, cache_map_t map, cache_org_t org)
{
    // We don't need to memset this because we initialize all values later
    cache_total_t *cache = malloc(sizeof(cache_total_t));

    printf("Cache Organization\n");
    printf(" -------------------- \n");

    if (org == sc)
    {
        size >>= 1; // Divide by 2 since the caches should be of equal size

        cache->data = make_cache(size, block_size, map);
        cache->instructions = make_cache(size, block_size, map);
    }
    else
    {
        // Unified cache just means we set the pointers to the same location
        cache_t *unified = make_cache(size, block_size, map);
        cache->data = unified;
        cache->instructions = unified;
    }

    printf("Cache size: %d\n", size);
    printf("Mapping: %s\n", map == dm ? "Direct Mapped" : "Fully Associative");
    printf("Organization: %s\n", org == sc ? "Split Cache" : "Unified Cache");
    printf("Offset: %d\n", cache->data->bits_offset);
    printf("Index: %d\n", cache->data->bits_index);
    printf("Tag: %d\n", cache->data->bits_tag);

    return cache;
}

/**
 * Simulate memory access with fully associative cache
 */
void access_mem_fa(cache_t *cache, cache_stat_t *statistics, mem_access_t access)
{
    uint32_t tag = get_tag(*cache, access);

    statistics->accesses++;
    cache->statistics.accesses++;

    uint32_t index = 0;

    for (uint32_t i = 0; i < cache->blocks; i++)
    {
        cache_line_t curr = cache->lines[i];
        if (!curr.valid)
        {
            index = i;
            continue;
        }

        if (cache->lines[index].inserted_at > curr.inserted_at)
        {
            index = i;
        }

        if (curr.tag != tag)
        {
            continue;
        }

        cache->statistics.hits++;
        statistics->hits++;
        return;
    }

    cache_line_t line = cache->lines[index];

    // Only keep track of when inserted since we are implementing a FIFO queue.
    // Change this to update with every access to implement LRU cache.
    line.inserted_at = statistics->accesses;
    line.valid = true;
    line.tag = tag;

    cache->lines[index] = line;
}

/**
 * Gets the index of the given memory access in the given cache when
 * using direct mapping
 */
uint32_t get_index(cache_t cache, mem_access_t access)
{
    // Make bit mask to grab the index bits
    uint32_t mask = ~(0xFFFFFFFF << cache.bits_index) << cache.bits_offset;
    return (access.address & mask) >> cache.bits_offset;
}

/**
 * Simulate memory access with directly mapped cache
 */
void access_mem_dm(cache_t *cache, cache_stat_t *statistics, mem_access_t access)
{
    uint32_t tag = get_tag(*cache, access);
    uint32_t index = get_index(*cache, access);

    // This shouldn't ever happen, but we check it here to make sure we
    // don't step outside the allocated memory which could cause unforeseen
    // issues
    if (index >= cache->blocks)
    {
        printf("Illegal access! Index: %d, max: %d\n", index, cache->blocks);
        exit(1);
        return;
    }

    statistics->accesses++;
    cache->statistics.accesses++;

    cache_line_t line = cache->lines[index];

    if (line.valid && line.tag == tag)
    {
        statistics->hits++;
        cache->statistics.hits++;
        return;
    }

    // Line is not present in cache. Insert it.
    line.valid = true;
    line.tag = tag;
    line.inserted_at = statistics->accesses;

    cache->lines[index] = line;
}

/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE *ptr_file)
{
    char buf[1000];
    char *token;
    char *string = buf;
    mem_access_t access;

    if (fgets(buf, 1000, ptr_file) != NULL)
    {

        /* Get the access type */
        token = strsep(&string, " \n");
        if (strcmp(token, "I") == 0)
        {
            access.accesstype = instruction;
        }
        else if (strcmp(token, "D") == 0)
        {
            access.accesstype = data;
        }
        else
        {
            printf("Unkown access type\n");
            exit(0);
        }

        /* Get the access type */
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtol(token, NULL, 16);

        return access;
    }

    /* If there are no more entries in the file,  
     * return an address 0 that will terminate the infinite loop in main
     */
    access.address = 0;
    return access;
}

void main(int argc, char **argv)
{
    // DECLARE CACHES AND COUNTERS FOR THE STATS HERE

    uint32_t cache_size;
    uint32_t block_size = 64;
    cache_map_t cache_mapping;
    cache_org_t cache_org;

    // USE THIS FOR YOUR CACHE STATISTICS
    cache_stat_t cache_statistics;

    // Reset statistics:
    memset(&cache_statistics, 0, sizeof(cache_stat_t));

    /* Read command-line parameters and initialize:
     * cache_size, cache_mapping and cache_org variables
     */

    if (argc != 4)
    { /* argc should be 2 for correct execution */
        printf("Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] [cache organization: uc|sc]\n");
        exit(0);
    }
    else
    {
        /* argv[0] is program name, parameters start with argv[1] */

        /* Set cache size */
        cache_size = atoi(argv[1]);

        /* Set Cache Mapping */
        if (strcmp(argv[2], "dm") == 0)
        {
            cache_mapping = dm;
        }
        else if (strcmp(argv[2], "fa") == 0)
        {
            cache_mapping = fa;
        }
        else
        {
            printf("Unknown cache mapping\n");
            exit(0);
        }

        /* Set Cache Organization */
        if (strcmp(argv[3], "uc") == 0)
        {
            cache_org = uc;
        }
        else if (strcmp(argv[3], "sc") == 0)
        {
            cache_org = sc;
        }
        else
        {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }

    // Make caches
    cache_total_t *cache = make_total_cache(cache_size, block_size, cache_mapping, cache_org);

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen("mem_trace.txt", "r");
    if (!ptr_file)
    {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    /* Loop until whole trace file has been read */
    mem_access_t access;
    while (1)
    {
        access = read_transaction(ptr_file);
        //If no transactions left, break out of loop
        if (access.address == 0)
            break;
        //printf("%d %x\n", access.accesstype, access.address);

        /* Do a cache access */

        // If this is a unified cache these will point to the same cache
        cache_t *target = (access.accesstype == instruction) ? cache->instructions : cache->data;

        if (cache_mapping == dm)
        {
            access_mem_dm(target, &cache_statistics, access);
        }
        else
        {
            access_mem_fa(target, &cache_statistics, access);
        }
    }

    /* Print the statistics */
    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n", (double)cache_statistics.hits / cache_statistics.accesses);
    // You can extend the memory statistic printing if you like!

    if (cache_org == sc)
    {
        printf("\n");
        printf("DCache Accesses: %ld\n", cache->data->statistics.accesses);
        printf("DCache Hits:     %ld\n", cache->data->statistics.hits);
        printf("DCache Hit Rate: %.4f\n", (double)cache->data->statistics.hits / cache->data->statistics.accesses);
        printf("\n");
        printf("ICache Accesses: %ld\n", cache->instructions->statistics.accesses);
        printf("ICache Hits:     %ld\n", cache->instructions->statistics.hits);
        printf("ICache Hit Rate: %.4f\n", (double)cache->instructions->statistics.hits / cache->instructions->statistics.accesses);
    }

    /* Close the trace file */
    fclose(ptr_file);

    free(cache);
    free(cache->data);

    if (cache_org == sc)
    {
        free(cache->instructions);
    }
}
