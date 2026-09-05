idf_component_register(
    SRCS "src/ipc_transport.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver esp_driver_gpio esp_driver_uart cobs_crc telemetry_decoder telemetry_buffer
)
