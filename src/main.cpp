#include "pico/stdlib.h"
#include <cstdio>
#include <cstdlib>
#include "extern/hx711-pico-c/include/common.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "helpers.h"
#include "rtc.h"
#include "sd_card_driver.h"
#include "config.h"
#include "main.h"
#include "display_driver.h"


// To override the log level uncomment the line below and change 'Debug' to the desired minimum level
//#undef LOG_LEVEL
//#define LOG_LEVEL Debug

#define MIN_DATA_BUFFER_LEN 80 // Since the HX711 sample rate is 80Hz, this is about 1s of time

// === FIFO FLAGS ===

//From core 0 to 1
#define SENDING_DATA_LIST_FLAG 1

//From core 1 to 0
#define DONT_SEND_DATA_FLAG 2
#define CAN_SEND_DATA_FLAG 3

// === END OF FIFO FLAGS ===

// === GLOBAL VARIABLES ===

hx711_t* hx;
int sd_success = 0;

int windClicks = 0;

bool recording;
uint64_t recordingStartingTime;

// Mem tracking
int createdNodes = 0;

// === END GLOBAL VARIABLES ===

// === ACTUAL CODE ===

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

    init_ST7920_display();

    multicore_launch_core1(core1_entry);

    sleep_ms(1500);

    init_hx711();
    init_wind_measure();



    measuring_loop_blocking();

    hx711_close(hx); // essa linha n executa eu sei ela ta ai so pra lembrar q essa funcao existe

    return 0;
}

[[noreturn]] void core1_entry() {
    multicore_fifo_clear_irq();

    LOG_COLORED(Information, CONSOLE_COLOR_GREEN, "Core 1 executing");


    sd_success = init_sd();

    gpio_init(RECORDING_SWITCH);
    gpio_set_dir(RECORDING_SWITCH, GPIO_IN);
    gpio_pull_up(RECORDING_SWITCH);

    sleep_ms(500);

    uint32_t last_flag = 0xFFFFFFFF;

    multicore_fifo_push_blocking(CAN_SEND_DATA_FLAG);
    while(true) {
        sleep_ms(5);

        if (recording && gpio_get(RECORDING_SWITCH)) {          // Recording sw off
            LOG_COLORED(Verbose, CONSOLE_COLOR_CYAN, "Recording switch turned off!");
            recording = false;
            finish_current_recording();

            if (sd_success) LOG_FOOTER("Not Recording      ");
        } else if (!recording && !gpio_get(RECORDING_SWITCH)) { // Recording sw on
            LOG_COLORED(Verbose, CONSOLE_COLOR_CYAN, "Recording switch turned on!");
            recording = true;
            char* filename = create_new_recording();

            if (sd_success) LOG_FOOTER("File: %s", filename);
        }

        while(multicore_fifo_rvalid()) {
            uint32_t raw = multicore_fifo_pop_blocking();

            if (last_flag == 0xFFFFFFFF) {
                last_flag = raw;
                continue;
            }

            switch (last_flag) {
                case SENDING_DATA_LIST_FLAG: {
                    Node *head = (Node *) ((uint64_t)raw);  // O RP2040 eh 32bit, mas a IDE nao sabe disso ent **SE A IDE** estiver dando erro
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

        tight_loop_contents();
    }
}


[[noreturn]] void measuring_loop_blocking() {
    auto head = static_cast<Node *>(malloc(sizeof(Node)));
    head->next = nullptr;
    Node* currentNode = head;

    createdNodes++;

    int count = 0;
    bool canSendData = true;

    float lastMeasuredWindSpeed = 0;

    uint64_t lastWindMeasureTime = time_us_64();
    uint64_t lastTime = time_us_64();

    while (true) {
        const int rawValue = hx711_get_value(hx); //blocking
        const float value = 5.0 * 10.0 * ((static_cast<float>(rawValue) - static_cast<float>(36000.0)) / static_cast<float>(44000.0));

        /*if ((value & 0x00C00000) >= 0)  // Bit 23 == 1
            value |= 0xFF000000;*/

        LOG(Debug, "blocking value: %8f | RawValue: %8i", value, rawValue);

        uint64_t timeSinceLastMeasureUS = time_us_64() - lastWindMeasureTime;
        if (timeSinceLastMeasureUS >= WIND_SAMPLE_TIME_US) {
            // 1 click = 2pi rad
            // x clicks/s = x*2pi rad/s
            // v = w * r
            // w = x*2pi rad/s
            // v = x * 2pi * r m/s
            // v = (clicks * 2pi * r) / timeSinceLast

            // The line below makes the calculation above in double precision, but converts o single precision (float) to save space
            lastMeasuredWindSpeed = (float)(((double)windClicks * 2 * 3.1415926535 * WIND_RADIUS) / ((double)timeSinceLastMeasureUS / 1000000.0));

            LOG(Information, "Calculated wind speed is: %2.3f m/s", lastMeasuredWindSpeed);

            windClicks = 0;
            lastWindMeasureTime = time_us_64();
        }

        currentNode->point.data.hx711_value = value;
        currentNode->point.data.time = time_us_64() - recordingStartingTime;
        currentNode->point.data.wind_speed = lastMeasuredWindSpeed;
        count++;

        if (count >= MIN_DATA_BUFFER_LEN) {
            LOG(Verbose, "blocking value: %8f | raw Value %li", value, rawValue);
            LOG(Verbose, "Period time: %" PRIu64, time_us_64() - lastTime);
            LOG(Verbose, "Can Send %d", canSendData);
        }


        while (multicore_fifo_rvalid()) {
            switch (const uint32_t flag = multicore_fifo_pop_blocking()) {
                case DONT_SEND_DATA_FLAG:
                    LOG(Information, "Cant send data flag received");
                    canSendData = false;
                    break;
                case CAN_SEND_DATA_FLAG:
                    LOG(Information, "Send data flag received");
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

void data_list_received(Node* list) {
    multicore_fifo_push_blocking(DONT_SEND_DATA_FLAG);


    if (createdNodes >= MIN_DATA_BUFFER_LEN * 20)
        LOG(Error, "[ERROR - MEM LEAK] Possible memory leak detected!!! %d nodes currently existing!", createdNodes);

    char str[16];
    char windStr[16];
    LOG(Information, "Received new buffer, first value is %f and wind speed is %f", list->point.data.hx711_value, list->point.data.wind_speed);

    sprintf(str, "F: %06.3f", list->point.data.hx711_value);
    sprintf(windStr, "W: %06.3f", list->point.data.wind_speed);

    update_display(list);
    send_buffer(0);

    while (list != nullptr) {
        //LOG(Debug, "Reading (sizeof: %lu)", sizeof(DataPoint));

        if (recording) {
            write_as_csv_buffered(list->point.data.time, list->point.data.wind_speed, list->point.data.hx711_value);
            //write_bytes_buffered(list->point.bytes, sizeof(DataPoint));
        }

        //LOG(Debug, "clearing list value %i", list->point.data.hx711_value);
        Node* lastNode = list;
        list = list->next;

        //LOG(Debug, "Freeing node memory");
        free(lastNode);
        createdNodes--;
    }


    multicore_fifo_push_blocking(CAN_SEND_DATA_FLAG);
}


// This function receives the hardware interrupts in any GPIO
void gpio_interrupt_handler(uint gpio, uint32_t events) {
    LOG(Verbose, "GPIO %d %d, edge rise event is %d", gpio, events, (uint32_t)GPIO_IRQ_EDGE_RISE);
    if (gpio != WIND_GPIO) return;
    if ((events & (uint32_t)GPIO_IRQ_EDGE_RISE) != (uint32_t)GPIO_IRQ_EDGE_RISE) return;

    LOG(Debug, "Adding to wind counter");
    windClicks++;
}


// === COMPONENT INIT FUNCTIONS ===

/*
 * The core that calls this funcion is the one that will receive the interrupt
 */
void init_wind_measure() {
    gpio_set_dir(WIND_GPIO, GPIO_IN);
    gpio_pull_down(WIND_GPIO);
    gpio_set_irq_enabled_with_callback(WIND_GPIO, GPIO_IRQ_EDGE_RISE, true, &gpio_interrupt_handler);
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