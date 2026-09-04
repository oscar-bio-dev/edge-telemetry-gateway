idf_component_register(
    SRCS "telemetry.pb.c"
    INCLUDE_DIRS "."
    REQUIRES nanopb
)

nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS telemetry.proto)
