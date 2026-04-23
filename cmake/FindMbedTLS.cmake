# FindMbedTLS.cmake — project-local override
#
# Replaces libdatachannel's FindMbedTLS.cmake, which has a bug in v0.21.0:
# it calls check_symbol_exists() without including CheckSymbolExists first.
#
# This file is found first because ${CMAKE_SOURCE_DIR}/cmake is prepended to
# CMAKE_MODULE_PATH before FetchContent_MakeAvailable(libdatachannel) runs.
# libdatachannel only appends its cmake/Modules dir, so ours stays ahead.
#
# Prerequisites: FetchContent_MakeAvailable(mbedtls) must already have run,
# which defines the mbedtls / mbedcrypto / mbedx509 CMake targets.

if(NOT TARGET mbedtls)
    message(FATAL_ERROR
        "FindMbedTLS: 'mbedtls' target not found. "
        "Call FetchContent_MakeAvailable(mbedtls) before find_package(MbedTLS).")
endif()

set(MbedTLS_VERSION      "3.6.0")
set(MbedTLS_INCLUDE_DIRS "${mbedtls_SOURCE_DIR}/include")
set(MbedTLS_FOUND        TRUE)
set(MBEDTLS_FOUND        TRUE)

# Create every namespaced target variant libdatachannel may link against.
# v0.21.0 uses MbedTLS::MbedTLS (capital); older versions use lowercase.
if(NOT TARGET MbedTLS::MbedTLS)
    add_library(MbedTLS::MbedTLS    ALIAS mbedtls)   # capital — v0.21+
endif()
if(NOT TARGET MbedTLS::mbedtls)
    add_library(MbedTLS::mbedtls    ALIAS mbedtls)   # lowercase — older
endif()
if(NOT TARGET MbedTLS::mbedcrypto)
    add_library(MbedTLS::mbedcrypto ALIAS mbedcrypto)
endif()
if(NOT TARGET MbedTLS::mbedx509)
    add_library(MbedTLS::mbedx509   ALIAS mbedx509)
endif()

# Honour any version requirement from the caller (libdatachannel asks for >= 3).
if(MbedTLS_FIND_VERSION AND
   MbedTLS_VERSION VERSION_LESS MbedTLS_FIND_VERSION)
    set(MbedTLS_FOUND FALSE)
    set(MbedTLS_NOT_FOUND_MESSAGE
        "Requested MbedTLS >= ${MbedTLS_FIND_VERSION}, have ${MbedTLS_VERSION}")
endif()
