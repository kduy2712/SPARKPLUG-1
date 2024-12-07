set(app_src ds3231.c i2cdev.c)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS ""
                    PRIV_REQUIRES ""
                    REQUIRES "")

# idf_component_register(SRCS ${app_src}
#                     INCLUDE_DIRS "."
#                     REQUIRES ${pri_req}
#                     PRIV_REQUIRES main nvs_flash app_update)