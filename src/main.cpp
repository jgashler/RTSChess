#include "Game.h"
#include "raylib.h"
#include <cstring>
#include <cstdio>
#include <string>

// ── Menu states ────────────────────────────────────────────────────────────
// HOST_SETUP  — generate WebRTC offer, show it, accept pasted answer
// JOIN_SETUP  — accept pasted offer, show generated answer, wait
enum class MenuState { MAIN, HOST_SETUP, JOIN_SETUP, IN_GAME };

// ── Draw a rounded status pill: e.g. "● Offer ready  (412 chars)" ──────────
static void DrawStatusPill(const char* label, int chars, int x, int y, Color col) {
    char buf[80];
    if (chars > 0)
        snprintf(buf, sizeof(buf), "%s  (%d chars)", label, chars);
    else
        snprintf(buf, sizeof(buf), "%s", label);
    DrawText(buf, x, y, 18, col);
}

int main() {
    const int SCREEN_W = 1280;
    const int SCREEN_H = 720;
    const int RENDER_W = 480;
    const int RENDER_H = 270;

    InitWindow(SCREEN_W, SCREEN_H, "RTSChess");
    SetTargetFPS(60);

    RenderTexture2D rt = LoadRenderTexture(RENDER_W, RENDER_H);
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);

    Game      game;
    MenuState menu = MenuState::MAIN;

    // ── WebRTC signaling state (used in HOST_SETUP / JOIN_SETUP) ───────────
    std::string genSdp;       // SDP we generated (offer or answer)
    std::string pasteSdp;     // SDP the user pasted
    bool        connecting  = false;   // offer/answer exchanged, waiting for link
    bool        sdpCopied   = false;
    float       copiedTimer = 0.f;

    auto resetSignal = [&]() {
        genSdp.clear();
        pasteSdp.clear();
        connecting  = false;
        sdpCopied   = false;
        copiedTimer = 0.f;
    };

    auto blitRT = [&]() {
        DrawTexturePro(
            rt.texture,
            { 0.f, 0.f, (float)RENDER_W, -(float)RENDER_H },
            { 0.f, 0.f, (float)SCREEN_W,  (float)SCREEN_H },
            { 0.f, 0.f }, 0.f, WHITE
        );
    };

    // Colours
    const Color BG    = {20, 15, 35, 255};
    const Color TITLE = {200, 100, 255, 255};
    const Color HINT  = {180, 255, 180, 255};
    const Color DIM   = {130, 130, 130, 255};
    const Color WARN  = {255, 200, 80, 255};

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int   cx = SCREEN_W / 2;

        // ── MAIN ────────────────────────────────────────────────────────────
        if (menu == MenuState::MAIN) {
            if (IsKeyPressed(KEY_H)) {
                resetSignal();
                game.SetNetMode(NetMode::HOST);
                menu = MenuState::HOST_SETUP;
            } else if (IsKeyPressed(KEY_J)) {
                resetSignal();
                menu = MenuState::JOIN_SETUP;
            } else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_ENTER)) {
                game.SetNetMode(NetMode::STANDALONE);
                game.Init();
                menu = MenuState::IN_GAME;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                break;
            }

            BeginDrawing();
                ClearBackground(BG);
                DrawText("RTSChess",              cx - 100, 200, 48, TITLE);
                DrawText("[H] Host a game",        cx - 130, 320, 28, WHITE);
                DrawText("[J] Join a game",        cx - 130, 360, 28, WHITE);
                DrawText("[S] Solo (same screen)", cx - 155, 400, 28, WHITE);
                DrawText("[Esc] Quit",             cx -  80, 440, 28, DIM);
            EndDrawing();
            continue;
        }

        // ── HOST_SETUP ──────────────────────────────────────────────────────
        // Flow:
        //   1. Wait for offer generation  (genSdp empty)
        //   2. Show offer + accept paste  (genSdp ready, !connecting)
        //   3. Connecting…                (connecting == true)
        //   4. Connected → IN_GAME
        if (menu == MenuState::HOST_SETUP) {
            game.PollNet();

            // Fetch generated offer when ready
            if (genSdp.empty()) genSdp = game.GetOffer();

            // Once the answer is submitted, wait for WebRTC handshake
            if (connecting) {
                if (game.IsNetConnected()) {
                    game.Init();
                    menu = MenuState::IN_GAME;
                }
            }

            // Paste detection (Ctrl+V)
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            if (ctrl && IsKeyPressed(KEY_V)) {
                const char* clip = GetClipboardText();
                if (clip && clip[0] != '\0') pasteSdp = clip;
            }

            // Copy offer to clipboard
            if (!genSdp.empty() && ctrl && IsKeyPressed(KEY_C)) {
                SetClipboardText(genSdp.c_str());
                sdpCopied   = true;
                copiedTimer = 2.f;
            }
            if (sdpCopied) { copiedTimer -= dt; if (copiedTimer <= 0) sdpCopied = false; }

            // Submit answer → start connecting
            if (!connecting && !pasteSdp.empty() && IsKeyPressed(KEY_ENTER)) {
                game.SetAnswer(pasteSdp);
                connecting = true;
            }

            // Back
            if (IsKeyPressed(KEY_ESCAPE)) {
                game.SetNetMode(NetMode::STANDALONE);
                menu = MenuState::MAIN;
            }

            // ── Draw ────────────────────────────────────────────────────────
            BeginDrawing();
                ClearBackground(BG);
                DrawText("Host a Game", cx - 110, 50, 36, TITLE);

                if (connecting) {
                    DrawText("Connecting...", cx - 85, 290, 28, WARN);
                    DrawText("Waiting for peer-to-peer link", cx - 165, 330, 20, DIM);
                    DrawText("[Esc]  Cancel", cx - 65, 500, 18, DIM);

                } else if (genSdp.empty()) {
                    DrawText("Generating your Offer Code...", cx - 170, 290, 22, DIM);
                    DrawText("(this takes a few seconds)", cx - 140, 320, 16, DIM);
                    DrawText("[Esc]  Back", cx - 55, 500, 18, DIM);

                } else {
                    // Step 1 — copy offer
                    DrawText("Step 1", cx - 240, 130, 14, DIM);
                    DrawText("Copy your Offer Code and send it to your friend.",
                             cx - 240, 150, 20, WHITE);
                    DrawStatusPill("Offer Code ready",
                                   (int)genSdp.size(), cx - 240, 182,
                                   Color{140,255,160,255});
                    if (sdpCopied)
                        DrawText("Copied!  \xE2\x9C\x93", cx - 240, 210, 18, Color{100,255,120,255});
                    else
                        DrawText("[Ctrl+C]  Copy Offer Code", cx - 240, 210, 18, WARN);

                    // Step 2 — paste answer
                    DrawText("Step 2", cx - 240, 280, 14, DIM);
                    DrawText("Paste your friend's Answer Code, then press Enter.",
                             cx - 240, 300, 20, WHITE);
                    if (pasteSdp.empty()) {
                        DrawText("[Ctrl+V]  Paste Answer Code", cx - 240, 330, 18, DIM);
                    } else {
                        DrawStatusPill("Answer Code pasted",
                                       (int)pasteSdp.size(), cx - 240, 330,
                                       Color{140,255,160,255});
                        DrawText("[Enter]  Connect", cx - 240, 358, 18, WARN);
                    }
                    DrawText("[Esc]  Back", cx - 55, 500, 18, DIM);
                }
            EndDrawing();
            continue;
        }

        // ── JOIN_SETUP ──────────────────────────────────────────────────────
        // Flow:
        //   1. Paste host offer          (pasteSdp empty)
        //   2. Enter → SetNetMode(CLIENT, offer) → wait for answer generation
        //   3. Show answer + copy        (genSdp ready)
        //   4. Connecting automatically while host pastes our answer
        //   5. Connected → IN_GAME
        if (menu == MenuState::JOIN_SETUP) {
            game.PollNet();

            // Fetch generated answer when ready
            if (genSdp.empty()) genSdp = game.GetAnswer();

            // Auto-transition when connected
            if (game.IsNetConnected()) {
                game.Init();
                menu = MenuState::IN_GAME;
            }

            // Paste detection
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            if (ctrl && IsKeyPressed(KEY_V)) {
                const char* clip = GetClipboardText();
                if (clip && clip[0] != '\0') pasteSdp = clip;
            }

            // Submit offer → triggers answer generation
            if (!connecting && !pasteSdp.empty() && IsKeyPressed(KEY_ENTER)) {
                game.SetNetMode(NetMode::CLIENT, pasteSdp.c_str());
                connecting = true;
            }

            // Copy answer to clipboard
            if (!genSdp.empty() && ctrl && IsKeyPressed(KEY_C)) {
                SetClipboardText(genSdp.c_str());
                sdpCopied   = true;
                copiedTimer = 2.f;
            }
            if (sdpCopied) { copiedTimer -= dt; if (copiedTimer <= 0) sdpCopied = false; }

            // Back
            if (IsKeyPressed(KEY_ESCAPE)) {
                game.SetNetMode(NetMode::STANDALONE);
                menu = MenuState::MAIN;
            }

            // ── Draw ────────────────────────────────────────────────────────
            BeginDrawing();
                ClearBackground(BG);
                DrawText("Join a Game", cx - 105, 50, 36, TITLE);

                if (!connecting) {
                    // Step 1 — paste host offer
                    DrawText("Step 1", cx - 240, 130, 14, DIM);
                    DrawText("Paste the host's Offer Code, then press Enter.",
                             cx - 240, 150, 20, WHITE);
                    if (pasteSdp.empty()) {
                        DrawText("[Ctrl+V]  Paste Offer Code", cx - 240, 182, 18, DIM);
                    } else {
                        DrawStatusPill("Offer Code pasted",
                                       (int)pasteSdp.size(), cx - 240, 182,
                                       Color{140,255,160,255});
                        DrawText("[Enter]  Process Offer", cx - 240, 210, 18, WARN);
                    }
                    DrawText("[Esc]  Back", cx - 55, 500, 18, DIM);

                } else if (genSdp.empty()) {
                    DrawText("Generating your Answer Code...", cx - 170, 290, 22, DIM);
                    DrawText("(this takes a few seconds)", cx - 140, 320, 16, DIM);

                } else {
                    // Step 2 — copy answer
                    DrawText("Step 2", cx - 240, 130, 14, DIM);
                    DrawText("Copy your Answer Code and send it to the host.",
                             cx - 240, 150, 20, WHITE);
                    DrawStatusPill("Answer Code ready",
                                   (int)genSdp.size(), cx - 240, 182,
                                   Color{140,255,160,255});
                    if (sdpCopied)
                        DrawText("Copied!  \xE2\x9C\x93", cx - 240, 210, 18, Color{100,255,120,255});
                    else
                        DrawText("[Ctrl+C]  Copy Answer Code", cx - 240, 210, 18, WARN);

                    DrawText("Waiting for host to connect...", cx - 240, 300, 20, DIM);
                    DrawText("[Esc]  Cancel", cx - 65, 500, 18, DIM);
                }
            EndDrawing();
            continue;
        }

        // ── IN_GAME ─────────────────────────────────────────────────────────
        if (IsKeyPressed(KEY_R)) game.Init();
        if (IsKeyPressed(KEY_ESCAPE)) {
            menu = MenuState::MAIN;
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
    return 0;
}
