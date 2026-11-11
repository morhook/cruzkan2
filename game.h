#ifndef CRUZ_GAME_H
#define CRUZ_GAME_H

/* Shared renderer state used by the game and its modules. */
#define SCREEN_W 320
#define SCREEN_H 200
#define HALF_H 100
#define NUM_RAYS 160
#define COLUMN_W 2
#define RAY_EPSILON 0.000001

extern unsigned char far *VGA;
extern double posX;
extern double posY;
extern double dirX;
extern double dirY;
extern double planeX;
extern double planeY;
extern int shot_fired;

int is_wall(int x, int y);
void set_palette(unsigned char index, unsigned char r, unsigned char g,
                 unsigned char b);

#endif
