idf_component_register(
    SRCS "src/ipc_sender.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver cobs_crc
)
