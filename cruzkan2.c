#include <dos.h>
#include <conio.h>
#include <math.h>

#define SCREEN_W 320
#define SCREEN_H 200
#define HALF_H 100
#define NUM_RAYS 160
#define COLUMN_W 2
#define TEX_W 32
#define TEX_H 32
#define TEX_SIZE (TEX_W * TEX_H)
#define NUM_TEXTURES 4

#define MAP_W 16
#define MAP_H 16

#define MOVE_SPEED 0.10
#define STRAFE_SPEED 0.08
#define ROT_SPEED 0.08
#define RAY_EPSILON 0.000001

#define SC_ESC 1
#define SC_W 17
#define SC_Q 16
#define SC_E 18
#define SC_S 31
#define SC_A 30
#define SC_D 32
#define SC_LEFT 75
#define SC_RIGHT 77
#define SC_UP 72
#define SC_DOWN 80

#define COL_SKY 20
#define COL_FLOOR 21
#define COL_TEXTURE_BASE 22

#define SHADE_DARK 0
#define SHADE_BASE 1
#define SHADE_LIGHT 2

unsigned char far *VGA = (unsigned char far *)MK_FP(0xA000, 0);
volatile unsigned char keys[128];
void interrupt (*old_key_handler)();
unsigned char wall_textures[NUM_TEXTURES][TEX_SIZE];

double posX = 3.5;
double posY = 3.5;
double dirX = 1.0;
double dirY = 0.0;
double planeX = 0.0;
double planeY = 0.66;

char world[MAP_H][MAP_W + 1] = {
    "1111111111111111",
    "1000000000000001",
    "1022220000333301",
    "1000020000000301",
    "1000020011110301",
    "1000000010000001",
    "1004440010000001",
    "1004000010002201",
    "1004000010002001",
    "1000000010002001",
    "1011111110002001",
    "1000000000002001",
    "1000333333002001",
    "1000000000000001",
    "1000000000000001",
    "1111111111111111"
};

void set_video_mode(unsigned char mode);
void set_palette(unsigned char index, unsigned char r, unsigned char g, unsigned char b);
void init_palette(void);
void wait_retrace(void);
void install_keyboard(void);
void remove_keyboard(void);
void interrupt keyboard_handler(void);
int is_wall(int x, int y);
int texture_index(char tile);
void init_textures(void);
void draw_column(int x, int draw_start, int draw_end, int line_height,
                 char tile, int side, int tex_x);
void rotate_player(double angle);
void try_move(double dx, double dy);
void update_player(void);
void render_frame(void);

void set_video_mode(unsigned char mode)
{
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = mode;
    int86(0x10, &regs, &regs);
}

void set_palette(unsigned char index, unsigned char r, unsigned char g, unsigned char b)
{
    outp(0x3C8, index);
    outp(0x3C9, r);
    outp(0x3C9, g);
    outp(0x3C9, b);
}

void init_palette(void)
{
    set_palette(COL_SKY, 10, 18, 30);
    set_palette(COL_FLOOR, 12, 10, 10);

    set_palette(22, 20, 3, 3);
    set_palette(23, 40, 8, 8);
    set_palette(24, 55, 16, 12);
    set_palette(25, 10, 2, 2);
    set_palette(26, 22, 4, 4);
    set_palette(27, 32, 8, 6);

    set_palette(28, 16, 16, 18);
    set_palette(29, 34, 34, 38);
    set_palette(30, 48, 48, 52);
    set_palette(31, 8, 8, 10);
    set_palette(32, 18, 18, 22);
    set_palette(33, 28, 28, 32);

    set_palette(34, 4, 9, 20);
    set_palette(35, 10, 18, 42);
    set_palette(36, 20, 34, 55);
    set_palette(37, 2, 4, 11);
    set_palette(38, 5, 8, 24);
    set_palette(39, 10, 17, 32);

    set_palette(40, 22, 14, 4);
    set_palette(41, 42, 32, 10);
    set_palette(42, 58, 48, 20);
    set_palette(43, 12, 7, 2);
    set_palette(44, 24, 18, 5);
    set_palette(45, 34, 26, 9);
}

void wait_retrace(void)
{
    while (inp(0x3DA) & 8) {
    }
    while (!(inp(0x3DA) & 8)) {
    }
}

void interrupt keyboard_handler(void)
{
    unsigned char scan;
    unsigned char ack;

    scan = inp(0x60);

    if (scan != 0xE0) {
        if (scan & 0x80) {
            keys[scan & 0x7F] = 0;
        } else {
            keys[scan] = 1;
        }
    }

    ack = inp(0x61);
    outp(0x61, ack | 0x80);
    outp(0x61, ack);
    outp(0x20, 0x20);
}

void install_keyboard(void)
{
    int i;
    for (i = 0; i < 128; ++i) {
        keys[i] = 0;
    }
    old_key_handler = getvect(9);
    setvect(9, keyboard_handler);
}

void remove_keyboard(void)
{
    setvect(9, old_key_handler);
}

int is_wall(int x, int y)
{
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) {
        return 1;
    }
    return world[y][x] != '0';
}

int texture_index(char tile)
{
    switch (tile) {
        case '1':
            return 0;
        case '2':
            return 1;
        case '3':
            return 2;
        case '4':
            return 3;
    }
    return 1;
}

void init_textures(void)
{
    int x;
    int y;
    int offset;
    int local_x;
    int local_y;
    int shade;

    for (y = 0; y < TEX_H; ++y) {
        for (x = 0; x < TEX_W; ++x) {
            offset = y * TEX_W + x;

            local_x = (x + ((y / 8) & 1) * 8) & 15;
            local_y = y & 7;
            if (local_x == 0 || local_y == 0) {
                shade = SHADE_DARK;
            } else if (local_y == 1) {
                shade = SHADE_LIGHT;
            } else {
                shade = SHADE_BASE;
            }
            wall_textures[0][offset] = (unsigned char)shade;

            local_x = (x + ((y / 8) & 1) * 5) & 15;
            local_y = y & 7;
            if (local_x == 0 || local_y == 0) {
                shade = SHADE_DARK;
            } else if (((x * 3 + y * 5 + (x ^ y)) & 15) < 3) {
                shade = SHADE_LIGHT;
            } else {
                shade = SHADE_BASE;
            }
            wall_textures[1][offset] = (unsigned char)shade;

            local_x = x & 15;
            local_y = y & 15;
            if (local_x == 0 || local_y == 0) {
                shade = SHADE_DARK;
            } else if ((local_x == 2 || local_x == 13) &&
                       (local_y == 2 || local_y == 13)) {
                shade = SHADE_LIGHT;
            } else if (local_x == 1 || local_y == 1) {
                shade = SHADE_LIGHT;
            } else {
                shade = SHADE_BASE;
            }
            wall_textures[2][offset] = (unsigned char)shade;

            local_x = (x + ((y / 8) & 1) * 8) & 15;
            local_y = y & 7;
            if (local_x == 0 || local_y == 0) {
                shade = SHADE_DARK;
            } else if (local_x == 1 || local_y == 1) {
                shade = SHADE_LIGHT;
            } else {
                shade = SHADE_BASE;
            }
            wall_textures[3][offset] = (unsigned char)shade;
        }
    }
}

void draw_column(int x, int draw_start, int draw_end, int line_height,
                 char tile, int side, int tex_x)
{
    int y;
    int texture;
    int tex_y;
    int wall_start;
    int wall_end;
    unsigned char shade;
    unsigned char color;
    unsigned int offset;
    long tex_step;
    long tex_pos;

    if (line_height < 1) {
        line_height = 1;
    }

    wall_start = draw_start;
    wall_end = draw_end;
    if (wall_start < 0) {
        wall_start = 0;
    }
    if (wall_end >= SCREEN_H) {
        wall_end = SCREEN_H - 1;
    }

    for (y = 0; y < wall_start; ++y) {
        offset = (y << 8) + (y << 6) + x;
        VGA[offset] = COL_SKY;
        VGA[offset + 1] = COL_SKY;
    }

    texture = texture_index(tile);
    tex_step = ((long)TEX_H << 16) / line_height;
    tex_pos = (long)(wall_start - draw_start) * tex_step;

    for (y = wall_start; y <= wall_end; ++y) {
        tex_y = (int)(tex_pos >> 16) & (TEX_H - 1);
        shade = wall_textures[texture][tex_y * TEX_W + tex_x];
        color = (unsigned char)(COL_TEXTURE_BASE + texture * 6 +
                                side * 3 + shade);
        offset = (y << 8) + (y << 6) + x;
        VGA[offset] = color;
        VGA[offset + 1] = color;
        tex_pos += tex_step;
    }

    for (y = wall_end + 1; y < SCREEN_H; ++y) {
        offset = (y << 8) + (y << 6) + x;
        VGA[offset] = COL_FLOOR;
        VGA[offset + 1] = COL_FLOOR;
    }
}

void rotate_player(double angle)
{
    double oldDirX;
    double oldPlaneX;
    double ca;
    double sa;

    ca = cos(angle);
    sa = sin(angle);

    oldDirX = dirX;
    dirX = dirX * ca - dirY * sa;
    dirY = oldDirX * sa + dirY * ca;

    oldPlaneX = planeX;
    planeX = planeX * ca - planeY * sa;
    planeY = oldPlaneX * sa + planeY * ca;
}

void try_move(double dx, double dy)
{
    double nx;
    double ny;

    nx = posX + dx;
    ny = posY + dy;

    if (!is_wall((int)nx, (int)posY)) {
        posX = nx;
    }
    if (!is_wall((int)posX, (int)ny)) {
        posY = ny;
    }
}

void update_player(void)
{
    if (keys[SC_W] || keys[SC_UP]) {
        try_move(dirX * MOVE_SPEED, dirY * MOVE_SPEED);
    }
    if (keys[SC_S] || keys[SC_DOWN]) {
        try_move(-dirX * MOVE_SPEED, -dirY * MOVE_SPEED);
    }
    if (keys[SC_D]) {
        try_move(-dirY * STRAFE_SPEED, dirX * STRAFE_SPEED);
    }
    if (keys[SC_A]) {
        try_move(dirY * STRAFE_SPEED, -dirX * STRAFE_SPEED);
    }
    if (keys[SC_LEFT]) {
        rotate_player(-ROT_SPEED);
    }
    if (keys[SC_RIGHT]) {
        rotate_player(ROT_SPEED);
    }
}

void render_frame(void)
{
    int col;

    for (col = 0; col < NUM_RAYS; ++col) {
        double cameraX;
        double rayDirX;
        double rayDirY;
        int mapX;
        int mapY;
        double sideDistX;
        double sideDistY;
        double deltaDistX;
        double deltaDistY;
        double perpWallDist;
        double wallX;
        int stepX;
        int stepY;
        int hit;
        int side;
        int lineHeight;
        int drawStart;
        int drawEnd;
        int texX;
        char tile;
        int screenX;

        cameraX = (2.0 * col) / (double)NUM_RAYS - 1.0;
        rayDirX = dirX + planeX * cameraX;
        rayDirY = dirY + planeY * cameraX;

        mapX = (int)posX;
        mapY = (int)posY;

        if (fabs(rayDirX) < RAY_EPSILON) {
            deltaDistX = 1.0e30;
        } else {
            deltaDistX = fabs(1.0 / rayDirX);
        }

        if (fabs(rayDirY) < RAY_EPSILON) {
            deltaDistY = 1.0e30;
        } else {
            deltaDistY = fabs(1.0 / rayDirY);
        }

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - posX) * deltaDistX;
        }

        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - posY) * deltaDistY;
        }

        hit = 0;
        side = 0;

        while (!hit) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (is_wall(mapX, mapY)) {
                hit = 1;
            }
        }

        if (side == 0) {
            perpWallDist = sideDistX - deltaDistX;
        } else {
            perpWallDist = sideDistY - deltaDistY;
        }

        if (perpWallDist < 0.1) {
            perpWallDist = 0.1;
        }

        lineHeight = (int)(SCREEN_H / perpWallDist);
        if (lineHeight < 1) {
            lineHeight = 1;
        }
        drawStart = HALF_H - lineHeight / 2;
        drawEnd = drawStart + lineHeight - 1;

        if (side == 0) {
            wallX = posY + perpWallDist * rayDirY;
        } else {
            wallX = posX + perpWallDist * rayDirX;
        }
        wallX -= floor(wallX);
        texX = (int)(wallX * TEX_W);
        if (texX < 0) {
            texX = 0;
        } else if (texX >= TEX_W) {
            texX = TEX_W - 1;
        }

        if ((side == 0 && rayDirX > 0) ||
            (side == 1 && rayDirY < 0)) {
            texX = TEX_W - texX - 1;
        }

        tile = world[mapY][mapX];
        screenX = col * COLUMN_W;

        draw_column(screenX, drawStart, drawEnd, lineHeight,
                    tile, side, texX);
    }
}

int main(void)
{
    clrscr();
    cprintf("Cruzkan2 demo for Turbo C\r\n");
    cprintf("--------------------------------\r\n\r\n");
    cprintf("W/S : move forward/back\r\n");
    cprintf("A/D : strafe left/right\r\n");
    cprintf("Up/Down/Left/Right : move forward/back turn left/right\r\n");
    cprintf("ESC : quit\r\n\r\n");
    cprintf("Press any key to start...");
    getch();

    install_keyboard();
    set_video_mode(0x13);
    init_palette();
    init_textures();

    while (!keys[SC_ESC]) {
        update_player();
        render_frame();
        wait_retrace();
    }

    set_video_mode(0x03);
    remove_keyboard();

    cprintf("Thanks for playing!\r\n");
    return 0;
}
