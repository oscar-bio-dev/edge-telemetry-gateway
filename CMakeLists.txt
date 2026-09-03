cmake_minimum_required(VERSION 3.16)

# Shared components available to both Host and Companion
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/shared_components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(edge-telemetry-gateway)
