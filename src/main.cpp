#include "raylib.h"
#include "Game.h"
#include <enet/enet.h>
#include <cstring>
#include <cstdio>

// ─────────────────────────────────────────────
//  Simple menu before the game starts:
//    H     → host (listen for one client)
//    J     → join (type an IP then Enter)
//    S / Enter → standalone (original single-screen mode)
//    Esc   → quit
// ─────────────────────────────────────────────

enum class MenuState { MAIN, JOIN_INPUT, IN_GAME };

int main() {
    const int SCREEN_W = 1280;
    const int SCREEN_H = 720;
    const int RENDER_W = 480;
    const int RENDER_H = 270;

    if (enet_initialize() != 0) {
        printf("Failed to initialise ENet.\n");
        return 1;
    }

    InitWindow(SCREEN_W, SCREEN_H, "RTSChess");
    SetTargetFPS(60);

    RenderTexture2D rt = LoadRenderTexture(RENDER_W, RENDER_H);
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);

    Game      game;
    MenuState menu    = MenuState::MAIN;
    char      ipBuf[64] = "192.168.";   // pre-filled prefix
    int       ipLen   = (int)strlen(ipBuf);

    // Helper: upscale the render texture to the window
    auto blitRT = [&]() {
        DrawTexturePro(
            rt.texture,
            { 0.f, 0.f, (float)RENDER_W, -(float)RENDER_H },
            { 0.f, 0.f, (float)SCREEN_W,  (float)SCREEN_H },
            { 0.f, 0.f }, 0.f, WHITE
        );
    };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ── Menu: MAIN ────────────────────────────────────────────────────
        if (menu == MenuState::MAIN) {
            if (IsKeyPressed(KEY_H)) {
                game.SetNetMode(NetMode::HOST);
                game.Init();
                menu = MenuState::IN_GAME;
            } else if (IsKeyPressed(KEY_J)) {
                menu = MenuState::JOIN_INPUT;
            } else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_ENTER)) {
                game.SetNetMode(NetMode::STANDALONE);
                game.Init();
                menu = MenuState::IN_GAME;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                break;
            }

            BeginDrawing();
                ClearBackground({20, 15, 35, 255});
                int cx = SCREEN_W / 2;
                DrawText("RTSChess",           cx - 100, 200, 48, {200, 100, 255, 255});
                DrawText("[H] Host a game",    cx - 130, 320, 28, WHITE);
                DrawText("[J] Join a game",    cx - 130, 360, 28, WHITE);
                DrawText("[S] Solo (same screen)", cx - 155, 400, 28, WHITE);
                DrawText("[Esc] Quit",         cx - 80,  440, 28, GRAY);
            EndDrawing();
            continue;
        }

        // ── Menu: JOIN — type the host's IP ───────────────────────────────
        if (menu == MenuState::JOIN_INPUT) {
            // Backspace
            if (IsKeyPressed(KEY_BACKSPACE) && ipLen > 0)
                ipBuf[--ipLen] = '\0';

            // Printable characters (raylib provides Unicode codepoint)
            int ch;
            while ((ch = GetCharPressed()) != 0) {
                if (ipLen < 63 && (ch == '.' || (ch >= '0' && ch <= '9'))) {
                    ipBuf[ipLen++] = (char)ch;
                    ipBuf[ipLen]   = '\0';
                }
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                menu = MenuState::MAIN;
            } else if (IsKeyPressed(KEY_ENTER) && ipLen > 0) {
                game.SetNetMode(NetMode::CLIENT, ipBuf);
                game.Init();
                menu = MenuState::IN_GAME;
            }

            BeginDrawing();
                ClearBackground({20, 15, 35, 255});
                int cx = SCREEN_W / 2;
                DrawText("Enter host IP address:", cx - 200, 280, 28, WHITE);
                DrawText(ipBuf,                   cx - 200, 330, 32, {200, 255, 180, 255});
                DrawText("_",                     cx - 200 + MeasureText(ipBuf, 32), 330, 32, {200, 255, 180, 200});
                DrawText("[Enter] Connect  [Esc] Back", cx - 210, 400, 22, GRAY);
            EndDrawing();
            continue;
        }

        // ── IN_GAME ───────────────────────────────────────────────────────
        if (IsKeyPressed(KEY_R)) game.Init();
        if (IsKeyPressed(KEY_ESCAPE)) {
            menu  = MenuState::MAIN;
            continue;
        }

        game.Update(dt);

        BeginTextureMode(rt);
            game.Draw();
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            blitRT();
        EndDrawing();
    }

    UnloadRenderTexture(rt);
    CloseWindow();
    enet_deinitialize();
    return 0;
}
