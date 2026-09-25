idf_component_register(
    SRCS "src/mailbox.c"
    INCLUDE_DIRS "include"
    REQUIRES cobs_crc
    PRIV_REQUIRES freertos
)
