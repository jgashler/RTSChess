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

# ── mbedTLS (fetch ourselves so it's self-contained — no system install) ─────
# We fetch this BEFORE libdatachannel so we can create the MbedTLS::* targets
# that libdatachannel's find_package(MbedTLS) looks for.
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

# Create the namespaced alias targets BEFORE libdatachannel's find_package runs.
# FindMbedTLS.cmake checks `if(NOT TARGET MbedTLS::mbedtls)` before creating
# its own IMPORTED targets, so ours will be used instead.
if(NOT TARGET MbedTLS::mbedtls)
    add_library(MbedTLS::mbedtls    ALIAS mbedtls)
endif()
if(NOT TARGET MbedTLS::mbedcrypto)
    add_library(MbedTLS::mbedcrypto ALIAS mbedcrypto)
endif()
if(NOT TARGET MbedTLS::mbedx509)
    add_library(MbedTLS::mbedx509   ALIAS mbedx509)
endif()

# Pre-set the CACHE vars that FindMbedTLS.cmake uses so find_package passes
# without searching for prebuilt .lib/.a files on disk.
# The include dir points to our fetched source; the lib vars are set to
# non-NOTFOUND strings (target names) which pass the empty-check in
# find_package_handle_standard_args without requiring real file paths.
set(MbedTLS_INCLUDE_DIR    "${mbedtls_SOURCE_DIR}/include" CACHE PATH     "" FORCE)
set(MbedTLS_LIBRARY        "mbedtls"    CACHE FILEPATH "" FORCE)
set(MbedTLS_Crypto_LIBRARY "mbedcrypto" CACHE FILEPATH "" FORCE)
set(MbedTLS_X509_LIBRARY   "mbedx509"   CACHE FILEPATH "" FORCE)

# ── libdatachannel ───────────────────────────────────────────────────────────
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
