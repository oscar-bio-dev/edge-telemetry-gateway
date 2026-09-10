idf_component_register(SRCS "src/cli_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES console nvs_flash ipc_transport)
