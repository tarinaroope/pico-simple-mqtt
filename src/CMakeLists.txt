add_library(pico-simple-mqtt 
        wifihelper.c
        tcptransport.c
        mqttagent.c
        mqtttask.c
        mqttthing.c
        topichelper.c
        #main.c
        )
target_include_directories(pico-simple-mqtt
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        )

# pull in common dependencies
target_link_libraries(pico-simple-mqtt
        pico_stdlib                        # for core pico functionality
        pico_cyw43_arch_lwip_sys_freertos  # Wifi with FreeRTOS
        FreeRTOS-Kernel-Heap4              # FreeRTOS kernel and dynamic heap
        FREERTOS_PORT                      # FreeRTOS config files
        LWIP_PORT                          # LWIP config files
        # twinThingPicoW                     # twinThingPicoW helper lib
	coreMQTT                           # coreMQTT for FreeRTOS
	coreMQTTAgent                      # coreMQTT Agent for FreeRTOS
        )

# Setting up credentials. If it fails, check if you followed instructions in "example_device.cmake"
#include(../creds/device.cmake)

# Compile definitions needed by the project
target_compile_definitions(pico-simple-mqtt PRIVATE
    WIFI_SSID=\"${WIFI_SSID_SET}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD_SET}\"
    MQTT_CLIENT=\"${MQTT_CLIENT_SET}\"
    MQTT_USER=\"${MQTT_USER_SET}\"
    MQTT_PASSWD=\"${MQTT_PASSWD_SET}\"
    MQTT_HOST=\"${MQTT_HOST_SET}\"
    MQTT_PORT=${MQTT_PORT_SET}
)

        
# Enable print functionality on usb & disable on uart
pico_enable_stdio_usb(pico-simple-mqtt 0) 
pico_enable_stdio_uart(pico-simple-mqtt 1)

# create map/bin/hex file etc.
#pico_add_extra_outputs(${NAME})

