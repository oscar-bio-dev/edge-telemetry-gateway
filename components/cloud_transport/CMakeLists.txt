idf_component_register(
    SRCS "src/cloud_transport.c"
         "src/jwt_generator.c"
         "src/gcp_pubsub.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_client mbedtls json nvs_flash
)
