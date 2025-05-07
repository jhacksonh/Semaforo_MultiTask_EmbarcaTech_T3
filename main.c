#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"

#include "lib/headers/leds_local.h"
#include "lib/headers/oled_local.h"
#include "lib/headers/buzzer_local.h"
#include "lib/headers/interrupt_local.h"

#define PIN_I2C_SDA 14
#define PIN_I2C_SCL 15
#define PIN_BT_A 5
#define PIN_BT_B 6
#define PIN_LEDS 7
#define PIN_BUZZER 21
bool NIGHT_MODE = false;
int COUNT_COLOR = -1;

uint RGB_LED[2] = {11,13};
uint8_t LEDS_ACTIVE[9] = {6,7,8,11,12,13,16,17,18};
uint8_t COLORS_GYR[4][3] = {{0,10,0},{10,10,0},{10,0,0},{0,0,0}};
int BUZZER_BEEPS[4][3] = {{3000,1000,5000},{2000,300,500},{1000,500,1500},{2000,300,2000}};
int TIMERS[3] = {5000,3000,500};
char *ALERT_MSG[3] = {"Pode Atravessar","Atencao","Pare"};


void Fill_Colors();
void vTraffic_light_RGBTask1();
void vTraffic_light_LedsTask2();
void vTraffic_light_BuzzerTask3();
void vTraffic_light_DisplayTask4();


void setup_config(uint pin, bool output){
    gpio_init(pin);
    gpio_set_dir(pin, output);
    gpio_put(pin,false);
}
// Trecho para modo BOOTSEL com botão B
void gpio_irq_handler(uint gpio, uint32_t events){
    if(!gpio_get(PIN_BT_B))reset_usb_boot(0, 0);
    if(!gpio_get(PIN_BT_A))NIGHT_MODE = !NIGHT_MODE;
}

int main(){
    stdio_init_all();
    for(int i = 0; i < sizeof(RGB_LED)/sizeof(RGB_LED[0]); i++)setup_config(RGB_LED[i], GPIO_OUT);
    Leds_init(PIN_LEDS,25);
    Fill_Colors();
    oled_Init(PIN_I2C_SDA, PIN_I2C_SCL);
    buzzer_init(PIN_BUZZER);
    itr_SetCallbackFunction(gpio_irq_handler);
    itr_Interruption(PIN_BT_A);
    itr_Interruption(PIN_BT_B);
    
    xTaskCreate(vTraffic_light_RGBTask1, "semaforo RGB_Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL);
    xTaskCreate(vTraffic_light_LedsTask2, "semaforo Leds_Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTraffic_light_BuzzerTask3, "semaforo Buzzer_Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTraffic_light_DisplayTask4, "semaforo Display_Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    vTaskStartScheduler();
    panic_unsupported();
}
uint8_t COLORS_TRAFFIC_LIGHT[4][9][3];
void Fill_Colors(){
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 9; j++) {
            memcpy(COLORS_TRAFFIC_LIGHT[i][j], COLORS_GYR[i], 4);
        }
    } 
}
void vTraffic_light_RGBTask1() {
    while (true) {
        COUNT_COLOR = (COUNT_COLOR + 1) % 3;//1,2,0,1,2(...)
        int time = (COUNT_COLOR == 1) ? TIMERS[1] : (NIGHT_MODE ? TIMERS[2] : TIMERS[0]);
        for (int i = 0; i < 3; i++) {
            bool on = (i == COUNT_COLOR && !NIGHT_MODE) || COUNT_COLOR == 1;
            gpio_put(RGB_LED[i == 2 ? 1 : i], on);
        }
        vTaskDelay(pdMS_TO_TICKS(time));
    }
}
void vTraffic_light_LedsTask2(){
    int last_index = -1;
    while(true){
        int index = NIGHT_MODE?(COUNT_COLOR!=1?3:COUNT_COLOR):COUNT_COLOR;
        vTaskDelay(pdMS_TO_TICKS(10));
        if(index != last_index){
            Leds_Map_leds_ON(LEDS_ACTIVE, COLORS_TRAFFIC_LIGHT[index],9,true);
        }
    }
}

void vTraffic_light_BuzzerTask3(){
    int last_index = -1;
    uint32_t last_time = 0;
    while (true){
        int index = NIGHT_MODE?(COUNT_COLOR!=1?1:COUNT_COLOR):COUNT_COLOR;
        vTaskDelay(pdMS_TO_TICKS(10));
        bool index_modified = index != last_index;
        uint32_t current_time = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual em microssegundos
        bool valid_time = current_time-last_time>BUZZER_BEEPS[NIGHT_MODE?3:index][2]*1000; // Verifica se o tempo de debounce foi atingido
        if(valid_time || index_modified){
            last_index = index;
            last_time = current_time;
            buzzer_play_note(BUZZER_BEEPS[index][0],BUZZER_BEEPS[index][1]);
        }
    }
}

void vTraffic_light_DisplayTask4(){
    int last_index = -1;
    while (true){
        int index = NIGHT_MODE?(COUNT_COLOR!=1?1:COUNT_COLOR):COUNT_COLOR;
        vTaskDelay(pdMS_TO_TICKS(10));
        if(index != last_index){
            last_index = index;
            oled_Clear();
            oled_Write_String(ALERT_MSG[index], 2, 27);
            oled_Update();
        }
    }
}