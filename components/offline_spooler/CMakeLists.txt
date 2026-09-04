idf_component_register(
    SRCS "src/offline_spooler.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES cobs_crc storage_manager
)
