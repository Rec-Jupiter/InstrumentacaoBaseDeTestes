//
// Created by GABRIEL on 12/04/2024.
//
#ifndef INSTRBASE_CONFIG_H
#define INSTRBASE_CONFIG_H




// If you are changing the pins, please refer to https://pico.pinout.xyz/

// ST7920 Display Config
#define ST7920_SD 0
#define ST7920_SCK 1
#define ST7920_CS 2
#define ST7920_RST 3

// SD Card GPIO Pins
#define SD_SPI spi0
#define SCK_GPIO 18     // SPI0
#define MOSI_GPIO 19    // SPI0
#define MISO_GPIO 16    // SPI0
#define SS_SDCARD_GPIO 17

// HX711 Pins
#define HX711_PIN_CLK 14
#define HX711_PIN_DATA 15

// Wind Stuff
#define WIND_GPIO 10
#define WIND_SAMPLE_TIME_US 1000000

#define WIND_RADIUS 0.105 // In meters

// Recording Controll GPIO Pin (Low active)
#define RECORDING_SWITCH 6




#define LED_PIN 25
#define LED_ON() gpio_put(LED_PIN, 1)
#define LED_OFF() gpio_put(LED_PIN, 0)

#endif //INSTRBASE_CONFIG_H
