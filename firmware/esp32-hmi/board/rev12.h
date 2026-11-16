#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
typedef struct { uint16_t x,y; bool pressed,valid; uint32_t ms; } board_touch_t;
esp_err_t board_rev12_init(esp_lcd_panel_handle_t *panel);
esp_err_t board_rev12_backlight(bool on);
/* Bounded queue snapshot, never performs I2C from the UI task. */
bool board_rev12_touch(board_touch_t *sample);
