#include "module.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "buttons_evt.h"

#if NO_BUTTONS == 0

#define CONFIG_BUTTON 25U
static const char *TAG="BTN";

ESP_EVENT_DEFINE_BASE(BUTTONS_BASE);
static xQueueHandle gpio_evt_queue = NULL;

typedef struct
{
	uint32_t num;
	bool val;
} btn_val_t;


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    btn_val_t *val = malloc(sizeof(btn_val_t));

    val->num = (uint32_t) arg;
    val->val = gpio_get_level(val->num);

    xQueueSendFromISR(gpio_evt_queue, &val, NULL);
}


static void btn_task(void* arg)
{
	bool prev_val = true;
    btn_val_t *val = NULL;

    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &val, portMAX_DELAY)) {
            if (prev_val != val->val) //TODO need some enchancments
            {
            	ESP_LOGI(TAG, "GPIO[%d] intr, val: %d",val->num, val->val);
            	prev_val = val->val;
            	if (val->val)
		    	{
		    		ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, BUTTONS_BASE, BUTTON_PRESS, &val->num, sizeof(uint32_t), portMAX_DELAY));
		    	} else
		    	{
		       		ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, BUTTONS_BASE, BUTTON_UNPRESS, &val->num, sizeof(uint32_t), portMAX_DELAY));	
		    	}
            }
		    free(val);
        }
    }
}


static void buttons_init(void)
{
	static const gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL<<CONFIG_BUTTON,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };


    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(btn_val_t *));

    xTaskCreate(btn_task, "btn_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Buttons started...");
    gpio_config(&button_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_remove(CONFIG_BUTTON);
    gpio_isr_handler_add(CONFIG_BUTTON, gpio_isr_handler, (void*) CONFIG_BUTTON);
}

module_init(buttons_init);
#endif /* #if NO_BUTTONS == 0 */