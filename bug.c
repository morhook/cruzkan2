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

void draw_bug(void)
{
    double sprite_x;
    double sprite_y;
    double inverse_det;
    double transform_x;
    double transform_y;
    double screen_center;
    double sprite_height;
    double sprite_width;
    double left;
    double top;
    int first_column;
    int last_column;
    int column;
    int x;
    int y;
    int source_x;
    int source_y;
    int screen_x;
    int screen_y;
    int ray;
    char pixel;
    unsigned int offset;
    double camera_x;
    double ray_dir_x;
    double ray_dir_y;
    double wall_distance;

    if (!bug_alive) {
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
    sprite_height = fabs(SCREEN_H / transform_y) * 0.62;
    sprite_width = sprite_height * BUG_W / BUG_H;
    left = screen_center - sprite_width / 2.0;
    top = HALF_H - sprite_height / 2.0;
    first_column = (int)left;
    last_column = (int)(left + sprite_width);
    if (first_column < 0) first_column = 0;
    if (last_column >= SCREEN_W) last_column = SCREEN_W - 1;

    for (column = first_column; column <= last_column; ++column) {
        ray = column / COLUMN_W;
        camera_x = (2.0 * ray) / (double)NUM_RAYS - 1.0;
        ray_dir_x = dirX + planeX * camera_x;
        ray_dir_y = dirY + planeY * camera_x;
        wall_distance = bug_wall_distance(ray_dir_x, ray_dir_y);
        if (transform_y >= wall_distance) {
            continue;
        }
        x = (int)(((column - left) / sprite_width) * BUG_W);
        if (x < 0) x = 0;
        if (x >= BUG_W) x = BUG_W - 1;
        for (y = 0; y < (int)sprite_height; ++y) {
            screen_y = (int)top + y;
            if (screen_y < 0 || screen_y >= SCREEN_H) {
                continue;
            }
            source_y = (int)((y / sprite_height) * BUG_H);
            pixel = bug_sprite[source_y][x];
            if (pixel == '.' || pixel == '\0') {
                continue;
            }
            source_x = pixel - '1';
            if (source_x < 0 || source_x > 5) {
                continue;
            }
            screen_x = column;
            offset = (unsigned int)screen_y * SCREEN_W + screen_x;
            VGA[offset] = (unsigned char)(BUG_COLOR_BASE + source_x);
        }
    }
}

int bug_game_result(void)
{
    return game_result;
}
