#include "Game.h"
#include "Signaling.h"
#include "raylib.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <string>

enum class MenuState { MAIN, HOST_SETUP, JOIN_SETUP, IN_GAME };

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

    // ── Setup state shared between HOST_SETUP and JOIN_SETUP ──────────────
    std::string roomCode;       // 6-char code (generated for host, typed for joiner)
    std::string codeInput;      // raw text the joiner is typing
    std::string statusMsg;      // one-line status shown to user
    bool        sigBusy    = false;  // a background signaling call is in flight
    bool        connecting = false;  // WebRTC handshake in progress
    float       pollTimer  = 0.f;    // seconds until next /answer poll

    auto resetSetup = [&]() {
        roomCode.clear();
        codeInput.clear();
        statusMsg.clear();
        sigBusy    = false;
        connecting = false;
        pollTimer  = 0.f;
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
    const Color BG    = {20,  15,  35,  255};
    const Color TITLE = {200, 100, 255, 255};
    const Color DIM   = {130, 130, 130, 255};
    const Color WARN  = {255, 200,  80, 255};
    const Color COL_OK  = {140, 255, 160, 255};

    // Draw the 6 code cells (big letters, used on both screens)
    auto drawCodeCells = [&](const std::string& code, int cx, int y) {
        const int cellW = 64, cellH = 72, gap = 10;
        int totalW = 6 * cellW + 5 * gap;
        int startX = cx - totalW / 2;
        for (int i = 0; i < 6; i++) {
            int x = startX + i * (cellW + gap);
            DrawRectangle(x, y, cellW, cellH, Color{40, 30, 60, 255});
            DrawRectangleLines(x, y, cellW, cellH, Color{100, 60, 160, 255});
            if (i < (int)code.size()) {
                char ch[2] = {code[i], '\0'};
                DrawText(ch, x + 18, y + 14, 44, WHITE);
            }
        }
    };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int   cx = SCREEN_W / 2;

        Signaling::Poll();  // dispatch any background HTTP results

        // ── MAIN ────────────────────────────────────────────────────────────
        if (menu == MenuState::MAIN) {
            if (IsKeyPressed(KEY_H)) {
                resetSetup();
                game.SetNetMode(NetMode::HOST);
                statusMsg = "Generating connection data...";
                menu = MenuState::HOST_SETUP;
            } else if (IsKeyPressed(KEY_J)) {
                resetSetup();
                statusMsg = "Enter the room code from your friend:";
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
        //  1. Wait for WebRTC offer to be generated.
        //  2. POST offer → Worker → receive room code.
        //  3. Show room code; poll /answer every 2 s.
        //  4. Answer arrives → SetAnswer → wait for P2P link → IN_GAME.
        if (menu == MenuState::HOST_SETUP) {
            game.PollNet();

            // Step 1 → 2: offer ready, post it
            if (roomCode.empty() && !sigBusy) {
                std::string offer = game.GetOffer();
                if (!offer.empty()) {
                    sigBusy   = true;
                    statusMsg = "Registering room with server...";
                    Signaling::AsyncPostOffer(offer, [&](std::string code) {
                        sigBusy = false;
                        if (code.empty()) {
                            statusMsg = "Could not reach server. Check your connection.";
                        } else {
                            roomCode  = code;
                            statusMsg = "Share this code with your friend:";
                        }
                    });
                }
            }

            // Step 3: poll for answer every 2 seconds
            if (!roomCode.empty() && !connecting && !sigBusy) {
                pollTimer -= dt;
                if (pollTimer <= 0.f) {
                    pollTimer = 2.f;
                    sigBusy   = true;
                    Signaling::AsyncPollAnswer(roomCode, [&](std::string answer) {
                        sigBusy = false;
                        if (!answer.empty()) {
                            game.SetAnswer(answer);
                            connecting = true;
                            statusMsg  = "Opponent found! Connecting...";
                        }
                    });
                }
            }

            // Step 4: connected
            if (connecting && game.IsNetConnected()) {
                game.Init();
                menu = MenuState::IN_GAME;
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                game.SetNetMode(NetMode::STANDALONE);
                menu = MenuState::MAIN;
            }

            // ── Draw ────────────────────────────────────────────────────────
            BeginDrawing();
                ClearBackground(BG);
                DrawText("Host a Game", cx - 110, 60, 36, TITLE);
                DrawText(statusMsg.c_str(),
                         cx - MeasureText(statusMsg.c_str(), 20) / 2,
                         220, 20, DIM);

                if (!roomCode.empty()) {
                    drawCodeCells(roomCode, cx, 270);

                    // Copy-to-clipboard convenience
                    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
                    if (ctrl && IsKeyPressed(KEY_C)) SetClipboardText(roomCode.c_str());
                    DrawText("[Ctrl+C] Copy code",
                             cx - MeasureText("[Ctrl+C] Copy code", 18) / 2,
                             360, 18, WARN);

                    if (connecting) {
                        DrawText("Connecting...",
                                 cx - MeasureText("Connecting...", 22) / 2,
                                 420, 22, COL_OK);
                    } else {
                        DrawText("Waiting for opponent to join...",
                                 cx - MeasureText("Waiting for opponent to join...", 18) / 2,
                                 420, 18, DIM);
                    }
                }

                DrawText("[Esc] Back", cx - MeasureText("[Esc] Back", 18) / 2, 620, 18, DIM);
            EndDrawing();
            continue;
        }

        // ── JOIN_SETUP ──────────────────────────────────────────────────────
        // Flow:
        //  1. User types 6-char room code, presses Enter.
        //  2. GET /join/{code} → fetch offer SDP.
        //  3. Feed offer to WebRTC → wait for answer SDP.
        //  4. POST /answer/{code} with answer SDP.
        //  5. Wait for P2P link → IN_GAME.
        if (menu == MenuState::JOIN_SETUP) {
            game.PollNet();

            // Step 1: keyboard input for the room code
            if (roomCode.empty() && !sigBusy) {
                int c = GetCharPressed();
                while (c > 0) {
                    if (codeInput.size() < 6 && (isalpha(c) || isdigit(c)))
                        codeInput += (char)toupper(c);
                    c = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && !codeInput.empty())
                    codeInput.pop_back();

                if (IsKeyPressed(KEY_ENTER) && (int)codeInput.size() == 6) {
                    sigBusy   = true;
                    statusMsg = "Looking up room...";
                    Signaling::AsyncGetOffer(codeInput, [&](std::string offer) {
                        sigBusy = false;
                        if (offer.empty()) {
                            statusMsg = "Room not found. Check the code and try again.";
                        } else {
                            roomCode  = codeInput;
                            statusMsg = "Room found! Generating connection data...";
                            game.SetNetMode(NetMode::CLIENT, offer.c_str());
                        }
                    });
                }
            }

            // Step 3 → 4: answer ready, post it
            if (!roomCode.empty() && !connecting && !sigBusy) {
                std::string answer = game.GetAnswer();
                if (!answer.empty()) {
                    sigBusy   = true;
                    statusMsg = "Sending connection data to host...";
                    Signaling::AsyncPostAnswer(roomCode, answer, [&](bool ok) {
                        sigBusy    = false;
                        connecting = ok;
                        statusMsg  = ok ? "Waiting for host to connect..."
                                        : "Failed to reach server. Try again.";
                    });
                }
            }

            // Step 5: connected
            if (connecting && game.IsNetConnected()) {
                game.Init();
                menu = MenuState::IN_GAME;
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                game.SetNetMode(NetMode::STANDALONE);
                menu = MenuState::MAIN;
            }

            // ── Draw ────────────────────────────────────────────────────────
            BeginDrawing();
                ClearBackground(BG);
                DrawText("Join a Game", cx - 105, 60, 36, TITLE);
                DrawText(statusMsg.c_str(),
                         cx - MeasureText(statusMsg.c_str(), 20) / 2,
                         220, 20, DIM);

                // Show typed code (or confirmed code once fetched)
                const std::string& displayCode = roomCode.empty() ? codeInput : roomCode;
                drawCodeCells(displayCode, cx, 270);

                if (roomCode.empty() && !sigBusy) {
                    DrawText("Type the 6-letter code, then press Enter",
                             cx - MeasureText("Type the 6-letter code, then press Enter", 18) / 2,
                             370, 18, WARN);
                } else if (connecting) {
                    DrawText("Connecting...",
                             cx - MeasureText("Connecting...", 22) / 2,
                             370, 22, COL_OK);
                } else {
                    DrawText(statusMsg.empty() ? "Working..." : "",
                             cx, 370, 18, DIM);
                }

                DrawText("[Esc] Back", cx - MeasureText("[Esc] Back", 18) / 2, 620, 18, DIM);
            EndDrawing();
            continue;
        }

        // ── IN_GAME ─────────────────────────────────────────────────────────
        if (IsKeyPressed(KEY_R)) {
            if (game.GetNetMode() == NetMode::CLIENT)
                game.SendRestartRequest();
            else
                game.Init();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            game.SetNetMode(NetMode::STANDALONE);
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
