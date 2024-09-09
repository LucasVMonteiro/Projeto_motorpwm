#include <stdio.h>
#include <string.h>
#include "driver/ledc.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#include "esp_log.h"
#include "driver/gpio.h"

// defines pwm

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (4) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz

// defines gpio

#define BTN1 2
#define BTN2 14
#define BTN3 12
#define BTN4 13

// confiurando pinos gpio

#define GPIO_PIN1 (1ULL<<BTN1)
#define GPIO_PIN2 (1ULL<<BTN2)
#define GPIO_PIN3 (1ULL<<BTN3)
#define GPIO_PIN4 (1ULL<<BTN4)

char flag_pwm1_on = 0x00;
char switch_duty_freq = 0x00;// set duty = 0x00 / set freq = 0x01

uint32_t dutyIN = 4096;
//FIFO
QueueHandle_t filaDeInterrupcao;

// STRUCTS DE CONFIGURACAO DO PWC - LIB LEDC
// Prepare and then apply the LEDC PWM timer configuration
ledc_timer_config_t ledc_timer = {
    .speed_mode       = LEDC_MODE,
    .duty_resolution  = LEDC_DUTY_RES,
    .timer_num        = LEDC_TIMER,
    .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
    .clk_cfg          = LEDC_AUTO_CLK,
    .deconfigure      = false
};
// Prepare and then apply the LEDC PWM channel configuration
ledc_channel_config_t ledc_channel = {
    .speed_mode     = LEDC_MODE,
    .channel        = LEDC_CHANNEL,
    .timer_sel      = LEDC_TIMER,
    .intr_type      = LEDC_INTR_DISABLE,
    .gpio_num       = LEDC_OUTPUT_IO,
    .duty           = LEDC_DUTY, // Set duty to 0%
    .hpoint         = 0
};

static void ligar_pwm(void)
{
    // tratamento aqui
    //printf("-----------------------OKOKOK---------------------\n");
    if( flag_pwm1_on == 0x00 )
    {
        flag_pwm1_on = 0x01;
        //printf("Botao pressionado\n");
        // Set the LEDC peripheral configuration

        ledc_timer.deconfigure = false;
        //Ativando timer e channel
        //----------------------------------------------------|
        //            _____              _________            |
        //           |     |            |         |           |
        //  clkin -->|TIMER|---clkdiv-->| CHANNEL |---PWM-->  |
        //           |_____|            |_________|           |
        //----------------------------------------------------|
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));//gerando warn
        printf("ok\n");


    
        // Set duty to 50%
        //ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
        // Update duty to apply the new value
        //ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    }
    else
    {
        flag_pwm1_on = 0x00;
        //desligando canal pwm
        ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
        //desligando timer do pwm
        ledc_timer_pause(LEDC_MODE,LEDC_TIMER);
        ledc_timer.deconfigure = true;
        ledc_timer_config(&ledc_timer);
    }



}

static void IRAM_ATTR gpio_isr_handler(void *args)
{
    int pino = (int)args;
   // jogando o item pino para o final da fila "filaDeInterrupcao" 
    xQueueSendFromISR(filaDeInterrupcao, &pino, NULL);
    
}

void trataIntBtn(void *params){
    int pino;//ID do pino gpio
    
    while(true){
        //Acessando primeiro item da fila
        if(xQueueReceive(filaDeInterrupcao,&pino,portMAX_DELAY)){   

            // evitando bouncing
            int estado =  gpio_get_level(pino);
            if( estado == 1){

                //desativando interrupcoes
                gpio_isr_handler_remove(pino);
                //quando ocorrer bouncing, espere
                while(gpio_get_level(pino) == estado){
                    vTaskDelay(50/portTICK_PERIOD_MS);
                }

                if(pino == BTN1){
                    ligar_pwm();
                }
                else if(pino == BTN2){
                    if(switch_duty_freq == 0x00){
                        switch_duty_freq = 0x00;
                    }else if(switch_duty_freq == 0x01){
                        switch_duty_freq = 0x00;
                    }
                }
                else if(pino == BTN3){
                    if(switch_duty_freq == 0x00 && flag_pwm1_on == 0x01){
                        //atualizar dutycycle
                         if( dutyIN - 512 > 0){
                            dutyIN = dutyIN - 512;
                        }else{
                            dutyIN = 512;
                        }
                        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, dutyIN));
                        // Update duty to apply the new value
                        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
                    }
                }
                else if(pino == BTN4){
                    if(switch_duty_freq == 0x00 && flag_pwm1_on == 0x01){
                        //atualizar dutycycle
                        if( dutyIN + 512 < 8192){
                            dutyIN = dutyIN + 512;
                        }else{
                            dutyIN = 8000;
                        }
                        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, dutyIN));
                        // Update duty to apply the new value
                        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
                    }
                }

               // reativando interrupcoes
                vTaskDelay(50/portTICK_PERIOD_MS);
                gpio_isr_handler_add(pino,gpio_isr_handler, (void *) pino);
            }

        }
    }
}

void app_main(void)
{

    gpio_config_t configBTN = {};
    configBTN.mode = GPIO_MODE_INPUT;
    configBTN.pull_down_en = 1;
    configBTN.pull_up_en = 0;
    configBTN.intr_type = GPIO_INTR_POSEDGE;

    // configurando pino botao

    configBTN.pin_bit_mask = GPIO_PIN1;//bitmask btn1
    gpio_config(&configBTN);
     
    configBTN.pin_bit_mask = GPIO_PIN2;//bitmask btn2
    gpio_config(&configBTN);

    configBTN.pin_bit_mask = GPIO_PIN3;//bitmask btn3
    gpio_config(&configBTN);

    configBTN.pin_bit_mask = GPIO_PIN4;//bitmask btn4
    gpio_config(&configBTN);

    filaDeInterrupcao = xQueueCreate(10,sizeof(int));
    xTaskCreate(trataIntBtn,"TrataBotao",4096, NULL,1,NULL);


    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN1,gpio_isr_handler, (void *) BTN1);
    gpio_isr_handler_add(BTN2,gpio_isr_handler, (void *) BTN2);
    gpio_isr_handler_add(BTN3,gpio_isr_handler, (void *) BTN3);
    gpio_isr_handler_add(BTN4,gpio_isr_handler, (void *) BTN4);
    

}
