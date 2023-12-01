// button.c
#include "button.h"
#include "pico/stdlib.h"

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

    // Initialize other buttons similarly...
}

// Check if any button is pressed and return its value
int check_buttons()
{
    if (gpio_get(RED_BUTTON_PIN) == 0)
        return 1;
    if (gpio_get(GREEN_BUTTON_PIN) == 0)
        return 2;
    if (gpio_get(YELLOW_BUTTON_PIN) == 0)
        return 3;
    if (gpio_get(BLUE_BUTTON_PIN) == 0)
        return 4;

    return 0;
}