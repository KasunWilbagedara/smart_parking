#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "parking_system.h"

void app_main(void) {
    // 1. Setup I2C
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = 0, .sda_io_num = I2C_SDA_IO, .scl_io_num = I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    i2c_new_master_bus(&bus_cfg, &bus_handle);
    i2c_device_config_t dev_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = LCD_ADDR, .scl_speed_hz = 100000 };
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

    // 2. Setup Hardware
    lcd_init(dev_handle);
    gpio_reset_pin(TRIG_PIN_1); gpio_set_direction(TRIG_PIN_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_PIN_1); gpio_set_direction(ECHO_PIN_1, GPIO_MODE_INPUT);
    gpio_reset_pin(LED_SLOT_1); gpio_set_direction(LED_SLOT_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BUZZER_PIN); gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    ledc_timer_config_t led_tmr = { .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = 0, .duty_resolution = 13, .freq_hz = 50, .clk_cfg = LEDC_AUTO_CLK };
    ledc_timer_config(&led_tmr);
    ledc_channel_config_t led_ch = { .speed_mode = LEDC_LOW_SPEED_MODE, .channel = 0, .timer_sel = 0, .gpio_num = SERVO_PIN, .duty = 0 };
    ledc_channel_config(&led_ch);

    // 3. Super Loop
    while (1) {
        int dist = get_distance(TRIG_PIN_1, ECHO_PIN_1);
        bool full = (dist > 0 && dist < 10);

        gpio_set_level(LED_SLOT_1, full);
        lcd_put_cursor(dev_handle, 0, 0);

        if (full) {
            lcd_send_string(dev_handle, " PARK IS FULL   ");
            lcd_put_cursor(dev_handle, 1, 0);
            lcd_send_string(dev_handle, " Gate: CLOSED   ");
            gpio_set_level(BUZZER_PIN, 1);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, 0, 400); // 0 degrees
        } else {
            lcd_send_string(dev_handle, " Slots Avail: 1 ");
            lcd_put_cursor(dev_handle, 1, 0);
            lcd_send_string(dev_handle, " Gate: OPEN     ");
            gpio_set_level(BUZZER_PIN, 0);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, 0, 800); // 90 degrees
        }
        ledc_update_duty(LEDC_LOW_SPEED_MODE, 0);

        vTaskDelay(pdMS_TO_TICKS(500)); // Non-blocking sleep for WDT and stability
    }
}