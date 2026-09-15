#ifndef MIOLK_H
#define MIOLK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;          // Bytes per line
    size_t screensize;       // Total sz in bytes of FB memory
    uint8_t *fb_pixels;      // mmap'd framebuffer memory
    uint32_t *back_buffer;   // ram buffer (32-bit XRGB8888)
} miolk_t;

bool miolk_init(miolk_t *ctx, const char *device_path);
void miolk_cleanup(miolk_t *ctx);

void miolk_get_size(const miolk_t *ctx, uint32_t *width, uint32_t *height);

void miolk_clear(miolk_t *ctx, uint32_t color);
void miolk_put_pixel(miolk_t *ctx, uint32_t x, uint32_t y, uint32_t color);
void miolk_draw_rect(miolk_t *ctx, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

void miolk_load_buffer(miolk_t *ctx, const uint32_t *src_pixels, uint32_t src_w, uint32_t src_h);
void miolk_present(miolk_t *ctx); // present it to the screen

#ifdef __cplusplus
}
#endif

#endif // MIOLK_H
