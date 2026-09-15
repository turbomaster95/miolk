#ifndef MIOLK_H
#define MIOLK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <lidrm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lidrm_t drm;
    uint32_t width;
    uint32_t height;
    uint32_t *back_buffer; // RAM double buffer to prevent tearing
} miolk_ctx_t;

// Initialization & Cleanup
bool miolk_init(miolk_ctx_t *ctx, const char *device_path);
void miolk_cleanup(miolk_ctx_t *ctx);

// Screen Info
void miolk_get_size(const miolk_ctx_t *ctx, uint32_t *width, uint32_t *height);

// Drawing Operations
void miolk_clear(miolk_ctx_t *ctx, uint32_t color);
void miolk_put_pixel(miolk_ctx_t *ctx, uint32_t x, uint32_t y, uint32_t color);
void miolk_draw_rect(miolk_ctx_t *ctx, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

// Buffer Copy & Display
void miolk_load_buffer(miolk_ctx_t *ctx, const uint32_t *src_pixels, uint32_t src_w, uint32_t src_h);
void miolk_present(miolk_ctx_t *ctx); // Swaps RAM back buffer to DRM memory & flushes display

#ifdef __cplusplus
}
#endif

#endif // MIOLK_H
