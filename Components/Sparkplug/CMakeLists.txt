set(app_src Sparkplug.c)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES  "tahu" "mosquitto" "main" "gpio_funcs" "modbusTCP" "modbusRTU"
                    REQUIRES "")