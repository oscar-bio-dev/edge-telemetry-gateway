idf_component_register(
    SRCS "src/cloud_transport.c"
         "src/jwt_generator.c"
         "src/gcp_pubsub.c"
    INCLUDE_DIRS "include"
    REQUIRES telemetry_decoder
    PRIV_REQUIRES esp_http_client mbedtls cjson nvs_flash offline_spooler esp_netif telemetry_buffer
)

if(NOT CONFIG_HW_ECDSA_ENABLE)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/dev_private_key.pem")
        message(WARNING "dev_private_key.pem not found. Copying dummy key for development.")
        execute_process(COMMAND cp "${CMAKE_CURRENT_LIST_DIR}/dev_private_key.pem.dummy" "${CMAKE_CURRENT_LIST_DIR}/dev_private_key.pem")
    endif()
    target_add_binary_data(${COMPONENT_LIB} "dev_private_key.pem" TEXT)
endif()
