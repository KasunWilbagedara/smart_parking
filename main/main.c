#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "esp_err.h"

// ─── Pin Definitions ───────────────────────────────────────────────────────
#define TRIG_PIN_1    5
#define ECHO_PIN_1    18
#define LED_SLOT_1    13
#define BUZZER_PIN    47
#define SERVO_PIN     21

// ─── I2C LCD ────────────────────────────────────────────────────────────────
#define I2C_MASTER_SDA_IO   8
#define I2C_MASTER_SCL_IO   9
#define I2C_MASTER_NUM      0
#define I2C_MASTER_FREQ_HZ  100000
#define LCD_ADDR            0x27

// ─── App Config ─────────────────────────────────────────────────────────────
#define DISTANCE_THRESHOLD_CM 10
#define TOTAL_SLOTS           1

// ─── LCD Bit Mapping for PCF8574 ─────────────────────────────────────────────
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

// ─── Global State ────────────────────────────────────────────────────────────
int  distance_1      = 0;
bool slot1_occupied  = false;
int  available_slots = TOTAL_SLOTS;

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

// ════════════════════════════════════════════════════════════════════════════
//  LCD Driver
// ════════════════════════════════════════════════════════════════════════════

void lcd_write_byte(uint8_t data)
{
    i2c_master_transmit(dev_handle, &data, 1, pdMS_TO_TICKS(100));
}

/**
 * Pulse the Enable line.
 * PCF8574 adds I2C propagation delay, so we need:
 *   - 1µs  EN high  (HD44780 requires >450 ns)
 *   - 50µs after deasserting EN (execution time for most commands)
 */
void lcd_pulse_enable(uint8_t data)
{
    lcd_write_byte(data | LCD_ENABLE | LCD_BACKLIGHT);     // EN high
    ets_delay_us(1);                                       // hold >450 ns
    lcd_write_byte((data & ~LCD_ENABLE) | LCD_BACKLIGHT);  // EN low → latch
    ets_delay_us(50);                                      // command settle
}

void lcd_send_nibble(uint8_t nibble, uint8_t mode)
{
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    lcd_pulse_enable(data);
}

/** Send a full command byte (two nibbles, RS=0). */
void lcd_send_cmd(uint8_t cmd)
{
    lcd_send_nibble(cmd & 0xF0, 0);
    lcd_send_nibble((cmd << 4) & 0xF0, 0);
}

/** Send a data byte (two nibbles, RS=1). */
void lcd_send_data(uint8_t data)
{
    lcd_send_nibble(data & 0xF0, LCD_RS);
    lcd_send_nibble((data << 4) & 0xF0, LCD_RS);
}

/**
 * HD44780 4-bit initialisation — original sequence preserved.
 * Only fix: all three reset nibble delays bumped to 5ms (was 150µs for
 * steps 2 & 3) to ensure the internal reset circuitry completes.
 */
void lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    // HD44780 3-step 8-bit reset sequence
    lcd_send_nibble(0x30, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_nibble(0x30, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_nibble(0x30, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_nibble(0x20, 0);   // Switch to 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_cmd(0x28); // 4-bit, 2 lines, 5x8 font
    lcd_send_cmd(0x0C); // Display ON, cursor OFF
    lcd_send_cmd(0x06); // Entry mode
    lcd_send_cmd(0x01); // Clear display
    vTaskDelay(pdMS_TO_TICKS(5));
}

/** Clear display and wait for the command to complete. */
void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(5));
}

/** Move cursor to (row, col).  row: 0 or 1.  col: 0–15. */
void lcd_put_cursor(int row, int col)
{
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_send_cmd(addr);
}

/** Write a null-terminated string from the current cursor position. */
void lcd_send_string(const char *str)
{
    while (*str)
        lcd_send_data((uint8_t)*str++);
}

// ════════════════════════════════════════════════════════════════════════════
//  Ultrasonic Sensor
// ════════════════════════════════════════════════════════════════════════════

/**
 * Trigger the HC-SR04 and return the measured distance in cm.
 * Returns -1 on timeout (no echo / object out of range).
 */
int get_distance(gpio_num_t trig_pin, gpio_num_t echo_pin)
{
    // Ensure trig is low before pulse
    gpio_set_level(trig_pin, 0);
    ets_delay_us(2);

    // 10 µs trigger pulse
    gpio_set_level(trig_pin, 1);
    ets_delay_us(10);
    gpio_set_level(trig_pin, 0);

    // Wait for echo to go high (timeout 10 ms)
    int64_t start_wait = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 0) {
        if ((esp_timer_get_time() - start_wait) > 10000) return -1;
    }

    // Measure echo pulse width (timeout 24 ms ≈ ~400 cm max range)
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 1) {
        if ((esp_timer_get_time() - echo_start) > 24000) return -1;
    }
    int64_t echo_end = esp_timer_get_time();

    // Distance (cm) = pulse duration (µs) / 58
    return (int)((echo_end - echo_start) / 58);
}

// ════════════════════════════════════════════════════════════════════════════
//  FreeRTOS Tasks
// ════════════════════════════════════════════════════════════════════════════

/** Reads distance sensor every 200 ms and updates the global variable. */
void sensor_task(void *pvParameters)
{
    while (1) {
        distance_1 = get_distance(TRIG_PIN_1, ECHO_PIN_1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * Evaluates slot state and updates LED, buzzer, servo, and LCD every second.
 *
 * LCD update strategy: instead of calling lcd_clear() every cycle (which
 * causes a brief blank flash and can leave garbage if interrupted mid-clear),
 * we overwrite each line with exactly 16 characters so old content is always
 * fully replaced without a visible clear.
 */
void logic_output_task(void *pvParameters)
{
    char line1_buff[17];  // 16 chars + null terminator

    // Perform a single clear at startup so the display is clean
    lcd_clear();

    while (1) {
        // ── Evaluate slot occupancy ──────────────────────────────────────
        slot1_occupied  = (distance_1 > 0 && distance_1 < DISTANCE_THRESHOLD_CM);
        available_slots = TOTAL_SLOTS - (int)slot1_occupied;

        // ── LED indicator ────────────────────────────────────────────────
        gpio_set_level(LED_SLOT_1, slot1_occupied ? 1 : 0);

        // ── Serial debug output ───────────────────────────────────────────
        printf("\n--- Smart Parking System ---\n");
        printf("Distance : %d cm\n", distance_1);
        printf("Slot 1   : %s\n", slot1_occupied ? "OCCUPIED" : "FREE");

        if (available_slots == 0) {
            // ── Parking FULL ─────────────────────────────────────────────
            // Overwrite both lines with exactly 16 chars (no lcd_clear needed)
            lcd_put_cursor(0, 0);
            lcd_send_string(" PARK IS FULL  ");  // 15 chars + leading space = 16

            lcd_put_cursor(1, 0);
            lcd_send_string(" Gate: CLOSED  ");  // 16 chars

            // Buzzer ON
            gpio_set_level(BUZZER_PIN, 1);

            // Servo: ~0° (closed gate) → duty ~400 / 8191 at 50 Hz
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 400);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            printf("Status   : PARK IS FULL\n");
        } else {
            // ── Slots available ───────────────────────────────────────────
            // %-2d left-justifies the number and pads to 2 chars so we
            // always write exactly 16 characters and overwrite any remnant.
            snprintf(line1_buff, sizeof(line1_buff), "Slots Avail: %-2d ", available_slots);

            lcd_put_cursor(0, 0);
            lcd_send_string(line1_buff);          // always 16 chars

            lcd_put_cursor(1, 0);
            lcd_send_string(" Gate: OPEN    ");   // 16 chars

            // Buzzer OFF
            gpio_set_level(BUZZER_PIN, 0);

            // Servo: ~90° (open gate) → duty ~800 / 8191 at 50 Hz
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 800);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            printf("Status   : Slots Available: %d\n", available_slots);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  app_main — Hardware Initialisation
// ════════════════════════════════════════════════════════════════════════════

void app_main(void)
{
    printf("Initializing Hardware...\n");

    // ── I2C master bus ───────────────────────────────────────────────────
    i2c_master_bus_config_t bus_config = {
        .i2c_port              = I2C_MASTER_NUM,
        .sda_io_num            = I2C_MASTER_SDA_IO,
        .scl_io_num            = I2C_MASTER_SCL_IO,
        .clk_source            = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt     = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // ── I2C device (PCF8574 at 0x27) ────────────────────────────────────
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = LCD_ADDR,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    // ── LCD init & splash screen ─────────────────────────────────────────
    lcd_init();

    lcd_put_cursor(0, 0);
    lcd_send_string("Smart Parking   ");
    lcd_put_cursor(1, 0);
    lcd_send_string("System Ready    ");
    vTaskDelay(pdMS_TO_TICKS(1500));

    // ── GPIO: ultrasonic sensor ──────────────────────────────────────────
    gpio_reset_pin(TRIG_PIN_1);
    gpio_set_direction(TRIG_PIN_1, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ECHO_PIN_1);
    gpio_set_direction(ECHO_PIN_1, GPIO_MODE_INPUT);

    // ── GPIO: LED indicator ──────────────────────────────────────────────
    gpio_reset_pin(LED_SLOT_1);
    gpio_set_direction(LED_SLOT_1, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_SLOT_1, 0);

    // ── GPIO: buzzer ─────────────────────────────────────────────────────
    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_PIN, 0);

    // ── LEDC (servo PWM): 50 Hz, 13-bit resolution ───────────────────────
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,   // 8191 steps
        .freq_hz         = 50,                  // Standard servo frequency
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = SERVO_PIN,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // ── Start FreeRTOS tasks ─────────────────────────────────────────────
    xTaskCreate(sensor_task,       "sensor_task", 2048, NULL, 5, NULL);
    xTaskCreate(logic_output_task, "logic_task",  4096, NULL, 5, NULL);

    // 3. Create FreeRTOS Tasks
    // This allows sensor reading and processing to run at the same time
    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 5, NULL);
    xTaskCreate(logic_output_task, "logic_task", 2048, NULL, 5, NULL);
}
    */
 #include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "parking_system.h"

void app_main(void) {
    // 1. Setup I2C Bus and Device
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = 0, .sda_io_num = I2C_SDA_IO, .scl_io_num = I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    i2c_new_master_bus(&bus_cfg, &bus_handle);
    i2c_device_config_t dev_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = LCD_ADDR, .scl_speed_hz = 100000 };
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

    // 2. Setup Peripheral Hardware
    lcd_init(dev_handle);
    gpio_reset_pin(TRIG_PIN_1); gpio_set_direction(TRIG_PIN_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_PIN_1); gpio_set_direction(ECHO_PIN_1, GPIO_MODE_INPUT);
    gpio_reset_pin(LED_SLOT_1); gpio_set_direction(LED_SLOT_1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BUZZER_PIN); gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    ledc_timer_config_t led_tmr = { .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = 0, .duty_resolution = 13, .freq_hz = 50, .clk_cfg = LEDC_AUTO_CLK };
    ledc_timer_config(&led_tmr);
    ledc_channel_config_t led_ch = { .speed_mode = LEDC_LOW_SPEED_MODE, .channel = 0, .timer_sel = 0, .gpio_num = SERVO_PIN, .duty = 0 };
    ledc_channel_config(&led_ch);

    // 3. Main Infinite Super-Loop
while (1) {
        // 1. Read Sensors
        // Old Sensor (5, 18) checks if the parking slot is occupied
        int slot_dist = get_distance(TRIG_PIN_1, ECHO_PIN_1); 
        vTaskDelay(pdMS_TO_TICKS(50)); 
        
        // New Sensor (10, 11) checks if a car is waiting at the gate
        int gate_req_dist = get_distance(TRIG_PIN_2, ECHO_PIN_2);

        // 2. Define States
        bool slot_occupied = (slot_dist > 0 && slot_dist < 10);
        bool car_at_gate   = (gate_req_dist > 0 && gate_req_dist < 10);

        // 3. Apply your Logic Rules
        // Rule 1: Gate opens ONLY IF (Car at Gate) AND (Slot is NOT occupied)
        // Rule 2: If Slot is occupied, gate MUST be closed
        // Rule 3: If no car at gate, gate MUST be closed
        
        bool should_open = car_at_gate && !slot_occupied;

        // 4. Execute Hardware Actions
        if (should_open) {
            // OPEN GATE
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 800); // ~90 degrees
            gpio_set_level(BUZZER_PIN, 0);
            
            lcd_put_cursor(dev_handle, 0, 0);
            lcd_send_string(dev_handle, "Welcome!        ");
            lcd_put_cursor(dev_handle, 1, 0);
            lcd_send_string(dev_handle, "Gate: OPENing   ");
        } 
        else {
            // CLOSE GATE
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 400); // ~0 degrees
            
            lcd_put_cursor(dev_handle, 0, 0);
            if (slot_occupied) {
                lcd_send_string(dev_handle, "SPOT OCCUPIED   ");
                gpio_set_level(BUZZER_PIN, 1); // Alarm if someone tries to enter when full
            } else {
                lcd_send_string(dev_handle, "READY - NO CAR  ");
                gpio_set_level(BUZZER_PIN, 0);
            }
            
            lcd_put_cursor(dev_handle, 1, 0);
            lcd_send_string(dev_handle, "Gate: CLOSED    ");
        }
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        // Debug console output
        printf("Slot Occ: %s | Car at Gate: %s\n", 
                slot_occupied ? "YES" : "NO", 
                car_at_gate ? "YES" : "NO");

        vTaskDelay(pdMS_TO_TICKS(500)); 
<<<<<<< HEAD
    }
=======
    }
    printf("System running.\n");
  
}
>>>>>>> b264723c3b08632561b16f683678294312e827a8
