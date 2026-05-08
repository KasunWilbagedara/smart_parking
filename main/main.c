#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "parking_system.h"

void app_main(void) {
    printf("Initializing Smart Gate System...\n");

    // 1. I2C Initialization for LCD
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = 0,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    // 2. Hardware Pins Initialization
    lcd_init(dev_handle);
    
    // Sensor 1 (Old: Parking Slot)
    gpio_reset_pin(TRIG_PIN_1); gpio_set_direction(TRIG_PIN_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_PIN_1); gpio_set_direction(ECHO_PIN_1, GPIO_MODE_INPUT);
    
    // Sensor 2 (New: Entry Gate)
    gpio_reset_pin(TRIG_PIN_2); gpio_set_direction(TRIG_PIN_2, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_PIN_2); gpio_set_direction(ECHO_PIN_2, GPIO_MODE_INPUT);

    // Outputs
    gpio_reset_pin(LED_SLOT_1); gpio_set_direction(LED_SLOT_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BUZZER_PIN); gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    // 3. Servo PWM Configuration (50Hz)
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = SERVO_PIN,
        .duty = 0
    };
    ledc_channel_config(&ledc_channel);

    // Splash Screen
    lcd_clear(dev_handle);
    lcd_put_cursor(dev_handle, 0, 0);
    lcd_send_string(dev_handle, "GATE SYSTEM ACTV");
    vTaskDelay(pdMS_TO_TICKS(1500));

    // 4. MAIN SUPER LOOP
    while (1) {
        // --- A. DATA ACQUISITION ---
        // Read Slot Sensor (5, 18)
        int slot_dist = get_distance(TRIG_PIN_1, ECHO_PIN_1);
        vTaskDelay(pdMS_TO_TICKS(50)); 
        
        // Read Entry Sensor (10, 11)
        int entry_dist = get_distance(TRIG_PIN_2, ECHO_PIN_2);

        // --- B. LOGIC EVALUATION ---
        bool slot_occupied = (slot_dist > 0 && slot_dist < 10);
        bool car_waiting   = (entry_dist > 0 && entry_dist < 10);

        // Logic Rule: Open ONLY if car is waiting AND slot is empty.
        bool open_gate = car_waiting && !slot_occupied;

        // --- C. ACTUATOR CONTROL ---
        if (open_gate) {
            // OPEN GATE
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 800); // ~90 degrees
            gpio_set_level(BUZZER_PIN, 0);
            
            lcd_put_cursor(dev_handle, 0, 0);
            lcd_send_string(dev_handle, "WELCOME!        ");
            lcd_put_cursor(dev_handle, 1, 0);
            lcd_send_string(dev_handle, "GATE: OPENING   ");
        } 
        else {
            // CLOSE GATE
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 400); // ~0 degrees
            
            lcd_put_cursor(dev_handle, 0, 0);
            if (slot_occupied && car_waiting) {
                lcd_send_string(dev_handle, "PARK FULL-WAIT  ");
                gpio_set_level(BUZZER_PIN, 1); // Alert driver parking is full
            } 
            else if (slot_occupied) {
                lcd_send_string(dev_handle, "SPOT OCCUPIED   ");
                gpio_set_level(BUZZER_PIN, 0);
            }
            else {
                lcd_send_string(dev_handle, "READY TO ENTER  ");
                gpio_set_level(BUZZER_PIN, 0);
            }
            
            lcd_put_cursor(dev_handle, 1, 0);
            lcd_send_string(dev_handle, "GATE: CLOSED    ");
        }
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        
        // Visual indicator for Slot Status
        gpio_set_level(LED_SLOT_1, slot_occupied);

        // --- D. DEBUG PRINT ---
        printf("Slot: %dcm (%s) | Entry: %dcm (%s) | Gate: %s\n", 
               slot_dist, slot_occupied ? "OCC" : "FREE", 
               entry_dist, car_waiting ? "WAIT" : "NONE",
               open_gate ? "OPEN" : "CLOSED");

        // Loop timing stability
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}