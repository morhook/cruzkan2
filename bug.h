#ifndef CRUZ_BUG_H
#define CRUZ_BUG_H

void init_bug_palette(void);
void update_bug(void);
void shoot_bug_if_aimed(void);
void begin_bug_frame(void);
void draw_bug_column(int column, double wall_distance);

/* 0 while playing, 1 if the player was caught, 2 if the bug was shot. */
int bug_game_result(void);

#endif
