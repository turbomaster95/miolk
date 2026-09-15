#include "miolk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

bool miolk_init(miolk_t *ctx, const char *device_path) {
    if (!ctx) return false;
    memset(ctx, 0, sizeof(miolk_t));

    const char *dev = device_path ? device_path : "/dev/fb0";

    ctx->fd = open(dev, O_RDWR);
    if (ctx->fd < 0) {
        perror("Error opening framebuffer device");
        return false;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(ctx->fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("Error reading fixed screen information");
        close(ctx->fd);
        return false;
    }

    if (ioctl(ctx->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("Error reading variable screen information");
        close(ctx->fd);
        return false;
    }

    ctx->width = vinfo.xres;
    ctx->height = vinfo.yres;
    ctx->bpp = vinfo.bits_per_pixel;
    ctx->pitch = finfo.line_length;
    ctx->screensize = finfo.smem_len;

    ctx->fb_pixels = (uint8_t *)mmap(NULL, ctx->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
    if (ctx->fb_pixels == MAP_FAILED) {
        perror("Error mapping framebuffer memory");
        close(ctx->fd);
        return false;
    }

    size_t back_buffer_size = (size_t)ctx->width * ctx->height * sizeof(uint32_t);
    ctx->back_buffer = (uint32_t *)malloc(back_buffer_size);
    if (!ctx->back_buffer) {
        munmap(ctx->fb_pixels, ctx->screensize);
        close(ctx->fd);
        return false;
    }

    miolk_clear(ctx, 0x00000000);
    return true;
}

void miolk_cleanup(miolk_t *ctx) {
    if (!ctx) return;
    if (ctx->back_buffer) {
        free(ctx->back_buffer);
        ctx->back_buffer = NULL;
    }
    if (ctx->fb_pixels && ctx->fb_pixels != MAP_FAILED) {
        munmap(ctx->fb_pixels, ctx->screensize);
        ctx->fb_pixels = NULL;
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

void miolk_get_size(const miolk_t *ctx, uint32_t *width, uint32_t *height) {
    if (!ctx) return;
    if (width) *width = ctx->width;
    if (height) *height = ctx->height;
}

void miolk_clear(miolk_t *ctx, uint32_t color) {
    if (!ctx || !ctx->back_buffer) return;
    size_t total_pixels = (size_t)ctx->width * ctx->height;
    for (size_t i = 0; i < total_pixels; i++) {
        ctx->back_buffer[i] = color;
    }
}

void miolk_put_pixel(miolk_t *ctx, uint32_t x, uint32_t y, uint32_t color) {
    if (!ctx || !ctx->back_buffer || x >= ctx->width || y >= ctx->height) return;
    ctx->back_buffer[y * ctx->width + x] = color;
}

void miolk_draw_rect(miolk_t *ctx, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!ctx) return;
    uint32_t max_x = (x + w > ctx->width) ? ctx->width : x + w;
    uint32_t max_y = (y + h > ctx->height) ? ctx->height : y + h;

    for (uint32_t py = y; py < max_y; py++) {
        for (uint32_t px = x; px < max_x; px++) {
            ctx->back_buffer[py * ctx->width + px] = color;
        }
    }
}

void miolk_load_buffer(miolk_t *ctx, const uint32_t *src_pixels, uint32_t src_w, uint32_t src_h) {
    if (!ctx || !ctx->back_buffer || !src_pixels) return;

    uint32_t copy_w = (src_w < ctx->width) ? src_w : ctx->width;
    uint32_t copy_h = (src_h < ctx->height) ? src_h : ctx->height;

    for (uint32_t y = 0; y < copy_h; y++) {
        memcpy(&ctx->back_buffer[y * ctx->width],
               &src_pixels[y * src_w],
               copy_w * sizeof(uint32_t));
    }
}

void miolk_present(miolk_t *ctx) {
    if (!ctx || !ctx->back_buffer || !ctx->fb_pixels) return;

    if (ctx->bpp == 32) {
        for (uint32_t y = 0; y < ctx->height; y++) {
            memcpy(ctx->fb_pixels + (y * ctx->pitch),
                   ctx->back_buffer + (y * ctx->width),
                   ctx->width * sizeof(uint32_t));
        }
    } else if (ctx->bpp == 16) {
        for (uint32_t y = 0; y < ctx->height; y++) {
            uint16_t *dst = (uint16_t *)(ctx->fb_pixels + (y * ctx->pitch));
            const uint32_t *src = ctx->back_buffer + (y * ctx->width);
            for (uint32_t x = 0; x < ctx->width; x++) {
                uint32_t c = src[x];
                uint8_t r = (c >> 16) & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = c & 0xFF;
                dst[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
    }
}
