#ifndef CRUZ_BUG_H
#define CRUZ_BUG_H

void init_bug_palette(void);
/* Builds the scare sample; call once before gameplay starts. */
void init_bug_sound(void);
void update_bug(void);
void shoot_bug_if_aimed(void);
void begin_bug_frame(void);
void draw_bug_column(int column, double wall_distance);
void draw_bug_explosion_column(int column, double wall_distance);

/* 0 while playing, 1 if the player was caught. Shooting the bug only
   clears the way; the game keeps running so the bullseye is still the
   only way to win. */
int bug_game_result(void);
/* Nonzero once the bug has been shot (its fireball may still be fading). */
int bug_killed(void);

#endif
