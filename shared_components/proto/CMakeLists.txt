idf_component_register(
    SRCS "telemetry.pb.c" "gateway_health.pb.c"
    INCLUDE_DIRS "."
    REQUIRES nanopb
)
