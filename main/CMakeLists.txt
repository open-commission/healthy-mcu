idf_component_register(
        SRCS "healthy-mcu.c" "adc/adc.c" "gpio/gpio.c" "gpio/pwm.c"
        "uart/uart.c" "sppbt/spp_client.c" "sc/sr04.c"
        "max/algorithm.c" "max/blood.c" "max/max30102.c" "max/myi2c.c"
        "hx/710b.c" "wendu/hongwai.c" "util/delay.c" "global/vars.c" "tasks/task.c"

        INCLUDE_DIRS "." "adc" "gpio" "uart" "sppbt" "sc" "max" "hx" "wendu" "util"
        "global" "tasks"

        PRIV_REQUIRES bt esp_adc esp_driver_uart esp_driver_gpio esp_driver_i2c
        nvs_flash esp_driver_ledc
)
