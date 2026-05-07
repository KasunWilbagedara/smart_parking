#include "parking_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static void lcd_write_nibble(i2c_master_dev_handle_t dev, uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    uint8_t en_high = data | LCD_ENABLE;
    uint8_t en_low = data & ~LCD_ENABLE;

    i2c_master_transmit(dev, &en_high, 1, 100);
    ets_delay_us(1);
    i2c_master_transmit(dev, &en_low, 1, 100);
    ets_delay_us(50);
}

void lcd_send_cmd(i2c_master_dev_handle_t dev, uint8_t cmd) {
    lcd_write_nibble(dev, cmd & 0xF0, 0);
    lcd_write_nibble(dev, (cmd << 4) & 0xF0, 0);
}

void lcd_send_data(i2c_master_dev_handle_t dev, uint8_t data) {
    lcd_write_nibble(dev, data & 0xF0, LCD_RS);
    lcd_write_nibble(dev, (data << 4) & 0xF0, LCD_RS);
}

void lcd_init(i2c_master_dev_handle_t dev) {
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_write_nibble(dev, 0x30, 0); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(dev, 0x30, 0); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(dev, 0x30, 0); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(dev, 0x20, 0); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_cmd(dev, 0x28); lcd_send_cmd(dev, 0x0C);
    lcd_send_cmd(dev, 0x06); lcd_send_cmd(dev, 0x01);
    vTaskDelay(pdMS_TO_TICKS(5));
}

void lcd_put_cursor(i2c_master_dev_handle_t dev, int row, int col) {
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_send_cmd(dev, addr);
}

void lcd_send_string(i2c_master_dev_handle_t dev, const char *str) {
    while (*str) lcd_send_data(dev, (uint8_t)*str++);
}

void lcd_clear(i2c_master_dev_handle_t dev) {
    lcd_send_cmd(dev, 0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}