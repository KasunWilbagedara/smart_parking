#ifndef PARKING_SYSTEM_H
#define PARKING_SYSTEM_H

#include "driver/gpio.h"

// --- Pin Definitions ---
#define TRIG_PIN_1     5
#define ECHO_PIN_1     18
#define TRIG_PIN_2     19
#define ECHO_PIN_2     21
#define LED_SLOT_1     13
#define LED_SLOT_2     14
#define BUZZER_PIN     27
#define SERVO_PIN      26

// --- Constants ---
#define DISTANCE_THRESHOLD_CM 10
#define TOTAL_SLOTS 2

// --- Global Variables (declared as extern to share across files) ---
extern int distance_1;
extern int distance_2;
extern bool slot1_occupied;
extern bool slot2_occupied;
extern int available_slots;

// --- Function Prototypes ---
void sensor_task(void *pvParameters);
void logic_output_task(void *pvParameters);

#endif