# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)


set(EXTRA_COMPONENT_DIRS
    components/BSP/bsp_button
    components/BSP/bsp_oled
    components/middlewares/middleware_display
    components/middlewares/middleware_storage
    components/middlewares/middleware_wifi
    components/middlewares/middleware_http_file_server
)
project(webFS_s3)
