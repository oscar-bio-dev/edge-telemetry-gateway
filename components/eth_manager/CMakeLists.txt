idf_component_register(
    SRCS "src/eth_manager.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_eth esp_netif esp_event
)
