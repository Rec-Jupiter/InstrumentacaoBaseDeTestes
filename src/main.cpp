#include "pico/stdlib.h"
#include <stdio.h>
#include "extern/hx711-pico-c/include/common.h"

#define LED_PIN 25

int main() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_put(LED_PIN, 1);

    sleep_ms(1000);

    gpio_put(LED_PIN, 0);

    sleep_ms(10000);
    printf("Starting");
    gpio_put(LED_PIN, 1);
    // 1. Set configuration
    hx711_config_t hxcfg;
    hx711_get_default_config(&hxcfg);

    hxcfg.clock_pin = 14; //GPIO pin connected to HX711 clock pin
    hxcfg.data_pin = 15; //GPIO pin connected to HX711 data pin

    //by default, the underlying PIO program will run on pio0
    //if you need to change this, you can do:
    //hxcfg.pio = pio1;

    hx711_t hx;

    // 2. Initialise
    hx711_init(&hx, &hxcfg);

    // 3. Power up the hx711 and set gain on chip
    hx711_power_up(&hx, hx711_gain_128);

    // 4. This step is optional. Only do this if you want to
    // change the gain AND save it to the HX711 chip
    //
    // hx711_set_gain(&hx, hx711_gain_64);
    // hx711_power_down(&hx);
    // hx711_wait_power_down();
    // hx711_power_up(&hx, hx711_gain_64);
    gpio_put(LED_PIN, 0);
    sleep_ms(5000);
    printf("Waiting to settle");
    // 5. Wait for readings to settle
    hx711_wait_settle(hx711_rate_80);
    printf("Settled");
    // 6. Read values
    // You can now...

    gpio_put(LED_PIN, 1);
    // wait (block) until a value is obtained
    while (true) {
        printf("blocking value: %li\n", hx711_get_value(&hx));
    }
    hx711_close(&hx);

    return 0;
}

