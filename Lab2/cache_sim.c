#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>

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

typedef struct
{
    uint32_t blocks;
    uint32_t bits_offset;
    uint32_t bits_index;
    uint32_t bits_tag;
    cache_line_t lines[];
} cache_t;

typedef struct
{
    cache_t *instructions;
    cache_t *data;
} cache_total_t;

uint32_t get_tag(cache_t cache, mem_access_t access)
{
    return access.address & ~(0xFFFFFFFF >> cache.bits_tag);
}

cache_t *make_cache(uint32_t size, uint32_t block_size, cache_map_t map)
{
    uint32_t blocks = size / block_size;
    if (blocks == 0) {
        printf("Invalid cache size %d! Needs to be larger than %d", size, block_size);
        exit(1);
    }

    size_t mem_size = sizeof(cache_t) + sizeof(cache_line_t) * blocks;
    cache_t *cache = malloc(mem_size);

    cache->blocks = blocks;
    cache->bits_offset = BIT_WIDTH(block_size);
    cache->bits_index = (map == dm) ? BIT_WIDTH(cache->blocks) : 0;
    cache->bits_tag = 32 - cache->bits_offset - cache->bits_index;

    return cache;
}

cache_total_t *make_total_cache(uint32_t size, uint32_t block_size, cache_map_t map, cache_org_t org)
{
    cache_total_t *cache = malloc(sizeof(cache_total_t));

    if (org == sc)
    {
        size >>= 1; // Divide by 2

        cache->data = make_cache(size, block_size, map);
        cache->instructions = make_cache(size, block_size, map);
    }
    else
    {
        cache_t *unified = make_cache(size, block_size, map);
        cache->data = unified;
        cache->instructions = unified;
    }

    return cache;
}

void insert_mem_fa(cache_t *cache, cache_stat_t statistics, mem_access_t access)
{
    cache_line_t line;
    uint32_t index;
    for (uint32_t i = 0; i < cache->blocks; i++)
    {
        cache_line_t curr = cache->lines[i];
        if (!curr.valid)
        {
            index = i;
            line = curr;
            break;
        }

        if (!&line || line.inserted_at > curr.inserted_at)
        {
            index = i;
            line = curr;
        }
    }

    line.tag = get_tag(*cache, access);
    // Only keep track of when inserted since we are implementing a FIFO queue
    line.inserted_at = statistics.accesses;
    line.valid = true;

    cache->lines[index] = line;
}

void access_mem_fa(cache_t *cache, cache_stat_t *statistics, mem_access_t access)
{
    uint32_t tag = get_tag(*cache, access);

    statistics->accesses++;

    for (uint32_t i = 0; i < cache->blocks; i++)
    {
        cache_line_t line = cache->lines[i];
        if (!line.valid)
        {
            continue;
        }

        if (line.tag != tag)
        {
            continue;
        }

        statistics->hits++;
        return;
    }

    insert_mem_fa(cache, *statistics, access);
}

uint32_t get_index(cache_t cache, mem_access_t access)
{
    uint32_t mask = ~(0xFFFFFFFF << cache.bits_index) << cache.bits_offset;
    return (access.address & mask) >> cache.bits_offset;
}

void access_mem_dm(cache_t *cache, cache_stat_t *statistics, mem_access_t access)
{
    uint32_t tag = get_tag(*cache, access);
    uint32_t index = get_index(*cache, access);
    if (index >= cache->blocks)
    {
        printf("Illegal access! Index: %d, max: %d\n", index, cache->blocks);
        exit(1);
        return;
    }

    statistics->accesses++;

    cache_line_t line = cache->lines[index];

    if (line.valid && line.tag == tag)
    {
        statistics->hits++;
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
        printf("%d %x\n", access.accesstype, access.address);

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

    /* Close the trace file */
    fclose(ptr_file);

    free(cache);
    free(cache->data);

    if (cache_org == sc)
    {
        free(cache->instructions);
    }
}
