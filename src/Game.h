#pragma once
#include "raylib.h"
#include "Board.h"
#include "Player.h"
#include "Packets.h"
#include <vector>
#include <memory>
#include <string>

enum class GameState { PLAYING, WHITE_WINS, BLACK_WINS };
enum class NetMode   { STANDALONE, HOST, CLIENT };

class NetHost;
class NetClient;

class Game {
public:
    void Init();
    void Update(float dt);
    void Draw();

    // Networking — called from main before Init()
    void SetNetMode(NetMode mode, const char* address = nullptr, uint16_t port = 7777);

    // Called by NetHost when a move request arrives from the client
    void ExecuteMoveRequest(uint8_t pieceId, GridPos dest);

    // Called by NetClient when a state packet arrives from the server
    void ApplyNetState(const GameStatePacket& pkt);

private:
    // --- state ---
    Board     board;
    Player    white{ PieceColor::Light };
    Player    black{ PieceColor::Dark };
    GameState state = GameState::PLAYING;

    Piece*               selectedPiece = nullptr;
    std::vector<GridPos> validMoves;

    // --- networking ---
    NetMode   netMode    = NetMode::STANDALONE;
    PieceColor localColor = PieceColor::Light;  // which side this instance plays
    std::unique_ptr<NetHost>   netHost;
    std::unique_ptr<NetClient> netClient;
    float netBroadcastTimer = 0.f;

    GameStatePacket BuildStatePacket() const;

    Camera3D  camera      = {};
    Texture2D bgTexture   = {};
    float     camWobbleT  = 0.0f;   // accumulated time for camera wobble

    // render-target dimensions (pixel look)
    static constexpr int RW = 480;
    static constexpr int RH = 270;

    // --- lighting / shader ---
    Shader litShader  = {};
    int    locLightDir   = -1;
    int    locAmbient    = -1;
    int    locCameraPos  = -1;

    // --- shared piece mesh primitives ---
    // Unit cylinder  r=0.5 h=1  (base at y=0, top at y=1)
    // Unit sphere    r=0.5      (centred at origin)
    // Unit cube      1×1×1      (centred at origin)
    Model mdlCyl   = {};
    Model mdlSphere = {};
    Model mdlCube  = {};

    void InitShaderAndModels();
    void UnloadShaderAndModels();

    // Draw a cylinder scaled to (radius, height) with bottom at `pos`
    void DrawCyl(Vector3 pos, float r, float h, Color col) const;
    // Draw a sphere of `r` centred at `pos`
    void DrawSph(Vector3 pos, float r, Color col) const;
    // Draw a cube (w×h×d) centred at `pos`
    void DrawBox(Vector3 pos, float w, float h, float d, Color col) const;

    // --- update ---
    void UpdateCamera(float dt);
    void HandleInput();
    void UpdatePieces(float dt);
    void CheckCollisions();
    void CheckWinCondition();

    // --- helpers ---
    GridPos GetMouseGrid() const;
    bool    IsValidDest(GridPos g) const;
    Player& PlayerFor(PieceColor c) { return c == PieceColor::Light ? white : black; }

    // --- draw ---
    void GenerateBgNoise();
    void DrawManaChannel(float edgeZ, float mana);
    void DrawBoard();
    void DrawPieces();
    void DrawPiece(const Piece& p);
    void DrawUI();

    static Color SquareColor(int x, int y);
    static Color PieceColors(const Piece& p, Color& outRim);
};
