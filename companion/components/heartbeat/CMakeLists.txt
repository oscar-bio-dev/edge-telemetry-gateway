idf_component_register(
    SRCS "src/heartbeat.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ipc_sender
)
