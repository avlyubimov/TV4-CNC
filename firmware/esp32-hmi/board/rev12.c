/* SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 * Display-only bench adapter. Pin/timing sources:
 * Waveshare Rev 1.2 schematic and official 09_lvgl_v9_demo at
 * bc51a84b30c0cea018fee6e0d7427ec10102060e.
 * RGB setup derived from vendor CC0-1.0 / Espressif example (2022).
 * No CAN, GPIO19/20 configuration, or motion outputs. */
#include "rev12.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
static const char *TAG="rev12";
static esp_lcd_touch_handle_t touch;
static StaticQueue_t queue_state;
static uint8_t queue_storage[sizeof(board_touch_t)];
static QueueHandle_t samples;
static uint8_t outputs=0x1a; /* SD CS high, LCD RST high, touch RST high, USB_SEL low. */
static esp_err_t expander(uint8_t addr,uint8_t value) {
    return i2c_master_write_to_device(I2C_NUM_0,addr,&value,1,pdMS_TO_TICKS(20));
}
esp_err_t board_rev12_backlight(bool on) {
    uint8_t next=(outputs & ~0x24u) | (on?0x04u:0); /* Always keep EXIO5 low: USB. */
    esp_err_t err=expander(0x38,next);
    if(err==ESP_OK) outputs=next;
    return err;
}
static void poll_touch(void *arg) {
    (void)arg;
    for(;;) {
        board_touch_t sample={.ms=(uint32_t)(esp_timer_get_time()/1000)};
        sample.valid=esp_lcd_touch_read_data(touch)==ESP_OK;
        uint8_t count=0; esp_lcd_touch_point_data_t point={0};
        if(sample.valid) {
            sample.valid=esp_lcd_touch_get_data(touch,&point,&count,1)==ESP_OK;
            sample.pressed=sample.valid && count>0;
            if(sample.pressed) { sample.x=point.x; sample.y=point.y; }
        }
        if(sample.x>=800 || sample.y>=480) { sample.valid=false; sample.pressed=false; }
        xQueueOverwrite(samples,&sample);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
bool board_rev12_touch(board_touch_t *sample) {
    return samples && xQueuePeek(samples,sample,0)==pdTRUE;
}
esp_err_t board_rev12_init(esp_lcd_panel_handle_t *panel) {
    if(!panel) return ESP_ERR_INVALID_ARG;
    *panel=NULL;
    i2c_config_t bus={.mode=I2C_MODE_MASTER,.sda_io_num=8,.scl_io_num=9,
        .sda_pullup_en=GPIO_PULLUP_ENABLE,.scl_pullup_en=GPIO_PULLUP_ENABLE,
        .master.clk_speed=400000};
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_NUM_0,&bus),TAG,"I2C config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_NUM_0,bus.mode,0,0,0),TAG,"I2C install");
    /* Set latch before output mode: SD remains deselected; backlight stays off.
       Unlike vendor 0x2c/0x2e sequence, USB_SEL never goes high (CAN mode). */
    ESP_RETURN_ON_ERROR(expander(0x38,0x18),TAG,"CH422G latch");
    ESP_RETURN_ON_ERROR(expander(0x24,0x01),TAG,"CH422G mode");
    gpio_config_t int_pin={.pin_bit_mask=1ULL<<4,.mode=GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&int_pin),TAG,"GT911 address pin");
    ESP_RETURN_ON_ERROR(gpio_set_level(4,0),TAG,"GT911 address low");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(expander(0x38,outputs),TAG,"GT911 reset release");
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_RETURN_ON_ERROR(gpio_set_direction(4,GPIO_MODE_INPUT),TAG,"GT911 INT release");
    esp_lcd_rgb_panel_config_t rgb={
        .clk_src=LCD_CLK_SRC_DEFAULT,
        .timings={.pclk_hz=16000000,.h_res=800,.v_res=480,
            .hsync_pulse_width=4,.hsync_back_porch=8,.hsync_front_porch=8,
            .vsync_pulse_width=4,.vsync_back_porch=8,.vsync_front_porch=8,
            .flags.pclk_active_neg=1},
        .data_width=16,.bits_per_pixel=16,.num_fbs=1,
        .bounce_buffer_size_px=800*10, /* API counts pixels, not bytes. */
        .sram_trans_align=4,.psram_trans_align=64,
        .hsync_gpio_num=46,.vsync_gpio_num=3,.de_gpio_num=5,.pclk_gpio_num=7,.disp_gpio_num=-1,
        .data_gpio_nums={14,38,18,17,10,39,0,45,48,47,21,1,2,42,41,40},
        .flags.fb_in_psram=1};
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&rgb,panel),TAG,"RGB create");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel),TAG,"RGB init");
    esp_lcd_panel_io_i2c_config_t io_cfg=ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz=0; /* Legacy I2C master: clock configured above. */
    esp_lcd_panel_io_handle_t io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0,&io_cfg,&io),TAG,"Touch I/O");
    static esp_lcd_touch_io_gt911_config_t gt911; gt911.dev_addr=io_cfg.dev_addr;
    esp_lcd_touch_config_t config={.x_max=800,.y_max=480,.rst_gpio_num=-1,.int_gpio_num=-1,.driver_data=&gt911};
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(io,&config,&touch),TAG,"GT911 create");
    samples=xQueueCreateStatic(1,sizeof(board_touch_t),queue_storage,&queue_state);
    if(!samples || xTaskCreate(poll_touch,"tv4_touch",4096,NULL,3,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
