#include "../../core/game.h"
#include "sprites_data.h"

#define PPU_CTRL   ((volatile unsigned char*)0x2000)
#define PPU_MASK   ((volatile unsigned char*)0x2001)
#define PPU_STATUS ((volatile unsigned char*)0x2002)
#define PPU_ADDR   ((volatile unsigned char*)0x2006)
#define PPU_DATA   ((volatile unsigned char*)0x2007)
#define OAM_DMA    ((volatile unsigned char*)0x4014)
#define JOYPAD1    ((volatile unsigned char*)0x4016)
#define JOYPAD2    ((volatile unsigned char*)0x4017)

unsigned char sprite_buffer[256];
volatile unsigned char nmi_flag = 0;
unsigned char flash_frame_counter = 0;

const unsigned char palette_data[16] = {
    0x0F, 0x16, 0x27, 0x30, 
    0x0F, 0x01, 0x11, 0x21, 
    0x0F, 0x1A, 0x2A, 0x3A, 
    0x0F, 0x12, 0x22, 0x32  
};

void system_init(void) {
    unsigned int i;
    *PPU_CTRL = 0; *PPU_MASK = 0;
    *PPU_ADDR = 0x00; *PPU_ADDR = 0x00;
    for (i = 0; i < 128; ++i) *PPU_DATA = chr_sprites_sheet[i];
    *PPU_ADDR = 0x08; *PPU_ADDR = 0x00;
    for (i = 0; i < 160; ++i) *PPU_DATA = chr_font_sheet[i];
    *PPU_ADDR = 0x3F; *PPU_ADDR = 0x00;
    for (i = 0; i < 16; ++i)  *PPU_DATA = palette_data[i];
    *PPU_CTRL = 0x80; *PPU_MASK = 0x1E;
}

void flush_vram_buffer(void) {
    unsigned char i = 0, count, j;
    unsigned int target_addr;
    while (i < vram_buf_ptr) {
        target_addr = (vram_buffer[i++] << 8);
        target_addr |= vram_buffer[i++];
        count = vram_buffer[i++];
        *PPU_ADDR = (target_addr >> 8) & 0xFF;
        *PPU_ADDR = target_addr & 0xFF;
        for (j = 0; j < count; ++j) {
            *PPU_DATA = vram_buffer[i++];
        }
    }
    vram_buf_ptr = 0;
}

unsigned char read_input(unsigned char port) {
    unsigned char state = 0, i;
    volatile unsigned char* pad_addr = (port == 0) ? JOYPAD1 : JOYPAD2;
    *pad_addr = 1; *pad_addr = 0;
    for (i = 0; i < 8; ++i) {
        state = (state << 1) | (*pad_addr & 1);
    }
    return state;
}

void clear_sprites(void) {
    unsigned char i;
    for (i = 0; i < 64; ++i) sprite_buffer[i * 4] = 240;
}

void update_sprites(void) {
    clear_sprites();
    if (current_state == STATE_PLAYING || current_state == STATE_PAUSED) {
        unsigned char p, s_offset = 0;
        // OAM Shuffling trick to prevent 8-sprite per-scanline invisible dropouts
        unsigned char start_p = (flash_frame_counter & 1);
        for (p = start_p; p < start_p + 2; ++p) {
            unsigned char idx = p % 2;
            if (!player_active[idx]) continue;

            sprite_buffer[s_offset + 0] = (ball_y[idx] >> 8);
            sprite_buffer[s_offset + 1] = 0x01;
            sprite_buffer[s_offset + 2] = (idx == 0) ? 0x00 : 0x01;
            sprite_buffer[s_offset + 3] = (ball_x[idx] >> 8);
            s_offset += 4;

            sprite_buffer[s_offset + 0] = (SCREEN_H_UNITS >> 8) - 4;
            sprite_buffer[s_offset + 1] = 0x02;
            sprite_buffer[s_offset + 2] = 0x00;
            sprite_buffer[s_offset + 3] = (paddle_x[idx] >> 8);
            s_offset += 4;

            sprite_buffer[s_offset + 0] = (SCREEN_H_UNITS >> 8) - 4;
            sprite_buffer[s_offset + 1] = 0x03;
            sprite_buffer[s_offset + 2] = 0x00;
            sprite_buffer[s_offset + 3] = (paddle_x[idx] >> 8) + 8;
            s_offset += 4;

            sprite_buffer[s_offset + 0] = (SCREEN_H_UNITS >> 8) - 4;
            sprite_buffer[s_offset + 1] = 0x04;
            sprite_buffer[s_offset + 2] = 0x00;
            sprite_buffer[s_offset + 3] = (paddle_x[idx] >> 8) + 16;
            s_offset += 4;
        }
    }
}

void main(void) {
    init_game();
    system_init();

    while (1) {
        while (!nmi_flag);
        nmi_flag = 0;
        flash_frame_counter++;

        update_game(read_input(0), read_input(1));
        update_sprites();

        if (vram_buf_ptr > 0) {
            flush_vram_buffer();
        }

        *OAM_DMA = ((unsigned int)sprite_buffer >> 8);
    }
}

void nmi_handler(void) {
    nmi_flag = 1;
}