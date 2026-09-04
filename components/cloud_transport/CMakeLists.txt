idf_component_register(
    SRCS "src/cloud_transport.c"
         "src/jwt_generator.c"
         "src/gcp_pubsub.c"
    INCLUDE_DIRS "include"
    REQUIRES telemetry_decoder
    PRIV_REQUIRES esp_http_client mbedtls json nvs_flash offline_spooler esp_netif telemetry_buffer
)
