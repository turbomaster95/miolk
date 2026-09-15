#include <miolk.h>
#include <unistd.h>
#include <stdlib.h>

int main(void) {
    miolk_ctx_t gfx;

    if (!miolk_init(&gfx, "/dev/dri/card1")) {
        return 1;
    }

    uint32_t w, h;
    miolk_get_size(&gfx, &w, &h);

    miolk_clear(&gfx, 0x00112244);

    for (uint32_t i = 0; i < 100 && i < w && i < h; i++) {
        miolk_put_pixel(&gfx, 100 + i, 100 + i, 0x00FF0000);
    }

    miolk_draw_rect(&gfx, w / 2 - 50, h / 2 - 50, 100, 100, 0x0000FF00);

    uint32_t *ram_buffer = calloc(100 * 100, sizeof(uint32_t));
    for (int i = 0; i < 100 * 100; i++) ram_buffer[i] = 0x00FFFFFF;

    miolk_load_buffer(&gfx, ram_buffer, 100, 100);
    free(ram_buffer);

    // push to drm
    miolk_present(&gfx);

    sleep(3);

    miolk_cleanup(&gfx);
    return 0;
}
