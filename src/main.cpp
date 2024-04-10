#include "pico/stdlib.h"
#include <cstdio>
#include <cstdlib>
#include "extern/hx711-pico-c/include/common.h"
#include "extern/pico-ssd1306/textRenderer/TextRenderer.h"
#include "extern/pico-ssd1306/ssd1306.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include <cinttypes>
#include "hardware/flash.h"
#include "helpers.h"
#include "ff.h"
#include "f_util.h"
#include "rtc.h"

// To override the log level uncomment the line below and change 'Debug' to the desired minimum level
//#undef LOG_LEVEL
//#define LOG_LEVEL Debug

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

#define MIN_DATA_BUFFER_LEN 80

//From core 0 to 1
#define SENDING_DATA_LIST_FLAG 1

//From core 1 to 0
#define DONT_SEND_DATA_FLAG 2
#define CAN_SEND_DATA_FLAG 3

struct Node;
struct DataPoint;
union DataPointUnion;


void init_hx711();
void init_display();
void init_i2c();
void init_wind_measure();

void gpio_interrupt_handler(uint gpio, uint32_t events);

void core1_interrupt_handler();

[[noreturn]] void core1_entry();

[[noreturn]] void measuring_loop_blocking();

void data_list_received(Node* list);

void handle_sd();

struct __attribute__ ((__packed__)) DataPoint {                 // __attribute__ ((__packed__)) means it will be
    uint64_t time;                                              // stored in memory without padding.
    float wind_speed;
    int hx711_value;
};

union DataPointUnion {                                           // Hacky union for getting the data as its bytes
    DataPoint data;
    uint8_t bytes[sizeof(DataPoint)];
};

struct Node {                                                    // Linked list used for buffering on Core 0
    DataPointUnion point;
    Node* next;
};


pico_ssd1306::SSD1306* display;
hx711_t* hx;


int windClicks = 0;

uint32_t last_flag = 0xFFFFFFFF;

bool recording;
uint64_t recordingStartingTime;


int createdNodes = 0;


int main() {
    stdio_init_all();
    time_init();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    LOG(Information, "Starting up!");
    LED_ON();

    recording = false;
    recordingStartingTime = 0;

    sleep_ms(3500);

    handle_sd();
    init_i2c();
    init_display();

    sleep_ms(1500);

    init_hx711();
    init_wind_measure();


    multicore_launch_core1(core1_entry);


    measuring_loop_blocking();

    hx711_close(hx); // essa linha n executa eu sei ela ta ai so pra lembrar q essa funcao existe

    return 0;
}

[[noreturn]] void core1_entry() {
    multicore_fifo_clear_irq();
    irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);
    irq_set_enabled(SIO_IRQ_PROC1, true);

    LOG(Information, "Core 1 executing");

    multicore_fifo_push_blocking(CAN_SEND_DATA_FLAG);
    while(true) {
        tight_loop_contents();
        sleep_ms(10);
    }
}

void core1_interrupt_handler() {
    while(multicore_fifo_rvalid()) {
        uint32_t raw = multicore_fifo_pop_blocking();

        if (last_flag == 0xFFFFFFFF) {
            last_flag = raw;
            continue;
        }

        switch (last_flag) {
            case SENDING_DATA_LIST_FLAG: {
                Node *head = (Node *) raw;  // O RP2040 eh 32bit, mas a IDE nao sabe disso ent **SE A IDE** estiver dando erro
                                            // de '... cast to smaller ...' ignora. Se quando compilar der erro nao ignora nn

                data_list_received(head);
                last_flag = 0xFFFFFFFF;
                break;
            }
            default:
                LOG(Error, "Error, flag not recognized");
        }
    }

    multicore_fifo_clear_irq();
}


[[noreturn]] void measuring_loop_blocking() {
    Node* head = (Node*)malloc(sizeof(Node));
    head->next = nullptr;
    Node* currentNode = head;

    createdNodes++;

    int count = 0;
    bool canSendData = true;

    float lastMeasuredWindSpeed = 0;

    uint64_t lastWindMeasureTime = time_us_64();
    uint64_t lastTime = time_us_64();

    while (true) {
        int value = hx711_get_raw_value(hx)/* * 852.0/730000.0*/; //blocking

        /*if ((value & 0x00C00000) >= 0)  // Bit 23 == 1
            value |= 0xFF000000;*/

        LOG(Debug, "blocking value: %8li", value);

        uint64_t timeSinceLastMeasureUS = time_us_64() - lastWindMeasureTime;
        if (timeSinceLastMeasureUS >= WIND_SAMPLE_TIME_US) {
            // 1 click = 2pi rad
            // x clicks/s = x*2pi rad/s
            // v = w * r
            // w = x*2pi rad/s
            // v = x * 2pi * r m/s
            // v = (clicks * 2pi * r) / timeSinceLast

            lastMeasuredWindSpeed = (float)(((double)windClicks * 2 * 3.1415926535f * WIND_RADIUS) / ((double)timeSinceLastMeasureUS / 1000000.0));

            LOG(Information, "Calculated wind speed is: %2.3f m/s", lastMeasuredWindSpeed);

            windClicks = 0;
            lastWindMeasureTime = time_us_64();
        }

        currentNode->point.data.hx711_value = value;
        currentNode->point.data.time = time_us_64() - recordingStartingTime;
        currentNode->point.data.wind_speed = lastMeasuredWindSpeed;
        count++;

        if (count >= MIN_DATA_BUFFER_LEN) {
            LOG(Information, "blocking value: %8li", value);
            LOG(Information, "Period time: %" PRIu64, time_us_64() - lastTime);
            LOG(Information, "Can Send %d", canSendData);
        }


        while (multicore_fifo_rvalid()) {
            uint32_t flag = multicore_fifo_pop_blocking();

            switch (flag) {
                case DONT_SEND_DATA_FLAG:
                    canSendData = false;
                    break;
                case CAN_SEND_DATA_FLAG:
                    canSendData = true;
                    break;
                default:
                    LOG(Error, "Unrecognized flag sent from core 1 to 0: %d", flag);
                    break;
            }
        }

        if (count >= MIN_DATA_BUFFER_LEN && canSendData) {
            multicore_fifo_push_blocking(SENDING_DATA_LIST_FLAG);
            multicore_fifo_push_blocking((uint64_t)head); //Maybe use timeout, when fifoo gets full (it shouldnt but anyways) this will bloock the program

            count = 0;

            head = (Node*)malloc(sizeof(Node));
            createdNodes++;

            head->next = nullptr;
            currentNode = head;
        } else {
            currentNode->next = (Node*)malloc(sizeof(Node));
            createdNodes++;

            currentNode = currentNode->next;
            currentNode->next = nullptr;
        }

        LOG(Debug, "Period time: %" PRIu64, time_us_64() - lastTime);

        lastTime = time_us_64();

        tight_loop_contents();
    }

}

void handle_sd() {
    gpio_pull_up(19);
    gpio_pull_up(16);

    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) LOG(Error, "f_mount error: %s (%d)", FRESULT_str(fr), fr);


    FIL fil;
    const char* const filename = "filename.txt";
    fr = f_open(&fil, filename, FA_CREATE_NEW | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
        LOG(Error, "f_open(%s) error: %s (%d)", filename, FRESULT_str(fr), fr);
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        LOG(Error, "f_printf failed");
    }
    fr = f_close(&fil);
    if (FR_OK != fr) {
        LOG(Error, "f_close error: %s (%d)", FRESULT_str(fr), fr);
    }


    f_unmount("");
}

void data_list_received(Node* list) {
    multicore_fifo_push_blocking(DONT_SEND_DATA_FLAG);


    if (createdNodes >= MIN_DATA_BUFFER_LEN * 12)
        LOG(Error, "[ERROR - MEM LEAK] Possible memory leak detected!!! %d nodes currently existing!", createdNodes);

    char str[16];
    char windStr[16];
    LOG(Information, "Received new buffer, first value is %d and wind speed is %f", list->point.data.hx711_value, list->point.data.wind_speed);

    sprintf(str, "F: %li", list->point.data.hx711_value);
    sprintf(windStr, "W: %06.3f", list->point.data.wind_speed);

    display->clear();
    pico_ssd1306::drawText(display, font_12x16, str, 0 ,0);
    pico_ssd1306::drawText(display, font_12x16, windStr, 0 ,20);
    display->sendBuffer();

    LOG(Debug, "Erasing");

    uint8_t data[FLASH_PAGE_SIZE];


    LOG(Debug, "While");
    int offset = 0;
    while (list != nullptr) {
        LOG(Debug, "Reading (sizeof: %d)", sizeof(DataPoint));
        if (offset + sizeof(DataPoint) > FLASH_PAGE_SIZE) {
            offset = 0;

            for (uint8_t &i : data) {
                i = 0;
            }
        }

        for (int i = 0; i < sizeof(DataPoint); i++) {
            data[offset + i] = list->point.bytes[i];
        }
        offset += sizeof(DataPoint);

        LOG(Debug, "clearing list value %li", list->point.data.hx711_value);
        Node* lastNode = list;
        list = list->next;

        LOG(Debug, "Freeing node memory");
        free(lastNode);
        createdNodes--;
    }


    multicore_fifo_push_blocking(CAN_SEND_DATA_FLAG);
}





void gpio_interrupt_handler(uint gpio, uint32_t events) {
    LOG(Verbose, "GPIO %d %d, edge rise event is %d", gpio, events, (uint32_t)GPIO_IRQ_EDGE_RISE);
    if (gpio != WIND_GPIO) return;
    if ((events & (uint32_t)GPIO_IRQ_EDGE_RISE) != (uint32_t)GPIO_IRQ_EDGE_RISE) return;

    LOG(Debug, "Adding to wind counter");
    windClicks++;
}




/*
 * The core that calls this funcion is the one that will receive the interrupt
 */
void init_wind_measure() {
    gpio_set_dir(WIND_GPIO, GPIO_IN);
    gpio_pull_down(WIND_GPIO);
    gpio_set_irq_enabled_with_callback(WIND_GPIO, GPIO_IRQ_EDGE_RISE, true, &gpio_interrupt_handler);
}


void init_i2c() {
    LOG(Information, "Setting i2c");
    i2c_init(I2C, 1000000); //Use i2c port with baud rate of 1Mhz
    //Set pins for I2C operation
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);
}

void init_display() {
    LOG(Information, "Creating display");
    display = new pico_ssd1306::SSD1306(I2C, 0x3C, pico_ssd1306::Size::W128xH64);

    LOG(Verbose, "Sending data!");
    //create a vertical line on x: 64 y:0-63
    for (int y = 0; y < 64; y++){
        display->setPixel(64, y);
    }
    LOG(Verbose, "Buffer Made");

    display->sendBuffer(); //Send buffer to device and show on screen
    sleep_ms(1000);
    display->clear();
    // Draw some text
    // Notice how we first pass the address of display object to the function
    drawText(display, font_12x16, "Waiting...", 0 ,0);
    display->sendBuffer();

    LED_OFF();
}

void init_hx711() {
    LOG(Information, "Starting HX711");
    LED_ON();

    // 1. Set configuration
    hx711_config_t hxcfg;
    hx711_get_default_config(&hxcfg);

    hxcfg.clock_pin = HX711_PIN_CLK; //GPIO pin connected to HX711 clock pin
    hxcfg.data_pin = HX711_PIN_DATA; //GPIO pin connected to HX711 data pin

    hx = new hx711_t();

    // 2. Initialise
    hx711_init(hx, &hxcfg);
    hx711_power_up(hx, hx711_gain_128);

    LED_OFF();

    LOG(Information, "Waiting for readings to settle");

    hx711_wait_settle(hx711_rate_80);

    LOG(Information, "Settled");

    LED_ON();
}