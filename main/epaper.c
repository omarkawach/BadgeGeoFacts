/* Panel protocol adapted from Gordon Williams' Espruino ePaper Badge driver:
 * https://github.com/espruino/Espruino/blob/master/libs/js/misc/epaper_badge.js
 * Source driver is Copyright Pur3 Ltd, Mozilla Public License 2.0.
 */
#include "epaper.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum { PIN_BUSY=0,PIN_RESET=1,PIN_LED_ENABLE=4,PIN_CLOCK=7,PIN_MOSI=8,PIN_CS=10,PIN_DC=21 };
static spi_device_handle_t panel;
static void pause_ms(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)+1); }
static esp_err_t transfer(const void *data,size_t length)
{
    const uint8_t *p=data;
    while(length) {
        size_t n=length>64 ? 64 : length;
        spi_transaction_t tx={.length=n*8,.tx_buffer=p};
        esp_err_t result=spi_device_polling_transmit(panel,&tx);
        if(result!=ESP_OK) return result;
        p+=n;length-=n;
    }
    return ESP_OK;
}
static esp_err_t send_command(uint8_t value)
{
    gpio_set_level(PIN_CS,0);gpio_set_level(PIN_DC,0);
    esp_err_t result=transfer(&value,1);gpio_set_level(PIN_CS,1);return result;
}
static esp_err_t send_data(const void *data,size_t length)
{
    gpio_set_level(PIN_CS,0);gpio_set_level(PIN_DC,1);
    esp_err_t result=transfer(data,length);gpio_set_level(PIN_CS,1);return result;
}
static esp_err_t command_data(uint8_t command,const uint8_t *data,size_t length)
{
    esp_err_t result=send_command(command);
    return result==ESP_OK ? send_data(data,length) : result;
}
static esp_err_t wait_ready(void)
{
    int64_t start=esp_timer_get_time();
    while(!gpio_get_level(PIN_BUSY)) {
        if(esp_timer_get_time()-start>90000000) return ESP_ERR_TIMEOUT;
        pause_ms(5);
    }
    return ESP_OK;
}

esp_err_t badge_epaper_setup(void)
{
    gpio_config_t outputs={.pin_bit_mask=(1ULL<<PIN_RESET)|(1ULL<<PIN_CS)|(1ULL<<PIN_DC)|(1ULL<<PIN_LED_ENABLE),.mode=GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&outputs),"EPAPER","GPIO setup failed");
    gpio_set_level(PIN_RESET,1);gpio_set_level(PIN_CS,1);gpio_set_level(PIN_DC,1);gpio_set_level(PIN_LED_ENABLE,0);
    gpio_config_t input={.pin_bit_mask=1ULL<<PIN_BUSY,.mode=GPIO_MODE_INPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&input),"EPAPER","BUSY setup failed");
    spi_bus_config_t bus={.mosi_io_num=PIN_MOSI,.miso_io_num=-1,.sclk_io_num=PIN_CLOCK,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=64};
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_DISABLED),"EPAPER","SPI bus setup failed");
    spi_device_interface_config_t device={.clock_speed_hz=4000000,.mode=0,.spics_io_num=-1,.queue_size=1};
    return spi_bus_add_device(SPI2_HOST,&device,&panel);
}

bool badge_epaper_ready(void) { return gpio_get_level(PIN_BUSY)!=0; }

esp_err_t badge_epaper_start(const FC3Frame *frame)
{
    /* Keep the maker's initialization values, including its 680-gate setting. */
    static const uint8_t init[]={
        0x00,2,0x2b,0x29, 0x06,4,0x0f,0x8b,0x93,0xc1,
        0x50,1,0x37, 0x30,1,0x08, 0x61,4,0x03,0x20,0x02,0xa8,
        0x62,8,0x76,0x76,0x76,0x5a,0x9d,0x8a,0x76,0x62,
        0x65,4,0,0,0,0, 0xe0,1,0x10, 0xe7,1,0xa4, 0xe9,1,1,
        0xef,1,1, 0xf6,1,0x20, 0xef,1,0, 0xe0,1,0x12,
        0xe6,1,92, 0xa5,1,0
    };
    pause_ms(20);gpio_set_level(PIN_RESET,0);pause_ms(10);gpio_set_level(PIN_RESET,1);pause_ms(10);
    ESP_RETURN_ON_ERROR(wait_ready(),"EPAPER","Reset timeout");
    for(size_t p=0;p<sizeof(init);) {
        uint8_t command=init[p++],length=init[p++];
        ESP_RETURN_ON_ERROR(command_data(command,init+p,length),"EPAPER","Panel init failed");p+=length;
    }
    ESP_RETURN_ON_ERROR(wait_ready(),"EPAPER","Init timeout");
    ESP_RETURN_ON_ERROR(send_command(0x04),"EPAPER","Power on failed");
    ESP_RETURN_ON_ERROR(wait_ready(),"EPAPER","Power on timeout");
    ESP_RETURN_ON_ERROR(send_command(0x10),"EPAPER","Frame command failed");
    for(int i=0;i<FC3_FRAME_CHUNKS;++i) ESP_RETURN_ON_ERROR(send_data(frame->chunks[i],FC3_FRAME_CHUNK_BYTES),"EPAPER","Frame transfer failed");
    const uint8_t zero=0;
    ESP_RETURN_ON_ERROR(command_data(0x12,&zero,1),"EPAPER","Refresh failed");
    pause_ms(10);
    return ESP_OK;
}

esp_err_t badge_epaper_finish(void)
{
    const uint8_t zero=0,a5=0xa5;
    ESP_RETURN_ON_ERROR(wait_ready(),"EPAPER","Refresh timeout");
    ESP_RETURN_ON_ERROR(command_data(0x02,&zero,1),"EPAPER","Power off failed");
    ESP_RETURN_ON_ERROR(wait_ready(),"EPAPER","Power off timeout");
    return command_data(0x07,&a5,1);
}
