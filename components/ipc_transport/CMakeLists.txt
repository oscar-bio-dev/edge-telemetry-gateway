idf_component_register(
    SRCS "src/ipc_transport.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver cobs_crc
)
