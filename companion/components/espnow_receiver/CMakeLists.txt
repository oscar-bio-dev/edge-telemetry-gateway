idf_component_register(
    SRCS "src/espnow_receiver.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_event ipc_sender
)
