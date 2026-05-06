/*#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

// --- Pin Definitions ---
#define TRIG_PIN_1    5
#define ECHO_PIN_1    18
#define TRIG_PIN_2    19
#define ECHO_PIN_2    21
#define LED_SLOT_1    13
#define LED_SLOT_2    14
#define BUZZER_PIN    27
#define SERVO_PIN     26

// --- System Constants ---
#define DISTANCE_THRESHOLD_CM 10
#define TOTAL_SLOTS 2

// --- Global Variables (Shared between tasks) ---
int distance_1 = 0;
int distance_2 = 0;
bool slot1_occupied = false;
bool slot2_occupied = false;
int available_slots = TOTAL_SLOTS;

// --- Function to read Ultrasonic Sensor (Raw ESP-IDF) ---
int get_distance(gpio_num_t trig_pin, gpio_num_t echo_pin) {
    // 1. Send 10us pulse to Trigger pin
    gpio_set_level(trig_pin, 0);
    ets_delay_us(2);
    gpio_set_level(trig_pin, 1);
    ets_delay_us(10);
    gpio_set_level(trig_pin, 0);

    // 2. Wait for Echo pin to go HIGH
    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 0) {
        if (esp_timer_get_time() - start_time > 10000) return -1; // Timeout
    }

    // 3. Measure how long Echo pin stays HIGH
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 1) {
        if (esp_timer_get_time() - echo_start > 24000) return -1; // Timeout (~400cm max)
    }
    int64_t echo_end = esp_timer_get_time();

    // 4. Calculate distance in cm
    int64_t time_us = echo_end - echo_start;
    return time_us / 58; 
}

// --- MODULE 1: Sensor Acquisition Task ---
void sensor_task(void *pvParameters) {
    while(1) {
        distance_1 = get_distance(TRIG_PIN_1, ECHO_PIN_1);
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between sensor reads
        distance_2 = get_distance(TRIG_PIN_2, ECHO_PIN_2);
        
        vTaskDelay(pdMS_TO_TICKS(200)); // Run this task every 200ms
    }
}

// --- MODULE 2 & 3: Processing & Output Task ---
void logic_output_task(void *pvParameters) {
    while(1) {
        // --- Processing Logic ---
        slot1_occupied = (distance_1 > 0 && distance_1 < DISTANCE_THRESHOLD_CM);
        slot2_occupied = (distance_2 > 0 && distance_2 < DISTANCE_THRESHOLD_CM);
        available_slots = TOTAL_SLOTS - (slot1_occupied + slot2_occupied);

        // --- Output Control (LEDs) ---
        gpio_set_level(LED_SLOT_1, slot1_occupied ? 1 : 0);
        gpio_set_level(LED_SLOT_2, slot2_occupied ? 1 : 0);

        // --- Output Control (LCD & WiFi Simulation) ---
        printf("\n--- Smart Parking System ---\n");
        printf("Slot 1: %s (%d cm)\n", slot1_occupied ? "[OCCUPIED]" : "[FREE]", distance_1);
        printf("Slot 2: %s (%d cm)\n", slot2_occupied ? "[OCCUPIED]" : "[FREE]", distance_2);
        printf("Available Slots: %d\n", available_slots);

        // --- Output Control (Buzzer & Servo Gate) ---
        if (available_slots == 0) {
            gpio_set_level(BUZZER_PIN, 1); // Turn ON buzzer
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 400); // Servo Closed (~0 degrees)
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            printf("Status: PARKING FULL! Gate Closed.\n");
        } else {
            gpio_set_level(BUZZER_PIN, 0); // Turn OFF buzzer
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 800); // Servo Open (~90 degrees)
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            printf("Status: Space Available. Gate Open.\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Update outputs every 1 second
    }
}

// --- Main Application Entry Point ---
void app_main(void) {
    printf("Initializing Hardware...\n");

    // 1. Configure Basic GPIOs (Sensors, LEDs, Buzzer)
    gpio_reset_pin(TRIG_PIN_1); gpio_set_direction(TRIG_PIN_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_PIN_1); gpio_set_direction(ECHO_PIN_1, GPIO_MODE_INPUT);
    gpio_reset_pin(TRIG_PIN_2); gpio_set_direction(TRIG_PIN_2, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_PIN_2); gpio_set_direction(ECHO_PIN_2, GPIO_MODE_INPUT);
    
    gpio_reset_pin(LED_SLOT_1); gpio_set_direction(LED_SLOT_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_SLOT_2); gpio_set_direction(LED_SLOT_2, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BUZZER_PIN); gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    // 2. Configure PWM for Servo Motor (50Hz)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 50,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN,
        .duty           = 0, 
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    // 3. Create FreeRTOS Tasks
    // This allows sensor reading and processing to run at the same time
    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 5, NULL);
    xTaskCreate(logic_output_task, "logic_task", 2048, NULL, 5, NULL);
}
    */
   #include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "parking_system.h"

void app_main(void) {
    printf("Parking System Modular Startup...\n");

    // Initialize GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL<<TRIG_PIN_1) | (1ULL<<TRIG_PIN_2) | (1ULL<<LED_SLOT_1) | (1ULL<<LED_SLOT_2) | (1ULL<<BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0, .pull_up_en = 0, .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    gpio_set_direction(ECHO_PIN_1, GPIO_MODE_INPUT);
    gpio_set_direction(ECHO_PIN_2, GPIO_MODE_INPUT);

    // Initialize Servo PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT, .freq_hz = 50, .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0, .gpio_num = SERVO_PIN, .duty = 0
    };
    ledc_channel_config(&ledc_channel);

    // Start Tasks
    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 5, NULL);
    xTaskCreate(logic_output_task, "logic_task", 2048, NULL, 5, NULL);
}