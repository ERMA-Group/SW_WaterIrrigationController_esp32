
# Shift Register CBK
add_library(M_ShiftRegister_Cbk STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shift_register_cbk.c
)

target_include_directories(M_ShiftRegister_Cbk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}              # local config headers
        # level up to application.h
        ${CMAKE_CURRENT_SOURCE_DIR}/..     # application headers
        ${CMAKE_CURRENT_SOURCE_DIR}/../../M_ShiftRegister/inc  # driver API headers
)
