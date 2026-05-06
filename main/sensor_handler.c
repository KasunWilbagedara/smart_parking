#include "parking_system.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

int get_distance(gpio_num_t trig_pin, gpio_num_t echo_pin) {
    gpio_set_level(trig_pin, 0);
    ets_delay_us(2);
    gpio_set_level(trig_pin, 1);
    ets_delay_us(10);
    gpio_set_level(trig_pin, 0);

    int64_t start_wait = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 0) {
        if ((esp_timer_get_time() - start_wait) > 10000) return -1;
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 1) {
        if ((esp_timer_get_time() - echo_start) > 24000) return -1;
    }
    return (int)((esp_timer_get_time() - echo_start) / 58);
}