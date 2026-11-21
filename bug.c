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

/* ------------------------------------------------------------------ */
/* Ghost enemy — teleporting, translucent specter.                    */
/* ------------------------------------------------------------------ */

#define GHOST_W 14
#define GHOST_H 14
#define GHOST_SPEED 0.018
#define GHOST_CONTACT_DISTANCE 0.35
#define GHOST_SHOT_RADIUS 0.26
/* Keep every ghost color above the weapon/texture/bug ranges: the palette
   is shared, so overlapping indices would recolor the gun and walls. */
#define GHOST_COLOR_BASE 76
#define GHOST_TELEPORT_MIN 90
#define GHOST_TELEPORT_MAX 200
#define GHOST_TELEPORT_SHIMMER 15
#define GHOST_SCARE_DISTANCE 2.8
#define GHOST_SCREECH_FRAMES 45
#define GHOST_EXPLOSION_COLOR_BASE 82
#define GHOST_EXPLOSION_FRAMES 20

static double ghost_x = 13.5;
static double ghost_y = 12.5;
static int ghost_alive = 1;
static unsigned int ghost_screech_wait = 0;
static int ghost_teleport_charge = 0;
static int ghost_teleporting = 0;
static int ghost_teleport_timer = 0;

/* The ghost's wisp death effect. */
static double ghost_explosion_x = 0.0;
static double ghost_explosion_y = 0.0;
static int ghost_explosion_age = 0;
static int ghost_explosion_live = 0;
static int ghost_explosion_visible = 0;
static int ghost_explosion_first_column = 0;
static int ghost_explosion_last_column = 0;
static double ghost_explosion_center_x = 0.0;
static double ghost_explosion_center_y = 0.0;
static double ghost_explosion_radius = 0.0;
static double ghost_explosion_depth = 0.0;
static int ghost_explosion_phase = 0;

/* Sprite projection */
static int ghost_visible = 0;
static int ghost_first_column = 0;
static int ghost_last_column = 0;
static double ghost_left = 0.0;
static double ghost_top = 0.0;
static double ghost_sprite_width = 0.0;
static double ghost_sprite_height = 0.0;
static double ghost_depth = 0.0;

/* Ghost screech: a thin, spectral whistle that drops in pitch. */
#define GHOST_SCREECH_RATE 11025UL
#define GHOST_SCREECH_SAMPLES 4410
#define GHOST_WAIL_SAMPLES 4410
static unsigned char far ghost_screech[GHOST_SCREECH_SAMPLES];
static unsigned char far ghost_wail[GHOST_WAIL_SAMPLES];

/* Ghost sprite — a translucent, wispy apparition with trailing tendrils. */
static char ghost_sprite[GHOST_H][GHOST_W + 1] = {
    "....1111....",
    "...122221...",
    "..12222221..",
    ".1223333221.",
    "122334433221",
    "122344443221",
    ".1234444321.",
    "..12344321..",
    "..12.4.21...",
    ".12..4..21..",
    "12...4...21.",
    "1.......21..",
    ".1.....21...",
    "..1...21...."
};

void init_ghost_palette(void)
{
    /* Ghost body: ethereal blue-grey shades. */
    set_palette(GHOST_COLOR_BASE, 18, 22, 30);
    set_palette(GHOST_COLOR_BASE + 1, 30, 38, 50);
    set_palette(GHOST_COLOR_BASE + 2, 42, 50, 60);
    set_palette(GHOST_COLOR_BASE + 3, 22, 28, 38);
    set_palette(GHOST_COLOR_BASE + 4, 55, 58, 62);
    set_palette(GHOST_COLOR_BASE + 5, 63, 63, 65);

    /* Ghost wisp explosion: white-hot to ghostly blue smoke. */
    set_palette(GHOST_EXPLOSION_COLOR_BASE, 58, 58, 63);
    set_palette(GHOST_EXPLOSION_COLOR_BASE + 1, 50, 50, 63);
    set_palette(GHOST_EXPLOSION_COLOR_BASE + 2, 42, 42, 58);
    set_palette(GHOST_EXPLOSION_COLOR_BASE + 3, 30, 30, 48);
    set_palette(GHOST_EXPLOSION_COLOR_BASE + 4, 18, 18, 36);
    set_palette(GHOST_EXPLOSION_COLOR_BASE + 5, 10, 10, 26);
    set_palette(GHOST_EXPLOSION_COLOR_BASE + 6, 6, 6, 18);
}

void init_ghost_sound(void)
{
    unsigned int i;
    unsigned int seed;
    double time;
    double progress;
    double frequency;
    double envelope;
    double value;
    double noise;
    double tremolo;
    double tone;

    /* Ghost screech: thin, wavering whistle. */
    seed = 0x3A7Fu;
    for (i = 0; i < GHOST_SCREECH_SAMPLES; ++i) {
        time = (double)i / (double)GHOST_SCREECH_RATE;
        progress = (double)i / (double)GHOST_SCREECH_SAMPLES;
        frequency = 500.0 - 280.0 * progress;
        seed = (unsigned int)(seed * 25173UL + 13849UL);
        noise = (double)(seed & 0xFFU) / 127.5 - 1.0;
        tremolo = 0.5 + 0.5 * sin(2.0 * 3.141592653589793 * 12.0 * time);
        tone = sin(2.0 * 3.141592653589793 * frequency * time) +
               0.45 * sin(2.0 * 3.141592653589793 * frequency * 2.01 * time +
                          0.7);
        envelope = (1.0 - exp(-time / 0.012)) * exp(-time / 0.28);
        value = 128.0 + 2.6 * envelope * tremolo * (tone * 28.0 + noise * 18.0);
        if (value > 255.0) value = 255.0;
        else if (value < 0.0) value = 0.0;
        ghost_screech[i] = (unsigned char)value;
    }

    /* Ghost wail: low, droning moan. */
    seed = 0x7E21u;
    for (i = 0; i < GHOST_WAIL_SAMPLES; ++i) {
        time = (double)i / (double)GHOST_SCREECH_RATE;
        progress = (double)i / (double)GHOST_WAIL_SAMPLES;
        frequency = 80.0 + 50.0 * sin(2.0 * 3.141592653589793 * 3.0 * time);
        seed = (unsigned int)(seed * 25173UL + 13849UL);
        noise = (double)(seed & 0xFFU) / 127.5 - 1.0;
        tone = sin(2.0 * 3.141592653589793 * frequency * time) +
               0.35 * sin(2.0 * 3.141592653589793 * frequency * 1.5 * time +
                          1.3);
        envelope = (1.0 - exp(-time / 0.010)) * exp(-time / 0.32);
        value = 128.0 + 2.6 * envelope * (tone * 22.0 + noise * 30.0);
        if (value > 255.0) value = 255.0;
        else if (value < 0.0) value = 0.0;
        ghost_wail[i] = (unsigned char)value;
    }
}

static void play_ghost_screech(double distance)
{
    double closeness;
    unsigned char volume;
    double factor;

    closeness = 1.0 - distance / GHOST_SCARE_DISTANCE;
    if (closeness < 0.0) closeness = 0.0;
    volume = (unsigned char)(70.0 + closeness * 185.0);
    factor = 1.0 - closeness * 0.5; /* pitch shift: closer = higher */
    (void)audio_play_sample(ghost_screech,
                            (unsigned int)(GHOST_SCREECH_SAMPLES * factor),
                            volume);
}

static void play_ghost_wail(void)
{
    (void)audio_play_sample(ghost_wail, GHOST_WAIL_SAMPLES, 160);
}

static void start_ghost_explosion(void)
{
    ghost_explosion_x = ghost_x;
    ghost_explosion_y = ghost_y;
    ghost_explosion_age = 0;
    ghost_explosion_live = 1;
    ghost_explosion_visible = 0;
    (void)audio_play_sample(ghost_wail, GHOST_WAIL_SAMPLES, 120);
}

static void try_ghost_teleport(void)
{
    int tx;
    int ty;
    int attempts;
    int best_x;
    int best_y;
    double dx;
    double dy;
    double best_distance;

    /* Pick a random floor tile far enough from the player. */
    attempts = 0;
    do {
        tx = 1 + (rand() % (MAP_W - 2));
        ty = 1 + (rand() % (MAP_H - 2));
        dx = (double)tx + 0.5 - posX;
        dy = (double)ty + 0.5 - posY;
        ++attempts;
    } while ((is_wall(tx, ty) ||
              (dx * dx + dy * dy) < 9.0) &&
             attempts < 100);

    /* Fallback: use the farthest floor tile from the player. */
    if (is_wall(tx, ty) || (dx * dx + dy * dy) < 9.0) {
        best_x = tx;
        best_y = ty;
        best_distance = -1.0;
        for (ty = 1; ty < MAP_H - 1; ++ty) {
            for (tx = 1; tx < MAP_W - 1; ++tx) {
                if (is_wall(tx, ty)) continue;
                dx = (double)tx + 0.5 - posX;
                dy = (double)ty + 0.5 - posY;
                if (dx * dx + dy * dy > best_distance) {
                    best_distance = dx * dx + dy * dy;
                    best_x = tx;
                    best_y = ty;
                }
            }
        }
        tx = best_x;
        ty = best_y;
    }

    ghost_x = (double)tx + 0.5;
    ghost_y = (double)ty + 0.5;
    ghost_teleporting = 1;
    ghost_teleport_timer = GHOST_TELEPORT_SHIMMER;
    play_ghost_wail();
}

static void begin_ghost_explosion_frame(void)
{
    double screen_x;
    double depth;
    double progress;
    double radius;

    ghost_explosion_visible = 0;
    if (!ghost_explosion_live) return;
    if (ghost_explosion_age >= GHOST_EXPLOSION_FRAMES) {
        ghost_explosion_live = 0;
        ghost_explosion_age = 0;
        return;
    }
    if (!project_point(ghost_explosion_x, ghost_explosion_y, &screen_x, &depth))
        return;
    progress = (double)ghost_explosion_age / (double)(GHOST_EXPLOSION_FRAMES - 1);
    radius = (SCREEN_H / depth) * (0.18 + 0.38 * progress);
    if (radius > 130.0) radius = 130.0;
    if (screen_x < -200.0 || screen_x > (double)SCREEN_W + 200.0) return;
    ghost_explosion_center_x = screen_x;
    ghost_explosion_center_y = (double)HALF_H;
    ghost_explosion_radius = radius;
    ghost_explosion_depth = depth;
    ghost_explosion_phase = (int)((ghost_explosion_age * 3) /
                                   GHOST_EXPLOSION_FRAMES);
    ghost_explosion_first_column = (int)(screen_x - radius);
    ghost_explosion_last_column = (int)(screen_x + radius);
    if (ghost_explosion_last_column < 0 ||
        ghost_explosion_first_column >= SCREEN_W) return;
    if (ghost_explosion_first_column < 0) ghost_explosion_first_column = 0;
    if (ghost_explosion_last_column >= SCREEN_W)
        ghost_explosion_last_column = SCREEN_W - 1;
    ghost_explosion_visible = 1;
}

static void move_ghost(double dx, double dy)
{
    double next_x;
    double next_y;

    next_x = ghost_x + dx;
    next_y = ghost_y + dy;
    if (!is_wall((int)next_x, (int)ghost_y)) ghost_x = next_x;
    if (!is_wall((int)ghost_x, (int)next_y)) ghost_y = next_y;
}

void update_ghost(void)
{
    double dx;
    double dy;
    double distance;
    double step;

    if (!ghost_alive || game_result != 0) return;

    if (ghost_teleporting) {
        --ghost_teleport_timer;
        if (ghost_teleport_timer <= 0) ghost_teleporting = 0;
        return;
    }

    dx = posX - ghost_x;
    dy = posY - ghost_y;
    distance = sqrt(dx * dx + dy * dy);

    if (distance <= GHOST_CONTACT_DISTANCE) {
        game_result = 1;
        return;
    }

    if (ghost_screech_wait > 0) {
        --ghost_screech_wait;
    }

    /* Charging a teleport: hold still (still deadly to touch), then vanish. */
    if (ghost_teleport_charge > 0) {
        --ghost_teleport_charge;
        if (ghost_teleport_charge == 0) {
            try_ghost_teleport();
        }
        return;
    }

    if (distance < 0.001) return;

    step = GHOST_SPEED / distance;
    if (step > 1.0) step = 1.0;
    move_ghost(dx * step, dy * step);

    dx = posX - ghost_x;
    dy = posY - ghost_y;
    distance = sqrt(dx * dx + dy * dy);
    if (distance <= GHOST_CONTACT_DISTANCE) {
        game_result = 1;
        return;
    }

    /* Screech when close. */
    if (distance <= GHOST_SCARE_DISTANCE && ghost_screech_wait == 0) {
        ghost_screech_wait = GHOST_SCREECH_FRAMES;
        play_ghost_screech(distance);
    }

    /* Player got too close: wind up, then teleport away. */
    if (distance < 1.8) {
        ghost_teleport_charge = GHOST_TELEPORT_MIN +
            (rand() % (GHOST_TELEPORT_MAX - GHOST_TELEPORT_MIN));
    }
}

void shoot_ghost_if_aimed(void)
{
    double dx;
    double dy;
    double forward;
    double lateral;
    double wall_distance;

    if (!shot_fired || !ghost_alive || game_result != 0) return;
    dx = ghost_x - posX;
    dy = ghost_y - posY;
    forward = dx * dirX + dy * dirY;
    lateral = fabs(dx * dirY - dy * dirX);
    if (forward <= 0.0 || lateral > GHOST_SHOT_RADIUS) return;
    wall_distance = bug_wall_distance(dirX, dirY);
    if (forward < wall_distance) {
        ghost_alive = 0;
        start_ghost_explosion();
    }
}

void begin_ghost_frame(void)
{
    double screen_center;
    double sprite_height;
    double depth;

    ghost_visible = 0;
    begin_ghost_explosion_frame();
    if (ghost_explosion_live) ++ghost_explosion_age;

    if (!ghost_alive || game_result != 0) return;
    if (!project_point(ghost_x, ghost_y, &screen_center, &depth)) return;

    sprite_height = fabs(SCREEN_H / depth) * 0.55;
    ghost_sprite_height = sprite_height;
    ghost_sprite_width = sprite_height * GHOST_W / GHOST_H;
    ghost_left = screen_center - ghost_sprite_width / 2.0;
    ghost_top = HALF_H - ghost_sprite_height / 2.0;
    ghost_depth = depth;
    ghost_first_column = (int)ghost_left;
    ghost_last_column = (int)(ghost_left + ghost_sprite_width);
    if (ghost_first_column < 0) ghost_first_column = 0;
    if (ghost_last_column >= SCREEN_W) ghost_last_column = SCREEN_W - 1;
    ghost_visible = 1;
}

void draw_ghost_column(int column, double wall_distance)
{
    int x;
    int y;
    int source_x;
    int source_y;
    int screen_y;
    char pixel;
    unsigned int offset;
    int shimmer;

    if (!ghost_visible || column < ghost_first_column ||
        column > ghost_last_column) return;
    if (ghost_depth >= wall_distance) return;

    x = (int)(((column - ghost_left) / ghost_sprite_width) * GHOST_W);
    if (x < 0) x = 0;
    if (x >= GHOST_W) x = GHOST_W - 1;

    for (y = 0; y < (int)ghost_sprite_height; ++y) {
        screen_y = (int)ghost_top + y;
        if (screen_y < 0 || screen_y >= SCREEN_H) continue;
        source_y = (int)((y / ghost_sprite_height) * GHOST_H);
        pixel = ghost_sprite[source_y][x];
        if (pixel == '.' || pixel == '\0') continue;
        source_x = pixel - '1';
        if (source_x < 0 || source_x > 5) continue;

        /* Teleport shimmer: skip pixels for a flash effect. */
        if (ghost_teleporting ||
            (ghost_teleport_charge > 0 &&
             ghost_teleport_charge <= GHOST_TELEPORT_SHIMMER)) {
            shimmer = ghost_teleporting ? ghost_teleport_timer
                                        : ghost_teleport_charge;
            if (((column * 7 + y * 11 + shimmer * 3) & 3) == 0)
                continue;
        }

        offset = (unsigned int)screen_y * SCREEN_W + column;
        VGA[offset] = (unsigned char)(GHOST_COLOR_BASE + source_x);
    }
}

void draw_ghost_explosion_column(int column, double wall_distance)
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

    if (!ghost_explosion_visible || column < ghost_explosion_first_column ||
        column > ghost_explosion_last_column) return;
    if (ghost_explosion_depth >= wall_distance) return;

    outer_sq = (unsigned int)(ghost_explosion_radius * ghost_explosion_radius);
    if (outer_sq == 0) return;
    core_sq = outer_sq / 8;
    mid_sq = (outer_sq / 8) * 3;

    top = (int)(ghost_explosion_center_y - ghost_explosion_radius);
    bottom = (int)(ghost_explosion_center_y + ghost_explosion_radius);
    if (top < 0) top = 0;
    if (bottom >= SCREEN_H) bottom = SCREEN_H - 1;

    dx = column - (int)ghost_explosion_center_x;
    if (dx < 0) dx = -dx;
    if ((double)dx > ghost_explosion_radius) return;

    for (y = top; y <= bottom; ++y) {
        dy = y - (int)ghost_explosion_center_y;
        if (dy < 0) dy = -dy;
        if ((double)dy > ghost_explosion_radius) continue;
        d2 = (unsigned int)(dx * dx + dy * dy);
        if (d2 > outer_sq) continue;
        if (d2 <= core_sq) ring = 0;
        else if (d2 <= mid_sq) ring = 1;
        else ring = 2;

        /* Ragged, wispy edges for the ghostly explosion. */
        if (ring == 2 &&
            (((column * 13 + y * 11 + ghost_explosion_age * 7) & 7) == 0))
            continue;
        if (ring > 0 &&
            (((column * 9 + y * 17 + ghost_explosion_age * 13) & 7) == 0))
            ++ring;

        shade = ring + ghost_explosion_phase;
        if (shade > 6) shade = 6;
        color = (unsigned char)(GHOST_EXPLOSION_COLOR_BASE + shade);
        offset = (unsigned int)y * SCREEN_W + column;
        VGA[offset] = color;
    }
}

int ghost_killed(void)
{
    return !ghost_alive;
}

int bug_game_result(void)
{
    return game_result;
}

int bug_killed(void)
{
    return !bug_alive;
}
