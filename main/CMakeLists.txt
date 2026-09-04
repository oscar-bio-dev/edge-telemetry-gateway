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
)

if(NOT CONFIG_HW_ECDSA_ENABLE)
    target_add_binary_data(${COMPONENT_LIB} "dev_private_key.pem" TEXT)
endif()
