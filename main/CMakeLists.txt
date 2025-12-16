idf_component_register(SRCS "healthy-mcu.c" "adc/adc.c" "gpio/gpio.c" "gpio/pwm.c" "uart/uart.c"
        INCLUDE_DIRS "." "adc" "gpio" "uart"
        PRIV_REQUIRES esp_adc esp_driver_uart esp_driver_gpio
)
