# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.13)

include(pico-sdk/pico_sdk_init.cmake)

project(pico_uart_bridge)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT PICO_BOARD)
	set(PICO_BOARD pico CACHE STRING "Target board")
endif()

pico_sdk_init()

add_executable(uart_bridge uart-bridge.c usb-descriptors.c bridge-led.c bridge-diagnostics.c)
pico_generate_pio_header(uart_bridge ${CMAKE_CURRENT_LIST_DIR}/bridge-rgb.pio)

# Keep the SDK/submodules pristine. Generate a hash-checked C source adaptation
# with asymmetric transfer buffers; never patch a compiled file or silently
# mutate upstream. A changed SDK source fails configuration until reviewed.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CDC_UPSTREAM ${PICO_SDK_PATH}/lib/tinyusb/src/class/cdc/cdc_device.c)
set(CDC_ADAPTED ${CMAKE_CURRENT_BINARY_DIR}/cdc_device_asymmetric.c)
execute_process(COMMAND ${Python3_EXECUTABLE}
	${CMAKE_CURRENT_LIST_DIR}/scripts/prepare_tinyusb.py ${CDC_UPSTREAM} ${CDC_ADAPTED}
	RESULT_VARIABLE CDC_PREPARE_RESULT)
if(NOT CDC_PREPARE_RESULT EQUAL 0)
	message(FATAL_ERROR "TinyUSB source adaptation failed; SDK review required")
endif()
set_source_files_properties(${CDC_UPSTREAM} PROPERTIES HEADER_FILE_ONLY ON)
target_sources(uart_bridge PRIVATE ${CDC_ADAPTED})
target_include_directories(uart_bridge PRIVATE ${PICO_SDK_PATH}/lib/tinyusb/src/class/cdc)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	${CDC_UPSTREAM} ${CMAKE_CURRENT_LIST_DIR}/scripts/prepare_tinyusb.py)

foreach(pin UART0_TX UART0_RX UART1_TX UART1_RX LED_PIN RGB_PIN UART0_CTS UART1_CTS UART0_RTS UART1_RTS UART0_DTR UART1_DTR)
	if(DEFINED BRIDGE_${pin})
		target_compile_definitions(uart_bridge PRIVATE BRIDGE_${pin}=${BRIDGE_${pin}})
	endif()
endforeach()

target_include_directories(uart_bridge PUBLIC
	./
	pico-sdk/lib/tinyusb/src)

target_link_libraries(uart_bridge
	hardware_flash
	hardware_pio
	pico_multicore
	pico_stdlib
	pico_usb_reset
	tinyusb_device)

target_compile_definitions(uart_bridge PRIVATE
	PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
	PICO_USB_RESET_INCLUDE_DEFAULT_APP_DRIVER_CB=0
	PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
	PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=4)

pico_add_extra_outputs(uart_bridge)
