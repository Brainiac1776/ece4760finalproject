// button.c
#include "button.h"
// #include "RED.h"
// #include "BLUE.h"
// #include "GREEN.h"
// #include "YELLOW.h"
#include "pico/stdlib.h"

//Send sound to the DAC 
/*
void redSound()
{
 for (int i = 0; i < sizeof(RED_wav)){
     DAC_output_0 = i;
     DAC_data_0 = DAC_config_channel B |  DAC_output_0 & 0xffff
     spi_write16_blocking(SPI_PORT, &DACdata_0, 1)
 }
}

void blueSound()
{
 for (int i = 0; i < sizeof(BLUE_wav)){
     DAC_output_0 = i;
     DAC_data_0 = DAC_config_channel B |  DAC_output_0 & 0xffff
     spi_write16_blocking(SPI_PORT, &DACdata_0, 1)
 }
}

void greenSound()
{
 for (int i = 0; i < sizeof(GREEN_wav)){
     DAC_output_0 = i;
     DAC_data_0 = DAC_config_channel B |  DAC_output_0 & 0xffff
     spi_write16_blocking(SPI_PORT, &DACdata_0, 1)
 }
}

void yellowSound()
{
 for (int i = 0; i < sizeof(YELLOW_wav)){
     DAC_output_0 = i;
     DAC_data_0 = DAC_config_channel B |  DAC_output_0 & 0xffff
     spi_write16_blocking(SPI_PORT, &DACdata_0, 1)
 }
}
*/

// Initialize buttons
void init_buttons()
{
    gpio_init(RED_BUTTON_PIN);
    gpio_set_dir(RED_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(RED_BUTTON_PIN);

    gpio_init(BLUE_BUTTON_PIN);
    gpio_set_dir(BLUE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BLUE_BUTTON_PIN);

    gpio_init(YELLOW_BUTTON_PIN);
    gpio_set_dir(YELLOW_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(YELLOW_BUTTON_PIN);

    gpio_init(GREEN_BUTTON_PIN);
    gpio_set_dir(GREEN_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(GREEN_BUTTON_PIN);

}

// Check if any button is pressed and return its value
int check_buttons()
{
    if (gpio_get(RED_BUTTON_PIN) == 0) {
        //redSound();
        return 1;
    }
    if (gpio_get(GREEN_BUTTON_PIN) == 0){
        //blueSound();
        return 2;
    }
    if (gpio_get(YELLOW_BUTTON_PIN) == 0){
        //yellowSound();
        return 3;
    }
    if (gpio_get(BLUE_BUTTON_PIN) == 0){
       // greenSound();
        return 4;
    }

    return 0;
}
