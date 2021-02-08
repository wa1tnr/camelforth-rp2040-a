add_executable(camelforth-a
        camelforth-a.c
        )

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(camelforth-a pico_stdlib)

# enable usb output, disable uart output - input as well?
pico_enable_stdio_usb(camelforth-a 1)
pico_enable_stdio_uart(camelforth-a 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(camelforth-a)

# add url via pico_set_program_url
example_auto_set_url(camelforth-a)
