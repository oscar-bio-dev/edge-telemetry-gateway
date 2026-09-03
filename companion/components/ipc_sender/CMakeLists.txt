idf_component_register(
    SRCS "src/ipc_sender.c"
    INCLUDE_DIRS "include"
    REQUIRES cobs_crc
    PRIV_REQUIRES driver
)
