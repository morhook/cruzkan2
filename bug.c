#include <math.h>
#include "bug.h"
#include "game.h"

#define BUG_W 16
#define BUG_H 12
#define BUG_SPEED 0.025
#define BUG_CONTACT_DISTANCE 0.38
#define BUG_SHOT_RADIUS 0.28
#define BUG_COLOR_BASE 63

static double bug_x = 11.5;
static double bug_y = 3.5;
static int bug_alive = 1;
static int game_result = 0; /* 0 playing, 1 player defeated, 2 bug defeated */

/* Sprite projection is computed once per frame, then each screen column is
   composited as the renderer draws it. Drawing the sprite only after the
   whole frame makes it visible for part of a frame and causes flicker. */
static int bug_visible = 0;
static int bug_first_column = 0;
static int bug_last_column = 0;
static double bug_left = 0.0;
static double bug_top = 0.0;
static double bug_sprite_width = 0.0;
static double bug_sprite_height = 0.0;
static double bug_depth = 0.0;

static char bug_sprite[BUG_H][BUG_W + 1] = {
    "....11....11....",
    "...1221..1221...",
    "..122221122221..",
    ".12233333333221.",
    "1223344433443321",
    "1223445555443321",
    ".12345555554321.",
    "..12344444321..",
    ".12.1.22.1.21...",
    "12..1.22.1..21..",
    "....1....1......",
    "...22....22....."
};

void init_bug_palette(void)
{
    set_palette(BUG_COLOR_BASE, 10, 24, 5);
    set_palette(BUG_COLOR_BASE + 1, 24, 42, 8);
    set_palette(BUG_COLOR_BASE + 2, 42, 52, 12);
    set_palette(BUG_COLOR_BASE + 3, 30, 12, 4);
    set_palette(BUG_COLOR_BASE + 4, 58, 18, 8);
    set_palette(BUG_COLOR_BASE + 5, 63, 48, 12);
}

static void move_bug(double dx, double dy)
{
    double next_x;
    double next_y;

    next_x = bug_x + dx;
    next_y = bug_y + dy;
    if (!is_wall((int)next_x, (int)bug_y)) {
        bug_x = next_x;
    }
    if (!is_wall((int)bug_x, (int)next_y)) {
        bug_y = next_y;
    }
}

void update_bug(void)
{
    double dx;
    double dy;
    double distance;
    double step;

    if (!bug_alive || game_result != 0) {
        return;
    }
    dx = posX - bug_x;
    dy = posY - bug_y;
    distance = sqrt(dx * dx + dy * dy);
    if (distance <= BUG_CONTACT_DISTANCE) {
        game_result = 1;
        return;
    }
    if (distance < 0.001) {
        return;
    }
    step = BUG_SPEED / distance;
    if (step > 1.0) {
        step = 1.0;
    }
    move_bug(dx * step, dy * step);
    dx = posX - bug_x;
    dy = posY - bug_y;
    if (sqrt(dx * dx + dy * dy) <= BUG_CONTACT_DISTANCE) {
        game_result = 1;
    }
}

/* Return the first wall's perpendicular distance along a camera ray. */
static double bug_wall_distance(double ray_dir_x, double ray_dir_y)
{
    int map_x;
    int map_y;
    int step_x;
    int step_y;
    int side;
    double delta_x;
    double delta_y;
    double side_x;
    double side_y;

    map_x = (int)posX;
    map_y = (int)posY;
    delta_x = fabs(ray_dir_x) < RAY_EPSILON ? 1.0e30 : fabs(1.0 / ray_dir_x);
    delta_y = fabs(ray_dir_y) < RAY_EPSILON ? 1.0e30 : fabs(1.0 / ray_dir_y);
    if (ray_dir_x < 0.0) {
        step_x = -1;
        side_x = (posX - map_x) * delta_x;
    } else {
        step_x = 1;
        side_x = (map_x + 1.0 - posX) * delta_x;
    }
    if (ray_dir_y < 0.0) {
        step_y = -1;
        side_y = (posY - map_y) * delta_y;
    } else {
        step_y = 1;
        side_y = (map_y + 1.0 - posY) * delta_y;
    }
    for (;;) {
        if (side_x < side_y) {
            side_x += delta_x;
            map_x += step_x;
            side = 0;
        } else {
            side_y += delta_y;
            map_y += step_y;
            side = 1;
        }
        if (is_wall(map_x, map_y)) {
            return side == 0 ? side_x - delta_x : side_y - delta_y;
        }
    }
}

void shoot_bug_if_aimed(void)
{
    double dx;
    double dy;
    double forward;
    double lateral;
    double wall_distance;

    if (!shot_fired || !bug_alive || game_result != 0) {
        return;
    }
    dx = bug_x - posX;
    dy = bug_y - posY;
    forward = dx * dirX + dy * dirY;
    lateral = fabs(dx * dirY - dy * dirX);
    if (forward <= 0.0 || lateral > BUG_SHOT_RADIUS) {
        return;
    }
    wall_distance = bug_wall_distance(dirX, dirY);
    if (forward < wall_distance) {
        bug_alive = 0;
        game_result = 2;
    }
}

void begin_bug_frame(void)
{
    double sprite_x;
    double sprite_y;
    double inverse_det;
    double transform_x;
    double transform_y;
    double screen_center;

    bug_visible = 0;
    if (!bug_alive || game_result != 0) {
        return;
    }
    sprite_x = bug_x - posX;
    sprite_y = bug_y - posY;
    inverse_det = 1.0 / (planeX * dirY - dirX * planeY);
    transform_x = inverse_det * (dirY * sprite_x - dirX * sprite_y);
    transform_y = inverse_det * (-planeY * sprite_x + planeX * sprite_y);
    if (transform_y <= 0.10) {
        return;
    }
    screen_center = (SCREEN_W / 2.0) * (1.0 + transform_x / transform_y);
    bug_sprite_height = fabs(SCREEN_H / transform_y) * 0.62;
    bug_sprite_width = bug_sprite_height * BUG_W / BUG_H;
    bug_left = screen_center - bug_sprite_width / 2.0;
    bug_top = HALF_H - bug_sprite_height / 2.0;
    bug_depth = transform_y;
    bug_first_column = (int)bug_left;
    bug_last_column = (int)(bug_left + bug_sprite_width);
    if (bug_first_column < 0) bug_first_column = 0;
    if (bug_last_column >= SCREEN_W) bug_last_column = SCREEN_W - 1;
    bug_visible = 1;
}

void draw_bug_column(int column, double wall_distance)
{
    int x;
    int y;
    int source_x;
    int source_y;
    int screen_y;
    char pixel;
    unsigned int offset;

    if (!bug_visible || column < bug_first_column ||
        column > bug_last_column) {
        return;
    }
    if (bug_depth >= wall_distance) {
        return;
    }
    x = (int)(((column - bug_left) / bug_sprite_width) * BUG_W);
    if (x < 0) x = 0;
    if (x >= BUG_W) x = BUG_W - 1;
    for (y = 0; y < (int)bug_sprite_height; ++y) {
        screen_y = (int)bug_top + y;
        if (screen_y < 0 || screen_y >= SCREEN_H) {
            continue;
        }
        source_y = (int)((y / bug_sprite_height) * BUG_H);
        pixel = bug_sprite[source_y][x];
        if (pixel == '.' || pixel == '\0') {
            continue;
        }
        source_x = pixel - '1';
        if (source_x < 0 || source_x > 5) {
            continue;
        }
        offset = (unsigned int)screen_y * SCREEN_W + column;
        VGA[offset] = (unsigned char)(BUG_COLOR_BASE + source_x);
    }
}

int bug_game_result(void)
{
    return game_result;
}
