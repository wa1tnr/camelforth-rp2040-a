target_sources(forth
            ${CMAKE_CURRENT_LIST_DIR}/forth.c
            )

# target_link_libraries(forth pico_stdlib)
target_link_libraries(camelforth-a forth pico_stdlib)

# enable usb output, disable uart output - input as well?
pico_enable_stdio_usb(forth 1)
pico_enable_stdio_uart(forth 1)
# add_library(pico_stdlib)
