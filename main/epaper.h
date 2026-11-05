#ifndef FC3_EPAPER_H
#define FC3_EPAPER_H
#include <stdbool.h>
#include "esp_err.h"
#include "render.h"
esp_err_t badge_epaper_setup(void);
/* Start copies the whole framebuffer into the controller before returning. */
esp_err_t badge_epaper_start(const FC3Frame *frame);
bool badge_epaper_ready(void);
esp_err_t badge_epaper_finish(void);
#endif
