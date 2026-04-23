cmake_minimum_required(VERSION 3.16)
project(RTSChess CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── raylib ───────────────────────────────────────────────────────────────────
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ── mbedTLS ───────────────────────────────────────────────────────────────────
# Fetch before libdatachannel so our targets exist when find_package runs.
set(ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_DOCS     OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(mbedtls)

# CMAKE_FIND_PACKAGE_REDIRECTS_DIR (CMake ≥ 3.24) is consulted BEFORE any
# Find<Package>.cmake module.  We write a tiny redirect that creates the
# MbedTLS::* alias targets and sets MbedTLS_FOUND, so libdatachannel's
# FindMbedTLS.cmake (which has a bug: calls check_symbol_exists without
# including CheckSymbolExists) is never reached.
set(_fpr "${CMAKE_BINARY_DIR}/fpredirects")
file(MAKE_DIRECTORY "${_fpr}")
file(WRITE "${_fpr}/MbedTLS.cmake"
"set(MbedTLS_FOUND   TRUE)
set(MbedTLS_VERSION \"3.6.0\")
if(NOT TARGET MbedTLS::mbedtls)
    add_library(MbedTLS::mbedtls    ALIAS mbedtls)
    add_library(MbedTLS::mbedcrypto ALIAS mbedcrypto)
    add_library(MbedTLS::mbedx509   ALIAS mbedx509)
endif()
")
set(CMAKE_FIND_PACKAGE_REDIRECTS_DIR "${_fpr}")

# ── libdatachannel ────────────────────────────────────────────────────────────
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_MEDIA     ON CACHE BOOL "" FORCE)
set(USE_MBEDTLS  ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG        v0.21.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libdatachannel)

# ── executable ───────────────────────────────────────────────────────────────
add_executable(RTSChess
    src/main.cpp
    src/Piece.cpp
    src/Board.cpp
    src/Game.cpp
    src/NetHost.cpp
    src/NetClient.cpp
)

target_link_libraries(RTSChess PRIVATE raylib datachannel-static)
target_include_directories(RTSChess PRIVATE src ${libdatachannel_SOURCE_DIR}/include)

if(WIN32)
    target_link_libraries(RTSChess PRIVATE ws2_32 winmm)
endif()
