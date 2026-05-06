#ifndef PARKING_SYSTEM_H
#define PARKING_SYSTEM_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"

// --- Pin Definitions ---
#define TRIG_PIN_1     5
#define ECHO_PIN_1     18
#define LED_SLOT_1     13
#define BUZZER_PIN     47
#define SERVO_PIN      21
#define I2C_SDA_IO     8
#define I2C_SCL_IO     9

// --- LCD Constants ---
#define LCD_ADDR       0x27
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

// --- Prototypes ---
void lcd_init(i2c_master_dev_handle_t dev);
void lcd_send_string(i2c_master_dev_handle_t dev, const char *str);
void lcd_put_cursor(i2c_master_dev_handle_t dev, int row, int col);
void lcd_clear(i2c_master_dev_handle_t dev);
int get_distance(gpio_num_t trig, gpio_num_t echo);

#endif