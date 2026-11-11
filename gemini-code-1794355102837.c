#ifndef GAME_H
#define GAME_H

#define STATE_MENU     0
#define STATE_PLAYING  1
#define STATE_PAUSED   2
#define STATE_GAMEOVER 3

#define MODE_1PLAYER   0
#define MODE_2PLAYER   1

// Fixed-point coordinates (1 pixel = 256 subpixel units)
#define SCREEN_W_UNITS 61440 
#define SCREEN_H_UNITS 56320 
#define SPLIT_W_UNITS  30720 

#define BRICK_COLS     8
#define BRICK_ROWS     5
#define BRICK_W_UNITS  6144
#define BRICK_H_UNITS  2048

extern unsigned char current_state;
extern unsigned char game_mode;
extern unsigned char menu_selection;

// Parallel state arrays for Player 1 [0] and Player 2 [1]
extern unsigned char current_level[2];
extern unsigned int score[2];
extern unsigned char remaining_bricks[2];
extern unsigned char strikes[2];
extern unsigned char player_active[2];

extern int ball_x[2], ball_y[2], ball_z[2];
extern int ball_vx[2], ball_vy[2];
extern int paddle_x[2];
extern unsigned char brick_grid[2][BRICK_ROWS][BRICK_COLS];

// Match timer metrics (5 minutes @ 60Hz = 18,000 frames)
extern unsigned int match_frames_left;
extern unsigned char final_winner;

// VRAM Buffer Definitions
#define VRAM_BUF_SIZE 64
extern unsigned char vram_buffer[VRAM_BUF_SIZE];
extern unsigned char vram_buf_ptr;

void init_game(void);
void load_player_level(unsigned char p_idx, unsigned char lvl);
void update_game(unsigned char p1_pad, unsigned char p2_pad);
void queue_vram_text(unsigned int ppu_addr, const char* text);

#endif