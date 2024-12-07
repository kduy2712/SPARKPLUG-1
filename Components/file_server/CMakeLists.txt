idf_component_register(SRCS "file_server.c" 
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES "spiffs" "esp_http_server"
                    EMBED_FILES "favicon.ico" "upload_script.html")
