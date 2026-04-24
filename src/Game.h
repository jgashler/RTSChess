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
    Game();
    ~Game();   // defined in Game.cpp where NetHost/NetClient are complete types
    void Init();
    void Update(float dt);
    void Draw();

    // ── Networking ────────────────────────────────────────────────────────
    // Call SetNetMode before Init().  For CLIENT mode, `offerSdp` is the
    // host's offer string (pasted by the user).
    void SetNetMode(NetMode mode, const char* offerSdp = nullptr, uint16_t port = 7777);

    // WebRTC signaling helpers — called from the menu loop in main.cpp.
    // PollNet() must be called every frame while in the menu to process
    // libdatachannel callbacks on the main thread.
    void        PollNet();
    std::string GetOffer()  const;          // HOST:   offer SDP when ready
    void        SetAnswer(const std::string& sdp);  // HOST:   feed remote answer
    std::string GetAnswer() const;          // CLIENT: answer SDP when ready
    bool        IsNetConnected() const;     // true once P2P link is up

    // Called by NetHost callback when a move request arrives from the client
    void ExecuteMoveRequest(uint8_t pieceId, GridPos dest);

    // Called by NetClient callback when a state packet arrives from the host
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
    NetMode    netMode    = NetMode::STANDALONE;
    PieceColor localColor = PieceColor::Light;  // which side this instance plays
    std::unique_ptr<NetHost>   netHost;
    std::unique_ptr<NetClient> netClient;
    float       netBroadcastTimer = 0.f;
    std::string offerSdp;        // HOST: generated offer SDP (filled via callback)
    std::string answerSdp;       // CLIENT: generated answer SDP (filled via callback)
    bool        netConnected  = false;
    uint8_t     netResetGen   = 0;     // HOST: incremented on restart; CLIENT: tracks last seen
    bool        hasInitialized= false; // prevents counting the very first Init()

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

    // --- particles ---
    struct Particle {
        Vector3 pos, vel;
        float   life, maxLife;
    };
    std::vector<Particle> particles;
    void SpawnDeathParticles(Vector3 pos);
    void UpdateParticles(float dt);
    void DrawParticles();

    // --- check detection ---
    bool IsInCheck(PieceColor color) const;

    // --- draw ---
    void GenerateBgNoise();
    void DrawManaChannel(float edgeZ, float mana, bool flip);
    void DrawBoard();
    void DrawPieces();
    void DrawPiece(const Piece& p);
    void DrawUI();

    static Color SquareColor(int x, int y);
    static Color PieceColors(const Piece& p, Color& outRim);
};
