#define LIDRM_IMPL
#include <lidrm.h>
#undef LIDRM_IMPL

#include <miolk.h>
#include <stdlib.h>
#include <string.h>

bool miolk_init(miolk_ctx_t *ctx, const char *device_path) {
    if (!ctx) return false;
    memset(ctx, 0, sizeof(miolk_ctx_t));

    const char *dev = device_path ? device_path : "/dev/dri/card0";

    if (!lidrm_init(&ctx->drm, dev)) {
        return false;
    }

    if (!lidrm_set_mode(&ctx->drm)) {
        lidrm_cleanup(&ctx->drm);
        return false;
    }

    ctx->width = ctx->drm.width;
    ctx->height = ctx->drm.height;

    size_t buffer_size = (size_t)ctx->width * ctx->height * sizeof(uint32_t);
    ctx->back_buffer = (uint32_t *)malloc(buffer_size);
    if (!ctx->back_buffer) {
        lidrm_cleanup(&ctx->drm);
        return false;
    }

    miolk_clear(ctx, 0x00000000);
    return true;
}

void miolk_cleanup(miolk_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->back_buffer) {
        free(ctx->back_buffer);
        ctx->back_buffer = NULL;
    }
    lidrm_cleanup(&ctx->drm);
}

void miolk_get_size(const miolk_ctx_t *ctx, uint32_t *width, uint32_t *height) {
    if (!ctx) return;
    if (width) *width = ctx->width;
    if (height) *height = ctx->height;
}

void miolk_clear(miolk_ctx_t *ctx, uint32_t color) {
    if (!ctx || !ctx->back_buffer) return;
    size_t count = (size_t)ctx->width * ctx->height;
    for (size_t i = 0; i < count; i++) {
        ctx->back_buffer[i] = color;
    }
}

void miolk_put_pixel(miolk_ctx_t *ctx, uint32_t x, uint32_t y, uint32_t color) {
    if (!ctx || !ctx->back_buffer || x >= ctx->width || y >= ctx->height) return;
    ctx->back_buffer[y * ctx->width + x] = color;
}

void miolk_draw_rect(miolk_ctx_t *ctx, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!ctx) return;
    uint32_t max_x = (x + w > ctx->width) ? ctx->width : x + w;
    uint32_t max_y = (y + h > ctx->height) ? ctx->height : y + h;

    for (uint32_t py = y; py < max_y; py++) {
        for (uint32_t px = x; px < max_x; px++) {
            ctx->back_buffer[py * ctx->width + px] = color;
        }
    }
}

void miolk_load_buffer(miolk_ctx_t *ctx, const uint32_t *src_pixels, uint32_t src_w, uint32_t src_h) {
    if (!ctx || !ctx->back_buffer || !src_pixels) return;

    uint32_t copy_w = (src_w < ctx->width) ? src_w : ctx->width;
    uint32_t copy_h = (src_h < ctx->height) ? src_h : ctx->height;

    for (uint32_t y = 0; y < copy_h; y++) {
        memcpy(&ctx->back_buffer[y * ctx->width], 
               &src_pixels[y * src_w], 
               copy_w * sizeof(uint32_t));
    }
}

void miolk_present(miolk_ctx_t *ctx) {
    if (!ctx || !ctx->back_buffer || !ctx->drm.pixels) return;

    size_t bytes = (size_t)ctx->width * ctx->height * sizeof(uint32_t);
    memcpy(ctx->drm.pixels, ctx->back_buffer, bytes);

    lidrm_flush(&ctx->drm);
}
