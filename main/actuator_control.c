#include <stdio.h>
#include "parking_system.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool slot1_occupied = false;
bool slot2_occupied = false;
int available_slots = TOTAL_SLOTS;

void logic_output_task(void *pvParameters) {
    while(1) {
        slot1_occupied = (distance_1 > 0 && distance_1 < DISTANCE_THRESHOLD_CM);
        slot2_occupied = (distance_2 > 0 && distance_2 < DISTANCE_THRESHOLD_CM);
        available_slots = TOTAL_SLOTS - (slot1_occupied + slot2_occupied);

        gpio_set_level(LED_SLOT_1, slot1_occupied);
        gpio_set_level(LED_SLOT_2, slot2_occupied);

        printf("\n--- Status: %d Slots Available ---\n", available_slots);
        printf("S1: %d cm | S2: %d cm\n", distance_1, distance_2);

        if (available_slots == 0) {
            gpio_set_level(BUZZER_PIN, 1);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 400); // Closed
            printf("FULL: Gate Closed\n");
        } else {
            gpio_set_level(BUZZER_PIN, 0);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 800); // Open
            printf("OPEN: Gate Active\n");
        }
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}