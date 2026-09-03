idf_component_register(
    SRCS "src/telemetry_buffer.c"
    INCLUDE_DIRS "include"
    REQUIRES telemetry_decoder
    PRIV_REQUIRES cobs_crc
)
