set(app_src master.c)
# idf_component_register(SRCS  "master.c" "modbus_params.c"
#                     INCLUDE_DIRS "." "C:/Espressif" 
#                     )
# idf_component_register(SRCS "master.c" "modbus_params.c"
#                     INCLUDE_DIRS "."
#                     REQUIRES "")

                    
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES  "nvs_flash" "fatfs" "ds3231" "protocol_examples_common" "freemodbus" "Sparkplug" "tahu" "mosquitto" "main"
                    INCLUDE_DIRS "." "C:/Espressif/frameworks/esp-idf-v4.4.5/components" 
                    REQUIRES "")