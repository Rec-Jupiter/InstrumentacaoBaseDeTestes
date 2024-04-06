#include "pico/stdlib.h"
#include <stdio.h>
#include "extern/hx711-pico-c/include/common.h"
#include "extern/pico-ssd1306/textRenderer/TextRenderer.h"
#include "extern/pico-ssd1306/ssd1306.h"
#include "hardware/irq.h"
#include "pico/multicore.h"

#define LED_PIN 25

#define I2C_PIN_SDA 4
#define I2C_PIN_SCL 5

#define HX711_PIN_CLK 14
#define HX711_PIN_DATA 15

#define I2C i2c0

void init_hx711();
void init_display();
void init_i2c();

void core1_interrupt_handler();
void core1_entry();

pico_ssd1306::SSD1306* display;
hx711_t* hx;

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    sleep_ms(3500);

    init_i2c();
    init_display();

    sleep_ms(1500);

    init_hx711();

    multicore_launch_core1(core1_entry);

    while (true) {
        int value = hx711_get_raw_value(hx)/* * 852.0/730000.0*/;

        if ((value & 0x00800000) == 0x00800000) {  // Bit 23 == 1
            value |= 0xFF000000;
        }

        char str[16];

        printf("blocking value: %8li\n", value);

        multicore_fifo_push_blocking(value); //Maybe use timeout, when fifoo gets full (it shouldnt but anyways) this will bloock the program


        sleep_ms(5);
        tight_loop_contents();
   }


    hx711_close(hx); // essa linha n executa eu sei ela ta ai so pra lembrar q essa funcao existe

    return 0;
}

void core1_interrupt_handler() {
    while(multicore_fifo_rvalid()) {
        char str[16];
        int raw = multicore_fifo_pop_blocking();

        printf("received data! %d \n", raw);

        sprintf(str,"%8li",raw);

        display->clear();
        pico_ssd1306::drawText(display, font_12x16, str, 0 ,0);
        display->sendBuffer();
    }

    multicore_fifo_clear_irq();
}

void core1_entry() {
    multicore_fifo_clear_irq();
    irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);
    irq_set_enabled(SIO_IRQ_PROC1, true);

    printf("Core 1 executing");

    while(1) {
        tight_loop_contents();
        sleep_ms(10);
    }
}


void init_i2c() {
    printf("Setting i2c\n");
    i2c_init(I2C, 1000000); //Use i2c port with baud rate of 1Mhz
    //Set pins for I2C operation
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);
}

void init_display() {
    printf("Creating display\n");
    display = new pico_ssd1306::SSD1306(I2C, 0x3C, pico_ssd1306::Size::W128xH64);

    printf("Sending data!\n");
    //create a vertical line on x: 64 y:0-63
    for (int y = 0; y < 64; y++){
        display->setPixel(64, y);
    }
    printf("Buffer Made");

    display->sendBuffer(); //Send buffer to device and show on screen
    sleep_ms(1000);
    display->clear();
    // Draw some text
    // Notice how we first pass the address of display object to the function
    drawText(display, font_12x16, "Waiting...", 0 ,0);
    display->sendBuffer();
    gpio_put(LED_PIN, 0);
}

void init_hx711() {
    printf("Starting");
    gpio_put(LED_PIN, 1);

    // 1. Set configuration
    hx711_config_t hxcfg;
    hx711_get_default_config(&hxcfg);

    hxcfg.clock_pin = HX711_PIN_CLK; //GPIO pin connected to HX711 clock pin
    hxcfg.data_pin = HX711_PIN_DATA; //GPIO pin connected to HX711 data pin

    hx = new hx711_t();

    // 2. Initialise
    hx711_init(hx, &hxcfg);
    hx711_power_up(hx, hx711_gain_128);

    gpio_put(LED_PIN, 0);
    printf("Waiting for readings to settle\n");

    hx711_wait_settle(hx711_rate_80);

    printf("Settled\n");

    gpio_put(LED_PIN, 1);
}