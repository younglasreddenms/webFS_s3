idf_component_register(SRCS "main.c" "webfs.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash middleware_storage middleware_wifi middleware_http_file_server middleware_display bsp_button)
