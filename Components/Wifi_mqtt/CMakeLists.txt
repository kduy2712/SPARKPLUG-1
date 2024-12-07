set(app_src Wifi_mqtt.c)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES  "mqtt"
                    REQUIRES "")