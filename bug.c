#include <math.h>
#include "audio.h"
#include "bug.h"
#include "game.h"

#define BUG_W 16
#define BUG_H 12
#define BUG_SPEED 0.025
#define BUG_CONTACT_DISTANCE 0.38
#define BUG_SHOT_RADIUS 0.28
#define BUG_COLOR_BASE 63
#define EXPLOSION_COLOR_BASE 69
#define EXPLOSION_FRAMES 18

/* Scare sound: a dissonant, fluttering shriek that rises in pitch.
   It plays while the bug closes in, so proximity is what makes it scary. */
#define BUG_SCREECH_RATE 11025UL
#define BUG_SCREECH_SAMPLES 6615
#define BUG_BOOM_SAMPLES 5512
#define BUG_SCARE_DISTANCE 3.0
#define BUG_SCREECH_FRAMES 50

static double bug_x = 11.5;
static double bug_y = 3.5;
static int bug_alive = 1;
static int game_result = 0; /* 0 playing, 1 player caught */
static unsigned int scare_wait = 0;
static unsigned char far bug_screech[BUG_SCREECH_SAMPLES];
static unsigned char far bug_boom[BUG_BOOM_SAMPLES];

/* The fireball lives where the bug died and is drawn with the same
   per-column wall test, so walls still occlude it correctly. */
static double explosion_x = 0.0;
static double explosion_y = 0.0;
static int explosion_age = 0;
static int explosion_live = 0;
static int explosion_visible = 0;
static int explosion_first_column = 0;
static int explosion_last_column = 0;
static double explosion_center_x = 0.0;
static double explosion_center_y = 0.0;
static double explosion_radius = 0.0;
static double explosion_depth = 0.0;
static int explosion_heat = 0;

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

    /* Fireball ramp: white-hot core down through orange to dark smoke. */
    set_palette(EXPLOSION_COLOR_BASE, 63, 63, 56);
    set_palette(EXPLOSION_COLOR_BASE + 1, 63, 60, 18);
    set_palette(EXPLOSION_COLOR_BASE + 2, 63, 38, 4);
    set_palette(EXPLOSION_COLOR_BASE + 3, 48, 16, 2);
    set_palette(EXPLOSION_COLOR_BASE + 4, 28, 8, 2);
    set_palette(EXPLOSION_COLOR_BASE + 5, 18, 12, 10);
    set_palette(EXPLOSION_COLOR_BASE + 6, 10, 9, 9);
}

void init_bug_sound(void)
{
    unsigned int i;
    unsigned int seed;
    double time;
    double progress;
    double frequency;
    double envelope;
    double flutter;
    double tone;
    double noise;
    double value;

    /* No external asset: a fixed pseudo-random seed keeps the screech
       identical on every run while still sounding rough. */
    seed = 0x1D2Bu;
    for (i = 0; i < BUG_SCREECH_SAMPLES; ++i) {
        time = (double)i / (double)BUG_SCREECH_RATE;
        progress = (double)i / (double)BUG_SCREECH_SAMPLES;
        /* Climbing pitch reads as something charging at the player. */
        frequency = 160.0 + 640.0 * progress * progress;
        /* Fast flutter gives the tone an insect-like rasp. */
        flutter = 0.58 + 0.42 * sin(2.0 * 3.141592653589793 * 26.0 * time);
        tone = sin(2.0 * 3.141592653589793 * frequency * time) +
               0.62 * sin(2.0 * 3.141592653589793 * frequency * 1.47 * time +
                          1.1) +
               0.40 * sin(2.0 * 3.141592653589793 * frequency * 2.09 * time +
                          2.3) +
               0.50 * sin(2.0 * 3.141592653589793 * frequency * 0.5 * time);
        seed = (unsigned int)(seed * 25173UL + 13849UL);
        noise = (double)(seed & 0xFFU) / 127.5 - 1.0;
        envelope = (1.0 - exp(-time / 0.015)) * exp(-time / 0.34);
        value = 128.0 + 2.6 * envelope * flutter *
                        (tone * 33.0 + noise * 22.0);
        if (value > 255.0) {
            value = 255.0;
        } else if (value < 0.0) {
            value = 0.0;
        }
        bug_screech[i] = (unsigned char)value;
    }

    /* Explosion: descending thump smothered in decaying noise, with a
       short crackle on the leading edge so the hit reads instantly. */
    seed = 0x51F3u;
    for (i = 0; i < BUG_BOOM_SAMPLES; ++i) {
        time = (double)i / (double)BUG_SCREECH_RATE;
        progress = (double)i / (double)BUG_BOOM_SAMPLES;
        frequency = 150.0 - 108.0 * progress;
        seed = (unsigned int)(seed * 25173UL + 13849UL);
        noise = (double)(seed & 0xFFU) / 127.5 - 1.0;
        tone = sin(2.0 * 3.141592653589793 * frequency * time);
        envelope = (1.0 - exp(-time / 0.008)) * exp(-time / 0.26);
        value = 128.0 + 2.6 * (envelope * (tone * 46.0 + noise * 58.0) +
                               exp(-time / 0.045) * noise * 64.0);
        if (value > 255.0) {
            value = 255.0;
        } else if (value < 0.0) {
            value = 0.0;
        }
        bug_boom[i] = (unsigned char)value;
    }
}

static void play_bug_screech(double distance)
{
    double closeness;
    unsigned char volume;

    closeness = 1.0 - distance / BUG_SCARE_DISTANCE;
    if (closeness < 0.0) {
        closeness = 0.0;
    }
    volume = (unsigned char)(85.0 + closeness * 170.0);
    (void)audio_play_sample(bug_screech, BUG_SCREECH_SAMPLES, volume);
}

static void start_bug_explosion(void)
{
    explosion_x = bug_x;
    explosion_y = bug_y;
    explosion_age = 0;
    explosion_heat = 0;
    explosion_live = 1;
    explosion_visible = 0;
    (void)audio_play_sample(bug_boom, BUG_BOOM_SAMPLES, 240);
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
    if (scare_wait > 0) {
        --scare_wait;
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
    distance = sqrt(dx * dx + dy * dy);
    if (distance <= BUG_CONTACT_DISTANCE) {
        game_result = 1;
        return;
    }
    if (distance <= BUG_SCARE_DISTANCE && scare_wait == 0) {
        scare_wait = BUG_SCREECH_FRAMES;
        play_bug_screech(distance);
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
        start_bug_explosion();
    }
}

/* Project a world point into screen columns. Returns 0 when it is behind
   the camera or too close to be meaningful. */
static int project_point(double x, double y, double *screen_x, double *depth)
{
    double sprite_x;
    double sprite_y;
    double inverse_det;
    double transform_x;
    double transform_y;

    sprite_x = x - posX;
    sprite_y = y - posY;
    inverse_det = 1.0 / (planeX * dirY - dirX * planeY);
    transform_x = inverse_det * (dirY * sprite_x - dirX * sprite_y);
    transform_y = inverse_det * (-planeY * sprite_x + planeX * sprite_y);
    if (transform_y <= 0.10) {
        return 0;
    }
    *screen_x = (SCREEN_W / 2.0) * (1.0 + transform_x / transform_y);
    *depth = transform_y;
    return 1;
}

static void begin_explosion_frame(void)
{
    double screen_x;
    double depth;
    double progress;
    double radius;

    explosion_visible = 0;
    if (!explosion_live) {
        return;
    }
    if (explosion_age >= EXPLOSION_FRAMES) {
        explosion_live = 0;
        explosion_age = 0;
        return;
    }
    if (!project_point(explosion_x, explosion_y, &screen_x, &depth)) {
        return;
    }
    progress = (double)explosion_age / (double)(EXPLOSION_FRAMES - 1);
    radius = (SCREEN_H / depth) * (0.22 + 0.40 * progress);
    if (radius > 150.0) {
        radius = 150.0;
    }
    if (screen_x < -200.0 || screen_x > (double)SCREEN_W + 200.0) {
        return;
    }
    explosion_center_x = screen_x;
    explosion_center_y = (double)HALF_H;
    explosion_radius = radius;
    explosion_depth = depth;
    explosion_heat = (explosion_age * 5) / EXPLOSION_FRAMES;
    explosion_first_column = (int)(screen_x - radius);
    explosion_last_column = (int)(screen_x + radius);
    if (explosion_last_column < 0 || explosion_first_column >= SCREEN_W) {
        return;
    }
    if (explosion_first_column < 0) {
        explosion_first_column = 0;
    }
    if (explosion_last_column >= SCREEN_W) {
        explosion_last_column = SCREEN_W - 1;
    }
    explosion_visible = 1;
}

void begin_bug_frame(void)
{
    double screen_center;
    double sprite_height;
    double depth;

    bug_visible = 0;
    begin_explosion_frame();
    if (explosion_live) {
        ++explosion_age;
    }

    if (!bug_alive || game_result != 0) {
        return;
    }
    if (!project_point(bug_x, bug_y, &screen_center, &depth)) {
        return;
    }
    sprite_height = fabs(SCREEN_H / depth) * 0.62;
    bug_sprite_height = sprite_height;
    bug_sprite_width = sprite_height * BUG_W / BUG_H;
    bug_left = screen_center - bug_sprite_width / 2.0;
    bug_top = HALF_H - bug_sprite_height / 2.0;
    bug_depth = depth;
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

void draw_bug_explosion_column(int column, double wall_distance)
{
    int y;
    int top;
    int bottom;
    int dx;
    int dy;
    unsigned int d2;
    unsigned int outer_sq;
    unsigned int core_sq;
    unsigned int mid_sq;
    int ring;
    int shade;
    unsigned int offset;
    unsigned char color;

    if (!explosion_visible || column < explosion_first_column ||
        column > explosion_last_column) {
        return;
    }
    if (explosion_depth >= wall_distance) {
        return;
    }
    outer_sq = (unsigned int)(explosion_radius * explosion_radius);
    if (outer_sq == 0) {
        return;
    }
    core_sq = outer_sq / 8;
    mid_sq = (outer_sq / 8) * 3;

    top = (int)(explosion_center_y - explosion_radius);
    bottom = (int)(explosion_center_y + explosion_radius);
    if (top < 0) top = 0;
    if (bottom >= SCREEN_H) bottom = SCREEN_H - 1;

    dx = column - (int)explosion_center_x;
    if (dx < 0) dx = -dx;
    if ((double)dx > explosion_radius) {
        return;
    }
    for (y = top; y <= bottom; ++y) {
        dy = y - (int)explosion_center_y;
        if (dy < 0) dy = -dy;
        if ((double)dy > explosion_radius) {
            continue;
        }
        d2 = (unsigned int)(dx * dx + dy * dy);
        if (d2 > outer_sq) {
            continue;
        }
        if (d2 <= core_sq) {
            ring = 0;
        } else if (d2 <= mid_sq) {
            ring = 1;
        } else {
            ring = 2;
        }
        /* Ragged smoke edge and darker embers break up the circle. */
        if (ring == 2 &&
            (((column * 11 + y * 7 + explosion_age * 5) & 7) == 0)) {
            continue;
        }
        if (ring > 0 &&
            (((column * 7 + y * 13 + explosion_age * 11) & 7) == 0)) {
            ++ring;
        }
        shade = ring + explosion_heat;
        if (shade > 6) {
            shade = 6;
        }
        color = (unsigned char)(EXPLOSION_COLOR_BASE + shade);
        offset = (unsigned int)y * SCREEN_W + column;
        VGA[offset] = color;
    }
}

int bug_game_result(void)
{
    return game_result;
}

int bug_killed(void)
{
    return !bug_alive;
}
