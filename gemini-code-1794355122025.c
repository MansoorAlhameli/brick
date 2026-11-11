#include "game.h"

unsigned char current_state;
unsigned char game_mode;
unsigned char menu_selection;

unsigned char current_level[2];
unsigned int score[2];
unsigned char remaining_bricks[2];
unsigned char strikes[2];
unsigned char player_active[2];

int ball_x[2], ball_y[2], ball_z[2];
int ball_vx[2], ball_vy[2];
int paddle_x[2];
unsigned char brick_grid[2][BRICK_ROWS][BRICK_COLS];

unsigned int match_frames_left;
unsigned char final_winner;

unsigned char vram_buffer[VRAM_BUF_SIZE];
unsigned char vram_buf_ptr;

const unsigned char level_layouts[BRICK_ROWS][BRICK_COLS] = {
    { {1,1,1,1,1,1,1,1}, {2,2,2,2,2,2,2,2}, {3,3,3,3,3,3,3,3}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0} },
    { {3,0,3,0,3,0,3,0}, {0,2,0,2,0,2,0,2}, {1,0,1,0,1,0,1,0}, {2,2,2,2,2,2,2,2}, {0,0,0,0,0,0,0,0} },
    { {2,2,2,2,2,2,2,2}, {3,1,1,3,3,1,1,3}, {1,3,3,1,1,3,3,1}, {2,2,2,2,2,2,2,2}, {3,3,3,3,3,3,3,3} },
    { {1,2,3,1,2,3,1,2}, {2,3,1,2,3,1,2,3}, {3,1,2,3,1,2,3,1}, {1,2,3,1,2,3,1,2}, {0,0,0,0,0,0,0,0} },
    { {3,3,3,3,3,3,3,3}, {3,2,2,3,3,2,2,3}, {3,1,1,3,3,1,1,3}, {3,2,2,3,3,2,2,3}, {3,3,3,3,3,3,3,3} }
};

void queue_vram_text(unsigned int ppu_addr, const char* text) {
    unsigned char len = 0;
    const char* t = text;
    while (*t++) len++;
    if (vram_buf_ptr + len + 3 >= VRAM_BUF_SIZE) return;
    
    vram_buffer[vram_buf_ptr++] = (ppu_addr >> 8) & 0xFF;
    vram_buffer[vram_buf_ptr++] = ppu_addr & 0xFF;
    vram_buffer[vram_buf_ptr++] = len;
    while (*text) {
        if (*text >= '0' && *text <= '9')      vram_buffer[vram_buf_ptr++] = 0x80 + (*text - '0');
        else if (*text >= 'A' && *text <= 'Z') vram_buffer[vram_buf_ptr++] = 0x40 + (*text - 'A');
        else if (*text == '>')                 vram_buffer[vram_buf_ptr++] = 0x3E;
        else if (*text == ':')                 vram_buffer[vram_buf_ptr++] = 0x1A;
        else if (*text == '!')                 vram_buffer[vram_buf_ptr++] = 0x21;
        else                                   vram_buffer[vram_buf_ptr++] = 0x00;
        text++;
    }
}

void init_game(void) {
    current_state = STATE_MENU;
    menu_selection = 0;
    final_winner = 0;
    vram_buf_ptr = 0;
}

void load_player_level(unsigned char p_idx, unsigned char lvl) {
    unsigned char r, c;
    remaining_bricks[p_idx] = 0;
    current_level[p_idx] = lvl;
    for (r = 0; r < BRICK_ROWS; ++r) {
        for (c = 0; c < BRICK_COLS; ++c) {
            brick_grid[p_idx][r][c] = level_layouts[lvl - 1][r][c];
            if (brick_grid[p_idx][r][c] > 0) remaining_bricks[p_idx]++;
        }
    }
    if (game_mode == MODE_1PLAYER) {
        paddle_x[p_idx] = (SCREEN_W_UNITS / 2) - 3072;
        ball_x[p_idx] = SCREEN_W_UNITS / 2;
    } else {
        if (p_idx == 0) {
            paddle_x[0] = (SPLIT_W_UNITS / 2) - 3072;
            ball_x[0] = SPLIT_W_UNITS / 2;
        } else {
            paddle_x[1] = SPLIT_W_UNITS + (SPLIT_W_UNITS / 2) - 3072;
            ball_x[1] = SPLIT_W_UNITS + (SPLIT_W_UNITS / 2);
        }
    }
    ball_y[p_idx] = SCREEN_H_UNITS - 6144;
    ball_z[p_idx] = 0;
    ball_vx[p_idx] = 200 + (lvl * 20);
    ball_vy[p_idx] = -(200 + (lvl * 20));
}

void evaluate_endgame(void) {
    current_state = STATE_GAMEOVER;
    if (score[0] > score[1])      final_winner = 1;
    else if (score[1] > score[0]) final_winner = 2;
    else                          final_winner = 3;
}

void update_game(unsigned char p1_pad, unsigned char p2_pad) {
    static unsigned char last_p1_pad = 0;
    unsigned char p1_pressed = p1_pad & ~last_p1_pad;
    last_p1_pad = p1_pad;
    unsigned char p;

    if (current_state == STATE_MENU) {
        if (p1_pressed & 0x08) menu_selection = 0;
        if (p1_pressed & 0x04) menu_selection = 1;
        if (p1_pressed & 0x10) {
            game_mode = menu_selection;
            score[0] = 0; score[1] = 0;
            strikes[0] = 0; strikes[1] = 0;
            player_active[0] = 1;
            if (game_mode == MODE_1PLAYER) {
                player_active[1] = 0;
                load_player_level(0, 1);
            } else {
                player_active[1] = 1;
                load_player_level(0, 1);
                load_player_level(1, 1);
                match_frames_left = 18000; 
            }
            final_winner = 0;
            current_state = STATE_PLAYING;
        }
        return;
    }

    if (p1_pressed & 0x10) {
        if (current_state == STATE_PLAYING)      current_state = STATE_PAUSED;
        else if (current_state == STATE_PAUSED) current_state = STATE_PLAYING;
        return;
    }

    if (current_state != STATE_PLAYING) return;

    if (game_mode == MODE_2PLAYER) {
        if (match_frames_left > 0) {
            match_frames_left--;
            if (match_frames_left == 0) {
                evaluate_endgame();
                return;
            }
        }
    }

    for (p = 0; p < 2; ++p) {
        if (!player_active[p]) continue;

        unsigned char current_pad = (p == 0) ? p1_pad : p2_pad;
        int min_x = (game_mode == MODE_1PLAYER) ? 2048 : (p * SPLIT_W_UNITS) + 1024;
        int max_x = (game_mode == MODE_1PLAYER) ? SCREEN_W_UNITS - 8192 : ((p + 1) * SPLIT_W_UNITS) - 7168;

        if (current_pad & 0x02) { 
            paddle_x[p] -= 384; 
            if (paddle_x[p] < min_x) paddle_x[p] = min_x;
        }
        if (current_pad & 0x01) { 
            paddle_x[p] += 384; 
            if (paddle_x[p] > max_x) paddle_x[p] = max_x;
        }

        ball_x[p] += ball_vx[p];
        ball_y[p] += ball_vy[p];

        if (ball_x[p] < min_x)            { ball_x[p] = min_x; ball_vx[p] = -ball_vx[p]; }
        if (ball_x[p] > max_x + 6144)     { ball_x[p] = max_x + 6144; ball_vx[p] = -ball_vx[p]; }
        if (ball_y[p] < 4096)            { ball_y[p] = 4096; ball_vy[p] = -ball_vy[p]; }

        if (ball_y[p] > SCREEN_H_UNITS) {
            strikes[p]++;
            if (strikes[p] >= 3) {
                player_active[p] = 0;
                if (game_mode == MODE_1PLAYER) {
                    current_state = STATE_GAMEOVER;
                    return;
                } else {
                    if (player_active[0] == 0 && player_active[1] == 0) {
                        evaluate_endgame();
                    } else if (player_active[0] == 0) {
                        final_winner = 2;
                        current_state = STATE_GAMEOVER;
                    } else if (player_active[1] == 0) {
                        final_winner = 1;
                        current_state = STATE_GAMEOVER;
                    }
                    return;
                }
            } else {
                load_player_level(p, current_level[p]);
            }
            continue;
        }

        if (ball_y[p] >= SCREEN_H_UNITS - 4096 && ball_y[p] <= SCREEN_H_UNITS - 3072) {
            if (ball_x[p] >= paddle_x[p] && ball_x[p] <= paddle_x[p] + 8192) {
                int offset = (ball_x[p] - (paddle_x[p] + 4096)) / 32;
                ball_vx[p] = offset * 2;
                ball_vy[p] = -ball_vy[p];
                ball_y[p] = SCREEN_H_UNITS - 4100;
            }
        }

        {
            int local_x = (game_mode == MODE_1PLAYER) ? (ball_x[p] - 4096) : (ball_x[p] - (p * SPLIT_W_UNITS) - 2048);
            unsigned char grid_x = local_x / (BRICK_W_UNITS / (game_mode + 1));
            unsigned char grid_y = (ball_y[p] - 6144) / BRICK_H_UNITS;

            if (grid_x < BRICK_COLS && grid_y < BRICK_ROWS) {
                if (brick_grid[p][grid_y][grid_x] > 0) {
                    brick_grid[p][grid_y][grid_x] = 0;
                    ball_vy[p] = -ball_vy[p];
                    score[p] += 10;
                    remaining_bricks[p]--;
                    if (remaining_bricks[p] == 0) {
                        if (current_level[p] < 5) load_player_level(p, current_level[p] + 1);
                        else                      load_player_level(p, 1);
                    }
                }
            }
        }
    }
}