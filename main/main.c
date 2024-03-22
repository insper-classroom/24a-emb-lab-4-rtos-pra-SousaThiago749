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

//OLED
const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

//DISTANCIA E TIMER
const int TRIG_PIN = 16;
const int ECHO_PIN = 17;

int SOUND_SPEED = 340;

// volatile absolute_time_t start_time;
// volatile absolute_time_t end_time;

// volatile uint64_t start_time;
// volatile uint64_t end_time;


// criando uma fila

QueueHandle_t xQueue_time;
SemaphoreHandle_t xSemaphore_trigger;
QueueHandle_t xQueue_distance;


//OLED
void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void oled_task(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    while (1) {
        float distance;
        if ((xSemaphoreTake(xSemaphore_trigger, pdMS_TO_TICKS(100)) == pdTRUE) ){
            
            if (xQueueReceive(xQueue_distance, &distance, pdMS_TO_TICKS(50))){
                    size_t tamanho_float = sizeof(float);
                    int tamanho_float_int = (int)tamanho_float;
                    char distance_str[tamanho_float_int+3];
                    gfx_clear_buffer(&disp);
                    snprintf(distance_str, sizeof(distance_str), "%.2f", distance);
                    gfx_draw_string(&disp, 0, 0, 1, distance_str);
                    gfx_draw_line(&disp, 15, 27, distance, 27);
                    gfx_show(&disp);
                    vTaskDelay(pdMS_TO_TICKS(50));
            }
            
        }

    }
}



//DISTANCIA E TIMER
void pin_callback(uint gpio, uint32_t events) {
    static uint64_t start_time;
    static uint64_t end_time;
    if (gpio == ECHO_PIN) {
        if (gpio_get(ECHO_PIN)) {
            // ECHO_PIN mudou para alto
            start_time = to_us_since_boot(get_absolute_time());
        } else {
            // ECHO_PIN mudou para baixo
            end_time = to_us_since_boot(get_absolute_time());
            
            uint64_t duration = end_time - start_time;
            xQueueSendFromISR(xQueue_time, &duration, 0);
        }
    }
}


void send_pulse(){
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);
}

void trigger_task() {
    
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    while (true) {
        send_pulse();
        xSemaphoreGive(xSemaphore_trigger);
    }
    
}

void echo_sensor() {
    uint32_t duration;

    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_pull_up(ECHO_PIN);

    while (true) {
        if (xQueueReceive(xQueue_time, &duration, pdMS_TO_TICKS(50))) {
            float distance = duration / 58.0;
            xQueueSend(xQueue_distance, &distance, 0);
            printf("Distancia: %.2f cm\n", distance);
        }
    
    }

}

int main() {

    stdio_init_all();

    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);


    xSemaphore_trigger = xSemaphoreCreateBinary();
    xQueue_time = xQueueCreate(32, sizeof(uint64_t)); // Ajuste conforme necessário
    xQueue_distance = xQueueCreate(32, sizeof(float)); // Ajuste conforme necessário
    
    //task trigger:
    xTaskCreate(trigger_task, "Trigger Sensor", 4095, NULL, 1, NULL);

    //task echo:
    xTaskCreate(echo_sensor, "Echo Sensor", 4095, NULL, 1, NULL);

    //task oled:
    xTaskCreate(oled_task, "OLED", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true) {
        ;

    }
}