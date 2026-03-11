
# # Normal CMake build
# add_library(M_BSW_ESP32_Cfg STATIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/gpio_cfg.cpp
# )

# target_include_directories(M_BSW_ESP32_Cfg
#     INTERFACE
#         ${CMAKE_CURRENT_SOURCE_DIR}              # local config headers
#     PUBLIC
#         ${CMAKE_CURRENT_SOURCE_DIR}/../../M_BSW_ESP32/inc  # driver API headers
# )

# # Link to ESP-IDF components to get access to their headers
# target_link_libraries(M_BSW_ESP32_Cfg
#     PUBLIC
#         idf::esp_timer
#         idf::driver
#         idf::freertos
# )