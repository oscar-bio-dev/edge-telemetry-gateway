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

if(NOT CONFIG_HW_ECDSA_ENABLE)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/dev_private_key.pem")
        message(WARNING "dev_private_key.pem not found. Copying dummy key for development.")
        execute_process(COMMAND cp "${CMAKE_CURRENT_LIST_DIR}/dev_private_key.pem.dummy" "${CMAKE_CURRENT_LIST_DIR}/dev_private_key.pem")
    endif()
    target_add_binary_data(${COMPONENT_LIB} "dev_private_key.pem" TEXT)
endif()
