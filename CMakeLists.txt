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
# Fetched before libdatachannel so the mbedtls/mbedcrypto/mbedx509 targets
# exist when our FindMbedTLS.cmake runs inside libdatachannel's configure step.
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

# ── libdatachannel ────────────────────────────────────────────────────────────
# Prepend our cmake/ dir to CMAKE_MODULE_PATH so our FindMbedTLS.cmake is
# found before libdatachannel's buggy one (which calls check_symbol_exists
# without including CheckSymbolExists).  libdatachannel only appends its
# cmake/Modules dir, so the prepended entry stays first in all child scopes.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

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
