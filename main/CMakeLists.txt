idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        ipc_transport
        telemetry_decoder
        telemetry_buffer
        cloud_transport
        eth_manager
        companion_ota
        diagnostics
        storage_manager
        offline_spooler
)

