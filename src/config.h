//
// Created by GABRIEL on 12/04/2024.
//

#ifndef INSTRBASE_CONFIG_H
#define INSTRBASE_CONFIG_H

#define LED_PIN 25

// If you are changing the pins, please refer to https://pico.pinout.xyz/
#define I2C i2c0
#define I2C_PIN_SDA 4
#define I2C_PIN_SCL 5

#define HX711_PIN_CLK 14
#define HX711_PIN_DATA 15
#define WIND_GPIO 10
#define WIND_SAMPLE_TIME_US 2000000

#define WIND_RADIUS 0.105

#define LED_ON() gpio_put(LED_PIN, 1)
#define LED_OFF() gpio_put(LED_PIN, 0)


#endif //INSTRBASE_CONFIG_H
