set(app_src Init.c)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES  "nvs_flash" "fatfs" "ds3231" "protocol_examples_common" "mqtt"
                    INCLUDE_DIRS "." "C:/Espressif/frameworks/esp-idf-v4.4.5/components" 
                    REQUIRES "")