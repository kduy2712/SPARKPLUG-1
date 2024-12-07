# idf_component_register(SRCS "ethernet_example_main.c" "modbus_params.c" 
#                 INCLUDE_DIRS "." 
#               #   "C:/Espressif/frameworks/esp-idf-v4.4.5/components" "C:/Espressif/frameworks/esp-idf-v4.4.5/examples" 
#                        )
set(app_src ethernet_example_main.c )
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES "driver" "Sparkplug" "main" "Wifi_mqtt" "mosquitto" "Init" "tahu" "mosquitto" 
                    REQUIRES "")