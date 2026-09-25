idf_component_register(
    SRCS "src/espnow_receiver.c"
    INCLUDE_DIRS "include"
    REQUIRES cobs_crc mailbox
    PRIV_REQUIRES esp_wifi esp_event ipc_sender nvs_flash proto
)
