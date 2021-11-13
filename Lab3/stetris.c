#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>

// The game state can be used to detect what happens on the playfield
#define GAMEOVER 0
#define ACTIVE (1 << 0)
#define ROW_CLEAR (1 << 1)
#define TILE_ADDED (1 << 2)

#define SENSEHAT_FB_NAME "RPi-Sense FB"
#define SENSEHAT_JS_NAME "Raspberry Pi Sense HAT Joystick"
// Converts RGB888 to RGB565 by grabbing most significant bits of each color
#define RGB(r, g, b) (((r & 0xF8) << 8) + ((g & 0xFC) << 3) + (b >> 3))

// If you extend this structure, either avoid pointers or adjust
// the game logic allocate/deallocate and reset the memory
typedef struct
{
    bool occupied;
    u_int16_t color; // This is the color value (raw RGB565) for this tile
} tile;

typedef struct
{
    unsigned int x;
    unsigned int y;
} coord;

typedef struct
{
    coord const grid;                     // playfield bounds
    unsigned long const uSecTickTime;     // tick rate
    unsigned long const rowsPerLevel;     // speed up after clearing rows
    unsigned long const initNextGameTick; // initial value of nextGameTick

    unsigned int tiles;      // number of tiles played
    unsigned int rows;       // number of rows cleared
    unsigned int score;      // game score
    unsigned int level;      // game level
    unsigned int colorIndex; // The current index of the color to use

    tile *rawPlayfield; // pointer to raw memory of the playfield
    tile **playfield;   // This is the play field array
    unsigned int state;
    coord activeTile; // current tile

    unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                                // when reached 0, next game state calculated
    unsigned long nextGameTick; // sets when tick is wrapping back to zero
                                // lowers with increasing level, never reaches 0
} gameConfig;

// Pointers and information for controlling the Sensehat LED matrix
typedef struct
{
    int filedesc;
    u_int16_t *pixels;
    u_int32_t screen_bytes;
    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo var_info;
} sensehat_ctl_t;

// Information needed to read input from the Sensehat joystick
typedef struct
{
    int filedesc;
} sensehat_joystick_t;

gameConfig game = {
    .grid = {8, 8},
    .uSecTickTime = 10000,
    .rowsPerLevel = 2,
    .initNextGameTick = 50,
};

sensehat_ctl_t sensehat_ctl;
sensehat_joystick_t sensehat_joystick;

// The default tetris color scheme. Fetched from
// https://www.schemecolor.com/tetris-game-color-scheme.php
u_int16_t COLORS[] = {RGB(3, 65, 174), RGB(114, 203, 59), RGB(255, 213, 0), RGB(255, 151, 28), RGB(255, 50, 19)};
u_int16_t COLOR_COUNT = sizeof(COLORS) / sizeof(u_int16_t);

// Converts game x, y coordinates to the correct memory offset to
// display on the sensehat LED display
u_int32_t getLocation(u_int8_t x, u_int8_t y)
{
    // Inverted x and y coordinates so that it is natural to play using
    // the joystick with the right thumb. This makes it so the tiles
    // fall towards the side with the power port.
    return y + sensehat_ctl.var_info.xoffset + (x + sensehat_ctl.var_info.yoffset) * sensehat_ctl.var_info.xres;
}

// Clears (Turns off) the entire LED display matrix
void clearPixels()
{
    memset(sensehat_ctl.pixels, 0, sensehat_ctl.screen_bytes);
}

// Finds and initializes the sensehat joystick if it is present on the system
bool initializeSenseHatJoystick()
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir("/dev/input");

    // Unable to open directory
    if (!dir)
    {
        return false;
    }

    char file_path[64];
    char name[512];

    printf("Scanning /dev/input for sensehat joystick...\n");
    while ((entry = readdir(dir)) != NULL)
    {
        strcpy(file_path, "/dev/input/");
        strcat(file_path, (char *)&entry->d_name);

        printf("Opening file descriptor of %s\n", file_path);
        int filedesc = open(file_path, O_RDONLY | O_NONBLOCK); // Only need read access here
        if (filedesc == -1)
        {
            printf("Error occurred while opening file descriptor %s\n", file_path);
            continue;
        }

        // Will return length of string, or less than 0 if an error occurs
        if (ioctl(filedesc, EVIOCGNAME(sizeof(name)), &name) < 0)
        {
            printf("Failed to read name of input device %s\n", file_path);
            continue;
        }

        printf("Device %s found\n", name);
        if (strcmp(name, SENSEHAT_JS_NAME) != 0)
        {
            continue;
        }

        sensehat_joystick.filedesc = filedesc;
        printf("Found sensehat joystick!\n");
        break;
    }

    closedir(dir);
    return sensehat_joystick.filedesc != 0;
}

bool initializeSenseHatLED()
{
    // Local variables here so that we don't change the global state
    // until we are certain we have the right framebuffer
    struct fb_fix_screeninfo fixed_info;
    struct fb_var_screeninfo var_info;

    DIR *dir;
    struct dirent *entry;

    dir = opendir("/dev");

    // Unable to open directory
    if (!dir)
    {
        return false;
    }

    printf("Scanning /dev for sensehat...\n");
    while ((entry = readdir(dir)) != NULL)
    {
        // Check if directory entry name starts with fb
        if (strncmp(entry->d_name, "fb", 2))
        {
            continue;
        }

        printf("Found framebuffer %s!\n", entry->d_name);

        char file_path[8];
        strcpy(file_path, "/dev/");
        strcat(file_path, (char *)&entry->d_name);

        printf("Opening file descriptor of %s\n", file_path);
        int filedesc = open(file_path, O_RDWR);
        if (filedesc == -1)
        {
            printf("Error occurred while opening file descriptor %s\n", file_path);
            continue;
        }

        // Attempted to retrieve fixed screen info
        if (ioctl(filedesc, FBIOGET_FSCREENINFO, &fixed_info) != 0)
        {
            // If an error occurs, will not return 0
            printf("Unable to load fixed screen info!\n");
            continue;
        }

        printf("ID is %s!\n", fixed_info.id);
        if (strcmp(fixed_info.id, SENSEHAT_FB_NAME) != 0)
        {
            continue;
        }

        printf("Found sensehat at %s\n", file_path);
        if (ioctl(filedesc, FBIOGET_VSCREENINFO, &var_info) != 0)
        {
            printf("Unable to load variable screen info!\n");
            return false;
        }

        // We know that the amount of bits will be 16, but this wont hurt anything
        long screen_bytes = var_info.xres * var_info.yres * (var_info.bits_per_pixel >> 3);

        u_int16_t *sh_mem = (u_int16_t *)mmap(0, screen_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, filedesc, 0);
        if (sh_mem == -1)
        {
            printf("An error occurred while mapping framebuffer to memory");
            return false;
        }

        if (game.grid.x > var_info.xres || game.grid.y > var_info.yres)
        {
            printf("Grid is too large for sensehat display! Grid is %u by %u, only support for %u by %u ",
                   game.grid.x, game.grid.y, var_info.xres, var_info.yres);
            return false;
        }

        // Update variables so that they are accessible elsewhere
        sensehat_ctl.filedesc = filedesc;
        sensehat_ctl.pixels = sh_mem;
        sensehat_ctl.fix_info = fixed_info;
        sensehat_ctl.var_info = var_info;
        sensehat_ctl.screen_bytes = screen_bytes;

        // Start with a fresh "screen"
        clearPixels();

        printf("Successfully loaded sensehat display!\n");
        break;
    }

    closedir(dir);
    return sensehat_ctl.pixels != 0;
}

// This function is called on the start of your application
// Here you can initialize what ever you need for your task
// return false if something fails, else true
bool initializeSenseHat()
{
    return initializeSenseHatLED() && initializeSenseHatJoystick();
}

// This function is called when the application exits
// Here you can free up everything that you might have opened/allocated
void freeSenseHat()
{
    clearPixels();
    munmap(sensehat_ctl.pixels, sensehat_ctl.screen_bytes);
    close(sensehat_ctl.filedesc);
    close(sensehat_joystick.filedesc);
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed
// !!! when nothing was pressed you MUST return 0 !!!
int readSenseHatJoystick()
{
    // Shouldn't happen, but make sure here aswell
    if (sensehat_joystick.filedesc == 0)
    {
        return 0;
    }

    struct input_event event;

    // Read in a while loop since there may be events we don't care about, and we don't
    // want those to block a legit read
    while (true)
    {
        ssize_t read_bytes = read(sensehat_joystick.filedesc, &event, sizeof(struct input_event));
        // No more events to read
        if (read_bytes <= 0)
        {
            break;
        }

        // Check if we are looking at the right event type, and that this event doesn't
        // indicate that the key has been released. Event code 1 and 2 are what we want,
        // which correspond to pressed and continuation. 0 is release.
        if (event.type != EV_KEY || event.value == 0)
        {
            continue;
        }

        // We can return directly here because they joystick physically does not allow
        // more than one button to be pressed at once. event.code corresponds correctly
        // to our key mappings due to the rotation of our screen/LED matrix.
        return event.code;
    }

    // No events found, meaning nothing is pressed
    return 0;
}

// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfield
void renderSenseHatMatrix(bool const playfieldChanged)
{
    // No need to update
    if (!playfieldChanged)
    {
        return;
    }

    for (u_int32_t x = 0; x < game.grid.x; x++)
    {
        for (u_int32_t y = 0; y < game.grid.y; y++)
        {
            tile current = game.playfield[x][y];
            // Here abusing the fact that color will be 0 (i. e. off) for any unoccupied
            // tile. This avoids a clearPixels() call and an if-statement within the loop
            sensehat_ctl.pixels[getLocation(x, y)] = current.color;
        }
    }
}

// The game logic uses only the following functions to interact with the playfield.
// if you choose to change the playfield or the tile structure, you might need to
// adjust this game logic <> playfield interface
static inline void newTile(coord const target)
{
    game.playfield[target.y][target.x].occupied = true;
    game.playfield[target.y][target.x].color = COLORS[game.colorIndex];

    // Increment to next color
    game.colorIndex++;
    game.colorIndex %= COLOR_COUNT;
}

static inline void copyTile(coord const to, coord const from)
{
    memcpy((void *)&game.playfield[to.y][to.x], (void *)&game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from)
{
    memcpy((void *)&game.playfield[to][0], (void *)&game.playfield[from][0], sizeof(tile) * game.grid.x);
}

static inline void resetTile(coord const target)
{
    memset((void *)&game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target)
{
    memset((void *)&game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}

static inline bool tileOccupied(coord const target)
{
    return game.playfield[target.y][target.x].occupied;
}

static inline bool rowOccupied(unsigned int const target)
{
    for (unsigned int x = 0; x < game.grid.x; x++)
    {
        coord const checkTile = {x, target};
        if (!tileOccupied(checkTile))
        {
            return false;
        }
    }
    return true;
}

static inline void resetPlayfield()
{
    for (unsigned int y = 0; y < game.grid.y; y++)
    {
        resetRow(y);
    }
}

// Below here comes the game logic. Keep in mind: You are not allowed to change how the game works!
// that means no changes are necessary below this line! And if you choose to change something
// keep it compatible with what was provided to you!

bool addNewTile()
{
    game.activeTile.y = 0;
    game.activeTile.x = (game.grid.x - 1) / 2;
    if (tileOccupied(game.activeTile))
        return false;
    newTile(game.activeTile);
    return true;
}

bool moveRight()
{
    coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
    if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile))
    {
        copyTile(newTile, game.activeTile);
        resetTile(game.activeTile);
        game.activeTile = newTile;
        return true;
    }
    return false;
}

bool moveLeft()
{
    coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
    if (game.activeTile.x > 0 && !tileOccupied(newTile))
    {
        copyTile(newTile, game.activeTile);
        resetTile(game.activeTile);
        game.activeTile = newTile;
        return true;
    }
    return false;
}

bool moveDown()
{
    coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
    if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile))
    {
        copyTile(newTile, game.activeTile);
        resetTile(game.activeTile);
        game.activeTile = newTile;
        return true;
    }
    return false;
}

bool clearRow()
{
    if (rowOccupied(game.grid.y - 1))
    {
        for (unsigned int y = game.grid.y - 1; y > 0; y--)
        {
            copyRow(y, y - 1);
        }
        resetRow(0);
        return true;
    }
    return false;
}

void advanceLevel()
{
    game.level++;
    switch (game.nextGameTick)
    {
    case 1:
        break;
    case 2 ... 10:
        game.nextGameTick--;
        break;
    case 11 ... 20:
        game.nextGameTick -= 2;
        break;
    default:
        game.nextGameTick -= 10;
    }
}

void newGame()
{
    game.state = ACTIVE;
    game.tiles = 0;
    game.rows = 0;
    game.score = 0;
    game.tick = 0;
    game.level = 0;
    resetPlayfield();
}

void gameOver()
{
    game.state = GAMEOVER;
    game.nextGameTick = game.initNextGameTick;
}

bool sTetris(int const key)
{
    bool playfieldChanged = false;

    if (game.state & ACTIVE)
    {
        // Move the current tile
        if (key)
        {
            playfieldChanged = true;
            switch (key)
            {
            case KEY_LEFT:
                moveLeft();
                break;
            case KEY_RIGHT:
                moveRight();
                break;
            case KEY_DOWN:
                while (moveDown())
                {
                };
                game.tick = 0;
                break;
            default:
                playfieldChanged = false;
            }
        }

        // If we have reached a tick to update the game
        if (game.tick == 0)
        {
            // We communicate the row clear and tile add over the game state
            // clear these bits if they were set before
            game.state &= ~(ROW_CLEAR | TILE_ADDED);

            playfieldChanged = true;
            // Clear row if possible
            if (clearRow())
            {
                game.state |= ROW_CLEAR;
                game.rows++;
                game.score += game.level + 1;
                if ((game.rows % game.rowsPerLevel) == 0)
                {
                    advanceLevel();
                }
            }

            // if there is no current tile or we cannot move it down,
            // add a new one. If not possible, game over.
            if (!tileOccupied(game.activeTile) || !moveDown())
            {
                if (addNewTile())
                {
                    game.state |= TILE_ADDED;
                    game.tiles++;
                }
                else
                {
                    gameOver();
                }
            }
        }
    }

    // Press any key to start a new game
    if ((game.state == GAMEOVER) && key)
    {
        playfieldChanged = true;
        newGame();
        addNewTile();
        game.state |= TILE_ADDED;
        game.tiles++;
    }

    return playfieldChanged;
}

int readKeyboard()
{
    struct pollfd pollStdin = {
        .fd = STDIN_FILENO,
        .events = POLLIN};
    int lkey = 0;

    if (poll(&pollStdin, 1, 0))
    {
        lkey = fgetc(stdin);
        if (lkey != 27)
            goto exit;
        lkey = fgetc(stdin);
        if (lkey != 91)
            goto exit;
        lkey = fgetc(stdin);
    }
exit:
    switch (lkey)
    {
    case 10:
        return KEY_ENTER;
    case 65:
        return KEY_UP;
    case 66:
        return KEY_DOWN;
    case 67:
        return KEY_RIGHT;
    case 68:
        return KEY_LEFT;
    }
    return 0;
}

void renderConsole(bool const playfieldChanged)
{
    if (!playfieldChanged)
        return;

    // Goto beginning of console
    fprintf(stdout, "\033[%d;%dH", 0, 0);
    for (unsigned int x = 0; x < game.grid.x + 2; x++)
    {
        fprintf(stdout, "-");
    }
    fprintf(stdout, "\n");
    for (unsigned int y = 0; y < game.grid.y; y++)
    {
        fprintf(stdout, "|");
        for (unsigned int x = 0; x < game.grid.x; x++)
        {
            coord const checkTile = {x, y};
            fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
        }
        switch (y)
        {
        case 0:
            fprintf(stdout, "| Tiles: %10u\n", game.tiles);
            break;
        case 1:
            fprintf(stdout, "| Rows:  %10u\n", game.rows);
            break;
        case 2:
            fprintf(stdout, "| Score: %10u\n", game.score);
            break;
        case 4:
            fprintf(stdout, "| Level: %10u\n", game.level);
            break;
        case 7:
            fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
            break;
        default:
            fprintf(stdout, "|\n");
        }
    }
    for (unsigned int x = 0; x < game.grid.x + 2; x++)
    {
        fprintf(stdout, "-");
    }
    fflush(stdout);
}

inline unsigned long uSecFromTimespec(struct timespec const ts)
{
    return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // This sets the stdin in a special state where each
    // keyboard press is directly flushed to the stdin and additionally
    // not outputted to the stdout
    {
        struct termios ttystate;
        tcgetattr(STDIN_FILENO, &ttystate);
        ttystate.c_lflag &= ~(ICANON | ECHO);
        ttystate.c_cc[VMIN] = 1;
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    }

    // Allocate the playing field structure
    game.rawPlayfield = (tile *)malloc(game.grid.x * game.grid.y * sizeof(tile));
    game.playfield = (tile **)malloc(game.grid.y * sizeof(tile *));
    if (!game.playfield || !game.rawPlayfield)
    {
        fprintf(stderr, "ERROR: could not allocate playfield\n");
        return 1;
    }
    for (unsigned int y = 0; y < game.grid.y; y++)
    {
        game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
    }

    // Reset playfield to make it empty
    resetPlayfield();
    // Start with gameOver
    gameOver();

    if (!initializeSenseHat())
    {
        fprintf(stderr, "ERROR: could not initilize sense hat\n");
        return 1;
    };

    // Clear console, render first time
    fprintf(stdout, "\033[H\033[J");
    renderConsole(true);
    renderSenseHatMatrix(true);

    while (true)
    {
        struct timeval sTv, eTv;
        gettimeofday(&sTv, NULL);

        int key = readSenseHatJoystick();
        if (!key)
            key = readKeyboard();
        if (key == KEY_ENTER)
            break;

        bool playfieldChanged = sTetris(key);
        renderConsole(playfieldChanged);
        renderSenseHatMatrix(playfieldChanged);

        // Wait for next tick
        gettimeofday(&eTv, NULL);
        unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
        if (uSecProcessTime < game.uSecTickTime)
        {
            usleep(game.uSecTickTime - uSecProcessTime);
        }
        game.tick = (game.tick + 1) % game.nextGameTick;
    }

    freeSenseHat();
    free(game.playfield);
    free(game.rawPlayfield);

    return 0;
}
