# MATCLI/
cmake_minimum_required (VERSION 3.20)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/307lib/307modules")

set(ENV{MATCLI_VERSION} "0.0.0")
include(VersionTag)
GET_VERSION_TAG("${CMAKE_CURRENT_SOURCE_DIR}" MATCLI)

project ("MATCLI" VERSION "${MATCLI_VERSION}" LANGUAGES CXX)

add_subdirectory ("307lib")
add_subdirectory ("pow")
