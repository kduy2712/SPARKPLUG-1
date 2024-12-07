set(app_src gpio_funcs.c)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES "driver" "Sparkplug" "main" "Wifi_mqtt" "mosquitto" "Init"
                    REQUIRES "")