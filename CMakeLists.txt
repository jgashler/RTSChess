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

# ── libdatachannel (WebRTC DataChannels — P2P networking, no relay server) ───
# NO_WEBSOCKET and NO_MEDIA trim ~50% of the build we don't need.
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_MEDIA     ON CACHE BOOL "" FORCE)
set(USE_MBEDTLS  ON CACHE BOOL "" FORCE)   # static TLS — no OpenSSL DLLs to ship
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
