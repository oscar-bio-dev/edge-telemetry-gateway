idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        espnow_receiver
        ipc_sender
        heartbeat
)
