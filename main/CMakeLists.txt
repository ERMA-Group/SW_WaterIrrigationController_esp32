idf_component_register(SRCS "main.cpp"
                       REQUIRES 
                       spi_flash 
                       driver esp_wifi 
                       esp_event nvs_flash 
                       M_BSW_ESP32
                       INCLUDE_DIRS ""
                       #EMBED_TXTFILES "server_pubkey.pem"
                       )

# Link the plain CMake library into the main component
# COMPONENT_LIB is the target representing this component
# Link order matters: dependencies first, then libraries that depend on them
target_link_libraries(${COMPONENT_LIB} 
    PRIVATE 
    "-Wl,--start-group"
    SW_APP 
    "-Wl,--end-group"
)