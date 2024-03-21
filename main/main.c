/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "ssd1306.h"
#include "gfx.h"
#include "pico/stdlib.h"
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include <string.h>

const int TRIG_PIN = 17;
const int ECHO_PIN = 16;

int SOUND_SPEED = 340;

volatile bool measurement_enabled = true;


volatile absolute_time_t time_init;
volatile absolute_time_t time_end;
static volatile bool timer_fired = false;
volatile bool sec_fired = false; 

const uint ECHO = 16;
const uint TRIG = 17;


static void alarm_callback(alarm_id_t id, void *user_data) {
    timer_fired = true;
}

static void gpio_callback(uint gpio, uint32_t events) {
    if (events == GPIO_IRQ_EDGE_RISE) {
        time_init = get_absolute_time();
    } else if (events == GPIO_IRQ_EDGE_FALL) {
        time_end = get_absolute_time();
    }
}

void print_log(absolute_time_t timestamp, float distance) {
    uint64_t time_microseconds;
    memcpy(&time_microseconds, &timestamp, sizeof(uint64_t));
    int32_t time_seconds = time_microseconds / 1000000;
    time_microseconds %= 1000000;
    printf("%02d:%02d:%02d - %.2lf cm\n", time_seconds / 3600, (time_seconds % 3600) / 60, time_seconds % 60, distance);
} 

void echo_task(void *p) {
    while (1) {
        uint32_t start = time_us_32();
        while (gpio_get(ECHO_PIN) == 0) {
            start = time_us_32();
        }
        uint32_t end = time_us_32();
        while (gpio_get(ECHO_PIN) == 1) {
            end = time_us_32();
        }
        uint32_t pulse_width = end - start;
        printf("Pulse width: %d\n", pulse_width);
        sleep_ms(1000);
    }
}

void oled_task(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_callback);


    // configura o rtc para iniciar em um momento especifico
    datetime_t t = {
        .year  = 2020,
        .month = 01,
        .day   = 13,
        .dotw  = 3, // 0 is Sunday, so 3 is Wednesday
        .hour  = 11,
        .min   = 20,
        .sec   = 00
    };
    rtc_init();
    rtc_set_datetime(&t);

    // configura o alarme para disparar uma vez a cada segundo
    datetime_t alarm = {
        .year  = -1,
        .month = -1,
        .day   = -1,
        .dotw  = -1,
        .hour  = -1,
        .min   = -1,
        .sec   = 01 
    };

    rtc_set_alarm(&alarm, &alarm_callback);

    int index = 0;
    float distancia_cm;


    char cnt = 15;
    while (1) {
        int c = getchar_timeout_us(20);

        gpio_put(TRIG_PIN, 1); 
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);

        // calcula a duracao do pulso de eco recebido pelo sensor. time_end e time_init marcam a subida e descida de borda
        int ultrassom_duration = absolute_time_diff_us(time_end, time_init);

        //distabcia = velocidade * duracan
        distancia_cm = 0.017015 * ultrassom_duration * (-1);

        // horario da medicao e medida
        print_log(time_init, distancia_cm);

        timer_fired = false; 
        sleep_ms(1000); 

        char str[10];
        sprintf(str, "%d", distancia_cm);


        while (1) {
            uint32_t start = time_us_32();
            while (gpio_get(ECHO_PIN) == 0) {
                printf("Waiting for echo\n");
                start = time_us_32();
                printf("Start: %d\n", start);
            }
            uint32_t end = time_us_32();
            while (gpio_get(ECHO_PIN) == 1) {
                printf("Waiting for end\n");
                end = time_us_32();
                printf("End: %d\n", end);
            }
            uint32_t pulse_width = end - start;
            printf("Pulse width: %d\n", pulse_width);
            sleep_ms(1000);

            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "DISTANCIA");
            gfx_draw_string(&disp, 0, 10, 1, "0");
            int max_start = 10 + 10;
            gfx_draw_string(&disp, max_start, 10, 1, "MAX");
            // gfx_draw_line(&disp, 15, 27, distancia_cm, 27);
            gfx_draw_line(&disp, 15, 27, cnt, 27);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // if (++cnt == 112)
        //     cnt = 15;

        gfx_show(&disp);
    }
}

void oled1_demo_1(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");

    char cnt = 15;
    while (1) {
        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, "Distancia");
        gfx_draw_string(&disp, 0, 10, 1, "sensor");
        gfx_draw_line(&disp, 15, 27, cnt,
                        27);
        vTaskDelay(pdMS_TO_TICKS(50));
        if (++cnt == 112)
            cnt = 15;

        gfx_show(&disp);
    }
}



int main() {
    stdio_init_all();
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_callback);




    xTaskCreate(oled_task, "Demo 1", 4095, NULL, 1, NULL);
    //xTaskCreate(echo_task, "Demo 1", 4095, NULL, 1, NULL);
    //xTaskCreate(oled1_demo_1, "Demo 2", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}