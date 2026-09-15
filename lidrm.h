#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <drm/drm_mode.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIDRM_H
typedef struct {
    int fd;
    uint32_t connector_id;
    uint32_t crtc_id;
    uint32_t width;
    uint32_t height;

    // Active mode timings
    struct drm_mode_modeinfo mode;

    // Framebuffer state
    uint32_t fb_id;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    uint32_t *pixels; // ARGB8888 / XRGB8888 mapped buffer

    uint32_t saved_fb_id;
    uint32_t saved_x, saved_y;
    uint32_t saved_mode_valid;
    uint8_t saved_mode_blob[512];
} lidrm_t;
#endif

#define LIDRM_H

bool lidrm_init(lidrm_t *drm, const char *device_path);
bool lidrm_set_mode(lidrm_t *drm);
void lidrm_clear(lidrm_t *drm, uint32_t color);
void lidrm_cleanup(lidrm_t *drm);
void lidrm_flush(lidrm_t *drm);

#ifdef LIDRM_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#ifndef DRM_MODE_CONNECTED
#define DRM_MODE_CONNECTED 1
#endif
#ifndef DRM_MODE_UNKNOWNCONNECTION
#define DRM_MODE_UNKNOWNCONNECTION 3
#endif

bool lidrm_init(lidrm_t *drm, const char *device_path) {
    if (!drm) return false;
    memset(drm, 0, sizeof(lidrm_t));
    drm->fd = -1;

    const char *path = device_path ? device_path : "/dev/dri/card0";
    drm->fd = open(path, O_RDWR | O_CLOEXEC);
    if (drm->fd < 0) {
        perror("[lidrm] Failed to open DRM device");
        return false;
    }

    ioctl(drm->fd, DRM_IOCTL_SET_MASTER, 0);

    struct drm_mode_card_res res = {0};
    if (ioctl(drm->fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
        perror("[lidrm] GETRESOURCES failed");
        close(drm->fd);
        drm->fd = -1;
        return false;
    }

    uint32_t *conn_ids = res.count_connectors ? calloc(res.count_connectors, sizeof(uint32_t)) : NULL;
    uint32_t *crtc_ids = res.count_crtcs ? calloc(res.count_crtcs, sizeof(uint32_t)) : NULL;
    uint32_t *enc_ids  = res.count_encoders ? calloc(res.count_encoders, sizeof(uint32_t)) : NULL;
    uint32_t *fb_ids   = res.count_fbs ? calloc(res.count_fbs, sizeof(uint32_t)) : NULL;

    res.connector_id_ptr = (uint64_t)(uintptr_t)conn_ids;
    res.crtc_id_ptr      = (uint64_t)(uintptr_t)crtc_ids;
    res.encoder_id_ptr   = (uint64_t)(uintptr_t)enc_ids;
    res.fb_id_ptr        = (uint64_t)(uintptr_t)fb_ids;

    if (ioctl(drm->fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
        perror("[lidrm] GETRESOURCES populated query failed");
        free(conn_ids); free(crtc_ids); free(enc_ids); free(fb_ids);
        close(drm->fd);
        drm->fd = -1;
        return false;
    }

    free(enc_ids);
    free(fb_ids);

    struct drm_mode_modeinfo selected_mode = {0};
    bool found_connector = false;

    for (uint32_t i = 0; i < res.count_connectors; i++) {
        struct drm_mode_get_connector conn = {
            .connector_id = conn_ids[i]
        };

        if (ioctl(drm->fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) continue;

        if (conn.connection == DRM_MODE_CONNECTED || conn.connection == DRM_MODE_UNKNOWNCONNECTION) {
            if (conn.count_modes > 0) {
                struct drm_mode_modeinfo *modes = calloc(conn.count_modes, sizeof(struct drm_mode_modeinfo));
                
                struct drm_mode_get_connector conn_modes = {
                    .connector_id = conn_ids[i],
                    .modes_ptr = (uint64_t)(uintptr_t)modes,
                    .count_modes = conn.count_modes,
                    .count_props = 0,
                    .count_encoders = 0
                };

                if (ioctl(drm->fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn_modes) == 0) {
                    selected_mode = modes[0];
                    drm->connector_id = conn.connector_id;
                    drm->width = selected_mode.hdisplay;
                    drm->height = selected_mode.vdisplay;
                    found_connector = true;
                }
                free(modes);
            } else {
                selected_mode = (struct drm_mode_modeinfo){
                    .clock = 65000,
                    .hdisplay = 1024, .hsync_start = 1048, .hsync_end = 1184, .htotal = 1344,
                    .vdisplay = 768,  .vsync_start = 771,  .vsync_end = 777,  .vtotal = 806,
                    .vrefresh = 60,
                    .name = "1024x768"
                };
                drm->connector_id = conn.connector_id;
                drm->width = 1024;
                drm->height = 768;
                found_connector = true;
            }

            if (found_connector) break;
        }
    }

    if (!found_connector && res.count_connectors > 0) {
        drm->connector_id = conn_ids[0];
        selected_mode = (struct drm_mode_modeinfo){
            .clock = 65000,
            .hdisplay = 1024, .hsync_start = 1048, .hsync_end = 1184, .htotal = 1344,
            .vdisplay = 768,  .vsync_start = 771,  .vsync_end = 777,  .vtotal = 806,
            .vrefresh = 60,
            .name = "1024x768"
        };
        drm->width = 1024;
        drm->height = 768;
        found_connector = true;
    }

    drm->mode = selected_mode;

    if (found_connector && res.count_crtcs > 0) {
        uint32_t chosen_crtc = 0;
        struct drm_mode_get_connector conn_enc = { .connector_id = drm->connector_id };
        if (ioctl(drm->fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn_enc) == 0 && conn_enc.encoder_id > 0) {
            struct drm_mode_get_encoder enc = { .encoder_id = conn_enc.encoder_id };
            if (ioctl(drm->fd, DRM_IOCTL_MODE_GETENCODER, &enc) == 0) {
                for (uint32_t c = 0; c < res.count_crtcs; c++) {
                    if (enc.possible_crtcs & (1U << c)) {
                        chosen_crtc = crtc_ids[c];
                        break;
                    }
                }
            }
        }
        if (chosen_crtc == 0) {
            chosen_crtc = crtc_ids[0];
        }
        drm->crtc_id = chosen_crtc;
    }

    free(conn_ids);
    free(crtc_ids);

    if (!found_connector) {
        fprintf(stderr, "[lidrm] No active display connector found on device\n");
        close(drm->fd);
        drm->fd = -1;
        return false;
    }

    struct drm_mode_crtc saved_crtc = { .crtc_id = drm->crtc_id };
    if (ioctl(drm->fd, DRM_IOCTL_MODE_GETCRTC, &saved_crtc) == 0) {
        drm->saved_fb_id = saved_crtc.fb_id;
        drm->saved_x = saved_crtc.x;
        drm->saved_y = saved_crtc.y;
        drm->saved_mode_valid = saved_crtc.mode_valid;
        if (saved_crtc.mode_valid) {
            memcpy(drm->saved_mode_blob, &saved_crtc.mode, sizeof(struct drm_mode_modeinfo));
        }
    }

    struct drm_mode_create_dumb creq = {
        .width = drm->width,
        .height = drm->height,
        .bpp = 32
    };

    if (ioctl(drm->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        perror("[lidrm] CREATE_DUMB failed");
        close(drm->fd);
        drm->fd = -1;
        return false;
    }

    drm->handle = creq.handle;
    drm->pitch = creq.pitch;
    drm->size = creq.size;

    struct drm_mode_fb_cmd cmd = {
        .width = drm->width,
        .height = drm->height,
        .pitch = drm->pitch,
        .bpp = 32,
        .depth = 24,
        .handle = drm->handle
    };

    if (ioctl(drm->fd, DRM_IOCTL_MODE_ADDFB, &cmd) < 0) {
        perror("[lidrm] ADDFB failed");
        goto err_destroy_dumb;
    }
    drm->fb_id = cmd.fb_id;

    struct drm_mode_map_dumb mreq = { .handle = drm->handle };
    if (ioctl(drm->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
        perror("[lidrm] MAP_DUMB failed");
        goto err_rm_fb;
    }

    drm->pixels = (uint32_t *)mmap(0, drm->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm->fd, mreq.offset);
    if (drm->pixels == MAP_FAILED) {
        perror("[lidrm] mmap failed");
        drm->pixels = NULL;
        goto err_rm_fb;
    }

    // Initial modeset
    if (!lidrm_set_mode(drm)) {
        goto err_unmap;
    }

    printf("[lidrm] Initialized display %ux%u on CRTC %u (Connector %u)\n", 
           drm->width, drm->height, drm->crtc_id, drm->connector_id);
    return true;

err_unmap:
    munmap(drm->pixels, drm->size);
    drm->pixels = NULL;
err_rm_fb:
    ioctl(drm->fd, DRM_IOCTL_MODE_RMFB, &drm->fb_id);
err_destroy_dumb:
    {
        struct drm_mode_destroy_dumb dreq = { .handle = drm->handle };
        ioctl(drm->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    }
    close(drm->fd);
    drm->fd = -1;
    return false;
}

void lidrm_clear(lidrm_t *drm, uint32_t color) {
    if (!drm || !drm->pixels || drm->pixels == MAP_FAILED) return;
    size_t count = (size_t)(drm->size / sizeof(uint32_t));
    for (size_t i = 0; i < count; i++) {
        drm->pixels[i] = color;
    }
}

bool lidrm_set_mode(lidrm_t *drm) {
    if (!drm || drm->fd < 0 || !drm->fb_id || !drm->crtc_id) {
        return false;
    }

    struct drm_mode_crtc crtc = {
        .crtc_id = drm->crtc_id,
        .fb_id = drm->fb_id,
        .set_connectors_ptr = (uint64_t)(uintptr_t)&drm->connector_id,
        .count_connectors = 1,
        .mode = drm->mode,
        .mode_valid = 1
    };

    if (ioctl(drm->fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
        perror("[lidrm] SETCRTC failed in lidrm_set_mode");
        return false;
    }

    return true;
}

void lidrm_flush(lidrm_t *drm) {
    if (!drm || drm->fd < 0 || !drm->fb_id) return;

    struct drm_mode_fb_dirty_cmd dirty = {
        .fb_id = drm->fb_id,
        .flags = 0,
        .color = 0,
        .num_clips = 0,
        .clips_ptr = 0
    };
    ioctl(drm->fd, DRM_IOCTL_MODE_DIRTYFB, &dirty);
}

void lidrm_cleanup(lidrm_t *drm) {
    if (!drm || drm->fd < 0) return;

    if (drm->saved_mode_valid) {
        struct drm_mode_crtc crtc = {
            .crtc_id = drm->crtc_id,
            .fb_id = drm->saved_fb_id,
            .x = drm->saved_x,
            .y = drm->saved_y,
            .set_connectors_ptr = (uint64_t)(uintptr_t)&drm->connector_id,
            .count_connectors = 1,
            .mode_valid = 1
        };
        memcpy(&crtc.mode, drm->saved_mode_blob, sizeof(struct drm_mode_modeinfo));
        ioctl(drm->fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
    }

    if (drm->pixels && drm->pixels != MAP_FAILED) {
        munmap(drm->pixels, drm->size);
    }
    if (drm->fb_id) {
        ioctl(drm->fd, DRM_IOCTL_MODE_RMFB, &drm->fb_id);
    }
    if (drm->handle) {
        struct drm_mode_destroy_dumb dreq = { .handle = drm->handle };
        ioctl(drm->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    }

    ioctl(drm->fd, DRM_IOCTL_DROP_MASTER, 0);
    close(drm->fd);
    drm->fd = -1;
    memset(drm, 0, sizeof(lidrm_t));
    drm->fd = -1;
}

#endif // LIDRM_IMPL

#ifdef __cplusplus
}
#endif
