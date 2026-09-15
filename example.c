#include <miolk.h>
#include <unistd.h>
#include <stdlib.h>

int main(void) {
    miolk_t miolk;

    if (!miolk_init(&miolk, "/dev/fb0")) {
        return 1;
    }

    uint32_t w, h;
    miolk_get_size(&miolk, &w, &h);

    miolk_clear(&miolk, 0x00112244);

    for (uint32_t i = 0; i < 200 && i < w && i < h; i++) {
        miolk_put_pixel(&miolk, 50 + i, 50 + i, 0x00FF0000);
    }

    miolk_draw_rect(&miolk, w / 2 - 50, h / 2 - 50, 100, 100, 0x0000FF00);

    uint32_t *ram_buffer = (uint32_t *)calloc(150 * 150, sizeof(uint32_t));
    for (int i = 0; i < 150 * 150; i++) ram_buffer[i] = 0x00FFFF00;

    miolk_load_buffer(&miolk, ram_buffer, 150, 150);
    free(ram_buffer);

    miolk_present(&miolk);

    sleep(3);

    miolk_cleanup(&miolk);
    return 0;
}
