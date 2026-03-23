# 1. Define the libraries
add_library(SW_APP STATIC 
    application.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/cloud/webservice_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cloud/device_claim_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cloud/server_sync_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cloud/home_assistant_mqtt_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/local_mode/local_mode_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/local_mode/local_mode_scheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/local_mode/local_mode_web.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/asw_cfg/global_credentials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/asw_cfg/mqtt_runtime_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/asw_cfg/operating_mode_config.cpp

    # Command Adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/command_handling/ca_bsw_esp32.cpp
)

# Create a configuration library for BSW_ESP32
add_library(M_BSW_ESP32_Cfg STATIC 
    bsw_cfg/gpio_cfg.cpp
    bsw_cfg/uart_cfg.cpp
) 

# 2. Provide ALL header paths manually
# We must go up one level (..) to reach the project root where M_StpLedDriver lives
set(BSW_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../M_BSW_ESP32/inc")
set(LED_DRIVER_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../M_ShiftRegister/inc")

# Now set the include directories for each target
target_include_directories(SW_APP PUBLIC 
    "." 
    "asw_cfg" 
    "bsw_cfg"
    ${BSW_INC_PATH}
    ${LED_DRIVER_INC_PATH}

    # Command Adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/command_handling
)

if(DEFINED IDF_PATH)
    target_include_directories(SW_APP PUBLIC "${IDF_PATH}/components/json/cJSON")
    target_include_directories(SW_APP PUBLIC "${IDF_PATH}/components/mbedtls/esp_crt_bundle/include")
    target_include_directories(SW_APP PUBLIC "${IDF_PATH}/components/mbedtls/mbedtls/include")
endif()

target_include_directories(M_BSW_ESP32_Cfg PUBLIC 
    "bsw_cfg"
    ${BSW_INC_PATH}
)

# 3. Link the binaries to solve the 'undefined reference' issues
# This tells the linker to look inside the compiled ESP-IDF components
target_link_libraries(M_BSW_ESP32_Cfg PUBLIC __idf_M_BSW_ESP32 idf::driver)
target_link_libraries(SW_APP PUBLIC 
    __idf_M_BSW_ESP32 
    __idf_json
    __idf_mqtt
    M_ShiftRegister
    M_ESTP
    M_BSW_ESP32_Cfg 
    idf::freertos 
    idf::driver
)
add_subdirectory(asw_cfg)
add_subdirectory(command_handling)