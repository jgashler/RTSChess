#include "Game.h"
#include "NetHost.h"
#include "NetClient.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

// ─────────────────────────────────────────────
//  Embedded PS1-style Gouraud shaders
//  Lighting is computed per-vertex, giving the classic
//  flat-faceted look on low-poly meshes.
// ─────────────────────────────────────────────
static const char* VS_LIT = R"glsl(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform vec3 lightDir;       // normalised, points TOWARD the light
uniform vec4 ambientColor;
uniform vec3 cameraPos;
uniform vec4 colDiffuse;

out vec4  fragColor;

void main()
{
    vec4 worldPos = matModel * vec4(vertexPosition, 1.0);
    vec3 N = normalize(mat3(matModel) * vertexNormal);

    // Diffuse
    float NdL = max(dot(N, lightDir), 0.0);
    // PS1-style step quantise (4 bands)
    NdL = floor(NdL * 4.0) / 4.0;

    // Specular (Blinn-Phong, low shininess for plastic look)
    vec3 V   = normalize(cameraPos - worldPos.xyz);
    vec3 H   = normalize(lightDir + V);
    float sp = pow(max(dot(N, H), 0.0), 24.0) * 0.45;

    vec4 base   = colDiffuse * vertexColor;
    vec3 colour = base.rgb * (ambientColor.rgb + vec3(NdL)) + vec3(sp);

    fragColor = vec4(colour, base.a);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

static const char* FS_LIT = R"glsl(
#version 330
in vec4 fragColor;
out vec4 finalColor;
void main() { finalColor = fragColor; }
)glsl";

// ─────────────────────────────────────────────
//  Shader + model setup
// ─────────────────────────────────────────────
void Game::InitShaderAndModels() {
    if (litShader.id > 0) UnloadShaderAndModels();

    litShader = LoadShaderFromMemory(VS_LIT, FS_LIT);

    // Wire up the locations raylib needs to fill automatically
    litShader.locs[SHADER_LOC_MATRIX_MVP]   = GetShaderLocation(litShader, "mvp");
    litShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(litShader, "matModel");
    litShader.locs[SHADER_LOC_COLOR_DIFFUSE]= GetShaderLocation(litShader, "colDiffuse");

    locLightDir  = GetShaderLocation(litShader, "lightDir");
    locAmbient   = GetShaderLocation(litShader, "ambientColor");
    locCameraPos = GetShaderLocation(litShader, "cameraPos");

    // Low-poly primitives  — PS1 look comes from the low slice counts
    mdlCyl    = LoadModelFromMesh(GenMeshCylinder(0.5f, 1.0f, 7));
    mdlSphere = LoadModelFromMesh(GenMeshSphere(0.5f, 6, 8));
    mdlCube   = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    mdlCyl   .materials[0].shader = litShader;
    mdlSphere.materials[0].shader = litShader;
    mdlCube  .materials[0].shader = litShader;
}

void Game::UnloadShaderAndModels() {
    if (litShader.id > 0) UnloadShader(litShader);
    if (mdlCyl.meshCount)    UnloadModel(mdlCyl);
    if (mdlSphere.meshCount) UnloadModel(mdlSphere);
    if (mdlCube.meshCount)   UnloadModel(mdlCube);
    litShader = {};
}

// ─────────────────────────────────────────────
//  Primitive draw helpers
// ─────────────────────────────────────────────
void Game::DrawCyl(Vector3 pos, float r, float h, Color col) const {
    // unit cylinder: r=0.5, h=1, base at y=0
    mdlCyl.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = col;
    DrawModelEx(mdlCyl, pos, {0,1,0}, 0, {r*2.f, h, r*2.f}, WHITE);
}
void Game::DrawSph(Vector3 pos, float r, Color col) const {
    // unit sphere: r=0.5, centred at origin
    mdlSphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = col;
    DrawModelEx(mdlSphere, pos, {0,1,0}, 0, {r*2.f, r*2.f, r*2.f}, WHITE);
}
void Game::DrawBox(Vector3 pos, float w, float h, float d, Color col) const {
    // unit cube 1×1×1 centred at origin  →  pos is the centre
    mdlCube.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = col;
    DrawModelEx(mdlCube, pos, {0,1,0}, 0, {w, h, d}, WHITE);
}

// ─────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────
void Game::Init() {
    // Signal the client that a restart happened (skip the very first Init so
    // the client's initial state matches without triggering a spurious restart).
    if (hasInitialized && netMode == NetMode::HOST) netResetGen++;
    hasInitialized = true;

    board.Init();
    white         = Player{ PieceColor::Light };
    black         = Player{ PieceColor::Dark };
    state         = GameState::PLAYING;
    selectedPiece = nullptr;
    validMoves.clear();
    particles.clear();

    // Host (Light) sits on the negative-Z side; Client (Dark) on the positive-Z side.
    bool isDark       = (localColor == PieceColor::Dark);
    camera.position   = isDark ? Vector3{ 4.0f, 10.0f, 13.0f }
                               : Vector3{ 4.0f, 10.0f, -5.0f };
    camera.target     = { 4.0f,  0.0f,  4.0f };
    camera.up         = { 0.0f,  1.0f,  0.0f };
    camera.fovy       = 42.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    if (bgTexture.id > 0) UnloadTexture(bgTexture);
    GenerateBgNoise();

    InitShaderAndModels();
}

// ─────────────────────────────────────────────
//  Background noise (light blue + white speckle)
// ─────────────────────────────────────────────
void Game::GenerateBgNoise() {
    Image img = GenImageColor(RW, RH, { 170, 200, 230, 255 });
    for (int y = 0; y < RH; y++) {
        for (int x = 0; x < RW; x++) {
            int roll = GetRandomValue(0, 99);
            Color c;
            if (roll < 3) {
                c = { 255, 255, 255, 255 };
            } else if (roll < 10) {
                c = { (unsigned char)GetRandomValue(200,230),
                      (unsigned char)GetRandomValue(220,245),
                      (unsigned char)GetRandomValue(240,255), 255 };
            } else {
                int v = GetRandomValue(-18, 18);
                c = { (unsigned char)Clamp(170+v,140,210),
                      (unsigned char)Clamp(200+v,170,235),
                      (unsigned char)Clamp(230+v,200,255), 255 };
            }
            ImageDrawPixel(&img, x, y, c);
        }
    }
    bgTexture = LoadTextureFromImage(img);
    UnloadImage(img);
}

// ─────────────────────────────────────────────
//  Networking
// ─────────────────────────────────────────────
Game::Game()  = default;
Game::~Game() = default;   // defined here where NetHost/NetClient are complete

void Game::SetNetMode(NetMode mode, const char* offerSdp_, uint16_t /*port*/) {
    netMode      = mode;
    netConnected = false;
    offerSdp.clear();
    answerSdp.clear();
    netHost.reset();
    netClient.reset();

    if (mode == NetMode::HOST) {
        localColor = PieceColor::Light;
        netHost    = std::make_unique<NetHost>();
        netHost->SetMoveCallback([this](uint8_t id, int x, int y) {
            ExecuteMoveRequest(id, {x, y});
        });
        netHost->SetOfferCallback([this](const std::string& sdp) {
            offerSdp = sdp;
        });
        netHost->SetConnectedCallback([this]() {
            netConnected = true;
        });
        netHost->Init();

    } else if (mode == NetMode::CLIENT) {
        localColor = PieceColor::Dark;
        netClient  = std::make_unique<NetClient>();
        netClient->SetStateCallback([this](const GameStatePacket& pkt) {
            ApplyNetState(pkt);
        });
        netClient->SetAnswerCallback([this](const std::string& sdp) {
            answerSdp = sdp;
        });
        netClient->SetConnectedCallback([this]() {
            netConnected = true;
        });
        if (offerSdp_) netClient->SetOffer(offerSdp_);
    }
}

void Game::PollNet() {
    if (netHost)   netHost->Poll();
    if (netClient) netClient->Poll();
}

std::string Game::GetOffer()  const { return offerSdp;    }
std::string Game::GetAnswer() const { return answerSdp;   }
bool Game::IsNetConnected()   const { return netConnected; }

void Game::SetAnswer(const std::string& sdp) {
    if (netHost) netHost->SetAnswer(sdp);
}

GameStatePacket Game::BuildStatePacket() const {
    GameStatePacket pkt{};
    pkt.type       = PacketType::GAME_STATE;
    pkt.gameState  = (uint8_t)state;
    pkt.resetGen   = netResetGen;
    pkt.whiteMana  = white.mana;
    pkt.blackMana  = black.mana;
    pkt.pieceCount = (uint8_t)board.pieces.size();

    for (int i = 0; i < (int)board.pieces.size() && i < 32; i++) {
        const Piece& p  = *board.pieces[i];
        PieceNetState& s = pkt.pieces[i];
        s.worldX   = p.worldPos.x;
        s.worldY   = p.worldPos.y;
        s.worldZ   = p.worldPos.z;
        s.gridX    = (int8_t)p.gridPos.x;
        s.gridY    = (int8_t)p.gridPos.y;
        s.targetX  = (int8_t)p.targetGrid.x;
        s.targetY  = (int8_t)p.targetGrid.y;
        s.type     = (uint8_t)p.type;
        s.color    = (uint8_t)p.color;
        s.id       = p.id;
        s.flags    = 0;
        if (p.isMoving)    s.flags |= NET_FLAG_MOVING;
        if (p.isJumping)   s.flags |= NET_FLAG_JUMPING;
        if (p.isDead)      s.flags |= NET_FLAG_DEAD;
        if (p.hasMoved)    s.flags |= NET_FLAG_HAS_MOVED;
        if (p.justLanded)  s.flags |= NET_FLAG_LANDED;
        if (p.justArrived) s.flags |= NET_FLAG_ARRIVED;
    }
    return pkt;
}

void Game::ApplyNetState(const GameStatePacket& pkt) {
    // If the host restarted, mirror that before applying anything else.
    if (pkt.resetGen != netResetGen) {
        netResetGen = pkt.resetGen;
        Init();   // resets board/state; networking config is untouched
        return;   // this packet's piece data is for the fresh game; next packet syncs it
    }

    state       = (GameState)pkt.gameState;  // propagates WHITE_WINS / BLACK_WINS
    white.mana  = pkt.whiteMana;
    black.mana  = pkt.blackMana;

    for (int i = 0; i < (int)pkt.pieceCount && i < 32; i++) {
        const PieceNetState& s = pkt.pieces[i];
        Piece* p = board.GetPieceById(s.id);
        if (!p) continue;
        p->worldPos    = { s.worldX, s.worldY, s.worldZ };
        p->gridPos     = { s.gridX,  s.gridY  };
        p->targetGrid  = { s.targetX, s.targetY };
        p->type        = (PieceType)s.type;   // keeps promotion (pawn→queen) in sync
        p->isMoving    = (s.flags & NET_FLAG_MOVING)    != 0;
        p->isJumping   = (s.flags & NET_FLAG_JUMPING)   != 0;
        bool wasAlive  = !p->isDead;
        p->isDead      = (s.flags & NET_FLAG_DEAD)       != 0;
        if (wasAlive && p->isDead) SpawnDeathParticles(p->worldPos); // client-side FX
        p->hasMoved    = (s.flags & NET_FLAG_HAS_MOVED) != 0;
        p->justLanded  = (s.flags & NET_FLAG_LANDED)     != 0;
        p->justArrived = (s.flags & NET_FLAG_ARRIVED)    != 0;
    }
}

void Game::ExecuteMoveRequest(uint8_t pieceId, GridPos dest) {
    Piece* p = board.GetPieceById(pieceId);
    if (!p || p->isDead || p->isMoving || p->isJumping) return;

    // Validate: the client can only move dark pieces
    if (p->color != PieceColor::Dark) return;

    // Validate: dest must be in valid moves
    auto moves = board.GetValidMoves(p);
    bool valid = false;
    for (auto& m : moves) if (m == dest) { valid = true; break; }
    if (!valid) return;

    Player& pl = PlayerFor(p->color);
    int cost   = p->ManaCost();
    if (!pl.CanAfford(cost)) return;

    pl.Spend(cost);
    if (p->type == PieceType::KNIGHT) {
        p->StartJump(dest);
    } else {
        p->hasMoved = true;
        p->StartMove(dest);
    }
}

// ─────────────────────────────────────────────
//  Camera wobble
// ─────────────────────────────────────────────
void Game::UpdateCamera(float dt) {
    camWobbleT += dt;
    float t = camWobbleT;

    // Two overlapping sine waves per axis at irrational frequency ratios
    // → smooth pseudo-random drift that never exactly repeats
    float dAz = (sinf(t * 0.82f) * 0.6f + sinf(t * 1.34f) * 0.4f) * 2.0f * DEG2RAD;
    float dEl = (sinf(t * 0.70f + 1.2f) * 0.6f + sinf(t * 1.06f + 0.7f) * 0.4f) * 4.0f * DEG2RAD;

    // Spherical coordinates around board centre (4, 0, 4)
    // Base distance and elevation match the Init() position:
    //   position = (4, 9.5, -3.5)  →  offset = (0, 9.5, -7.5)
    //   r ≈ sqrt(9.5²+7.5²) ≈ 12.1,  elevation ≈ asin(9.5/12.1) ≈ 0.902 rad
    //   azimuth  = atan2(-7.5, 0) = -π/2
    const float r      = 13.45f;
    const float baseEl = 0.834f;
    // Dark player sits on the positive-Z side, so rotate 180° around Y.
    const float baseAz = (localColor == PieceColor::Dark) ? PI / 2.0f : -PI / 2.0f;

    float theta = baseEl + dEl;   // elevation angle from horizontal
    float phi   = baseAz + dAz;   // azimuth angle around Y axis

    camera.position = {
        4.0f + r * cosf(theta) * cosf(phi),
               r * sinf(theta),
        4.0f + r * cosf(theta) * sinf(phi)
    };
}

void Game::Update(float dt) {
    UpdateCamera(dt);

    if (netMode == NetMode::CLIENT) {
        // Client: receive server state, send our input — no local simulation
        if (netClient) netClient->Poll();
        if (state == GameState::PLAYING) HandleInput();
        UpdateParticles(dt);   // client runs its own particle simulation
        return;
    }

    // STANDALONE or HOST: run full simulation
    if (state != GameState::PLAYING) {
        UpdateParticles(dt);
        return;
    }
    white.Update(dt);
    black.Update(dt);
    HandleInput();
    UpdatePieces(dt);
    CheckCollisions();

    // Clear selectedPiece BEFORE PurgeDead so we never hold a dangling pointer.
    if (selectedPiece && selectedPiece->isDead) {
        selectedPiece = nullptr;
        validMoves.clear();
    }

    board.PurgeDead();
    // Run CheckWinCondition BEFORE broadcast so the win state ships in the same
    // packet that carries the kill.  Without this the host returns early next
    // frame (state != PLAYING) and the client never sees WHITE_WINS/BLACK_WINS.
    CheckWinCondition();

    // Broadcast: force an immediate send when pieces died this frame so the
    // client sees isDead=true and the (possibly updated) game state without delay.
    if (netMode == NetMode::HOST && netHost) {
        netHost->Poll();
        if (netHost->IsConnected()) {
            bool anyDead = (state != GameState::PLAYING); // win counts as "urgent"
            for (auto& p : board.pieces) if (p->isDead) { anyDead = true; break; }
            netBroadcastTimer += dt;
            if (netBroadcastTimer >= 0.05f || anyDead) {
                netBroadcastTimer = 0.f;
                netHost->BroadcastState(BuildStatePacket());
            }
        }
    }

    UpdateParticles(dt);
}

// ─────────────────────────────────────────────
//  Input
// ─────────────────────────────────────────────
GridPos Game::GetMouseGrid() const {
    Vector2 mouse  = GetMousePosition();
    Vector2 scaled = {
        mouse.x * (float)RW / (float)GetScreenWidth(),
        mouse.y * (float)RH / (float)GetScreenHeight()
    };

    float ndcX =  2.f * scaled.x / (float)RW - 1.f;
    float ndcY =  1.f - 2.f * scaled.y / (float)RH;

    float aspect  = (float)RW / (float)RH;
    float tanHalf = tanf(camera.fovy * DEG2RAD * 0.5f);

    Vector3 rayView = { ndcX * aspect * tanHalf, ndcY * tanHalf, -1.f };

    Vector3 fwd   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera.up));
    Vector3 up    = Vector3CrossProduct(right, fwd);

    Vector3 rayWorld = Vector3Normalize(
        Vector3Add(Vector3Add(Vector3Scale(right, rayView.x),
                              Vector3Scale(up,    rayView.y)), fwd));

    if (fabsf(rayWorld.y) < 0.0001f) return {-1,-1};
    // Intersect with the tile top surface (y=0.28), not the world floor (y=0).
    // At shallow camera angles the ~0.28 offset shifts the hit point by nearly
    // a full tile on the far side of the board, making far squares unclickable.
    constexpr float BOARD_SURF_Y = 0.28f;
    float t = (BOARD_SURF_Y - camera.position.y) / rayWorld.y;
    if (t < 0.f) return {-1,-1};

    float wx = camera.position.x + rayWorld.x * t;
    float wz = camera.position.z + rayWorld.z * t;

    int gx = (int)floorf(wx);
    int gy = (int)floorf(wz);
    if (gx < 0 || gx >= 8 || gy < 0 || gy >= 8) return {-1,-1};
    return {gx, gy};
}

bool Game::IsValidDest(GridPos g) const {
    for (auto& m : validMoves) if (m == g) return true;
    return false;
}

void Game::HandleInput() {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;

    // ── Build mouse ray (same projection as GetMouseGrid) ────────────────────
    Vector2 mouse  = GetMousePosition();
    Vector2 scaled = { mouse.x * (float)RW / (float)GetScreenWidth(),
                       mouse.y * (float)RH / (float)GetScreenHeight() };
    float ndcX    =  2.f * scaled.x / (float)RW - 1.f;
    float ndcY    =  1.f - 2.f * scaled.y / (float)RH;
    float aspect  = (float)RW / (float)RH;
    float tanHalf = tanf(camera.fovy * DEG2RAD * 0.5f);
    Vector3 rayView = { ndcX * aspect * tanHalf, ndcY * tanHalf, -1.f };
    Vector3 fwd   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera.up));
    Vector3 up3   = Vector3CrossProduct(right, fwd);
    Vector3 rayDir = Vector3Normalize(
        Vector3Add(Vector3Add(Vector3Scale(right, rayView.x),
                              Vector3Scale(up3,   rayView.y)), fwd));

    // ── Ray-sphere pick: find closest selectable piece body under the cursor ──
    // We test against a sphere centred at the piece's mid-body world position.
    // This lets the player click anywhere on the visible model, not just the
    // small square of board beneath it.
    auto canSelectPiece = [&](const Piece* p) {
        return p && !p->isDead && !p->isMoving && !p->isJumping &&
               (netMode == NetMode::STANDALONE || p->color == localColor);
    };

    Piece* rayPick  = nullptr;
    float  rayPickT = 1e30f;
    for (auto& p : board.pieces) {
        if (!canSelectPiece(p.get())) continue;
        // Sphere centred at mid-body (~0.55 above board surface, so 0.28+0.55=0.83 world-Y)
        Vector3 centre = { p->worldPos.x, p->worldPos.y + 0.55f, p->worldPos.z };
        const float R  = 0.42f;   // generous click radius
        Vector3 oc = Vector3Subtract(camera.position, centre);
        float   b  = Vector3DotProduct(oc, rayDir);
        float   c  = Vector3DotProduct(oc, oc) - R * R;
        float disc = b * b - c;
        if (disc < 0.f) continue;
        float t = -b - sqrtf(disc);
        if (t > 0.f && t < rayPickT) { rayPickT = t; rayPick = p.get(); }
    }

    // ── Grid square under cursor (for move destination and fallback select) ──
    GridPos clicked = GetMouseGrid();

    // ── Execute a pending move if the clicked square is a valid destination ──
    // Piece-click is intentionally NOT allowed to override this — you are
    // committing a move, not selecting a new piece.
    if (selectedPiece && clicked.x >= 0 && IsValidDest(clicked)) {
        if (netMode != NetMode::STANDALONE && selectedPiece->color != localColor) {
            selectedPiece = nullptr; validMoves.clear(); return;
        }
        Player& pl = PlayerFor(selectedPiece->color);
        int cost   = selectedPiece->ManaCost();
        if (pl.CanAfford(cost)) {
            if (netMode == NetMode::CLIENT) {
                netClient->SendMoveRequest(selectedPiece->id, clicked.x, clicked.y);
            } else {
                pl.Spend(cost);
                if (selectedPiece->type == PieceType::KNIGHT)
                    selectedPiece->StartJump(clicked);
                else {
                    selectedPiece->hasMoved = true;
                    selectedPiece->StartMove(clicked);
                }
            }
        }
        selectedPiece = nullptr; validMoves.clear();
        return;
    }

    // ── Selection: ray-sphere pick has priority, grid square is the fallback ──
    Piece* toSelect = nullptr;
    if (rayPick) {
        toSelect = rayPick;
    } else if (clicked.x >= 0) {
        Piece* gp = board.GetPieceAt(clicked);
        if (canSelectPiece(gp)) toSelect = gp;
    }

    if (toSelect) {
        selectedPiece = toSelect;
        validMoves    = board.GetValidMoves(toSelect);
    } else {
        selectedPiece = nullptr;
        validMoves.clear();
    }
}

// ─────────────────────────────────────────────
//  Piece update
// ─────────────────────────────────────────────
void Game::UpdatePieces(float dt) {
    for (auto& p : board.pieces) {
        if (p->isDead) continue;
        p->Update(dt);

        // Pawn promotion — auto-queen on reaching the far rank
        if (p->type == PieceType::PAWN && !p->isMoving && !p->isJumping) {
            if ((p->color == PieceColor::Light && p->gridPos.y == 7) ||
                (p->color == PieceColor::Dark  && p->gridPos.y == 0)) {
                p->type    = PieceType::QUEEN;
                p->hasMoved = true;
            }
        }
    }
}

// ─────────────────────────────────────────────
//  Collision
// ─────────────────────────────────────────────
void Game::CheckCollisions() {
    auto& pcs = board.pieces;

    // ── Moving-piece hitbox sweep ──────────────────────────────────────────
    // A moving piece can kill any enemy piece it hits, AND any same-color piece
    // that is also currently moving (friendly fire on moving pieces only).
    // Static same-color pieces are always safe.
    for (int i = 0; i < (int)pcs.size(); i++) {
        Piece* a = pcs[i].get();
        if (a->isDead || a->isJumping || !a->isMoving) continue;
        Vector3 aHit = a->GetHitboxPos();

        for (int j = 0; j < (int)pcs.size(); j++) {
            if (i == j) continue;
            Piece* b = pcs[j].get();
            if (b->isDead || b->isJumping) continue;
            // Friendly static pieces are untouchable
            if (b->color == a->color && !b->isMoving) continue;

            float dx = aHit.x - b->worldPos.x;
            float dz = aHit.z - b->worldPos.z;
            if (sqrtf(dx*dx + dz*dz) >= Piece::HITBOX_RADIUS + Piece::HURTBOX_RADIUS) continue;

            bool headOn = false;
            if (b->isMoving) {
                Vector3 bHit = b->GetHitboxPos();
                float ex = bHit.x - a->worldPos.x;
                float ez = bHit.z - a->worldPos.z;
                headOn = sqrtf(ex*ex + ez*ez) < Piece::HITBOX_RADIUS + Piece::HURTBOX_RADIUS;
            }
            if (headOn) {
                if (!a->isDead) { SpawnDeathParticles(a->worldPos); a->isDead = true; }
                if (!b->isDead) { SpawnDeathParticles(b->worldPos); b->isDead = true; }
                break;  // a is dead — no need to check more targets
            } else {
                if (!b->isDead) { SpawnDeathParticles(b->worldPos); b->isDead = true; }
            }
        }
    }

    // ── Knight landing sweep ───────────────────────────────────────────────
    // Kills enemies (or moving friendlies) that are within the landing splash
    // radius.  Does NOT preemptively kill pieces still heading toward the square;
    // those pieces will handle it themselves when they arrive (see below).
    for (auto& p : pcs) {
        if (p->isDead || !p->justLanded) continue;

        for (auto& other : pcs) {
            if (other.get() == p.get() || other->isDead) continue;
            // Static friendly pieces are safe from the splash
            if (other->color == p->color && !other->isMoving && !other->isJumping) continue;

            float dx = other->worldPos.x - p->worldPos.x;
            float dz = other->worldPos.z - p->worldPos.z;
            if (sqrtf(dx*dx + dz*dz) < Piece::HURTBOX_RADIUS * 1.6f) {
                if (!other->isDead) { SpawnDeathParticles(other->worldPos); other->isDead = true; }
            }
        }
    }

    // ── Arrival sweep (second-to-arrive wins) ─────────────────────────────
    // When any piece finishes moving (justArrived) or jumping (justLanded),
    // it kills any piece already sitting statically at that exact grid square,
    // regardless of color.  This implements the "you were here first, you lose"
    // rule: the first piece to a contested square dies to whoever shows up next.
    for (auto& p : pcs) {
        if (p->isDead || (!p->justArrived && !p->justLanded)) continue;
        for (auto& other : pcs) {
            if (other.get() == p.get() || other->isDead) continue;
            if (other->isMoving || other->isJumping) continue; // only static pieces
            if (other->gridPos != p->gridPos) continue;        // must be on the same square
            if (!other->isDead) { SpawnDeathParticles(other->worldPos); other->isDead = true; }
        }
    }
}

// ─────────────────────────────────────────────
//  Win condition
// ─────────────────────────────────────────────
void Game::CheckWinCondition() {
    bool wk = false, bk = false;
    for (auto& p : board.pieces) {
        if (p->isDead) continue;
        if (p->type == PieceType::KING)
            (p->color == PieceColor::Light ? wk : bk) = true;
    }
    if (!wk) state = GameState::BLACK_WINS;
    if (!bk) state = GameState::WHITE_WINS;
}

// ─────────────────────────────────────────────
//  Colour helpers
// ─────────────────────────────────────────────
Color Game::SquareColor(int x, int y) {
    return ((x+y)%2==0) ? Color{215, 155, 80, 255}   // light square (warm tan)
                        : Color{ 85,  42, 14, 255};   // dark square (deep walnut)
}

// Returns base colour; sets outRim to accent colour
Color Game::PieceColors(const Piece& p, Color& outRim) {
    if (p.color == PieceColor::Light) {
        outRim = { 200, 185, 155, 255 };  // warm ivory shadow
        return   { 240, 230, 210, 255 };  // bright ivory
    } else {
        outRim = { 25,  18,  10,  255 };  // near-black shadow
        return   { 55,  42,  28,  255 };  // dark walnut
    }
}

// ─────────────────────────────────────────────
//  DrawManaChannel
//
//  Uses raylib's BUILT-IN DrawCube / DrawSphere (not our
//  lit-shader models) so colours are UNLIT and always vivid.
//
//  Camera right-vector is in -X world space, so:
//    screen-LEFT  = world high-X  (VIS_LEFT  = 7.6)
//    screen-RIGHT = world low-X   (VIS_RIGHT = 0.4)
//  Bar anchors at VIS_LEFT and grows toward VIS_RIGHT → L→R visually.
// ─────────────────────────────────────────────
// flip=false (light player / standalone): fill anchors at VIS_LEFT, grows →
// flip=true  (dark  player):             fill anchors at VIS_RIGHT, grows ←
//   so the bar always reads "left=empty, right=full" from the local player's view.
void Game::DrawManaChannel(float edgeZ, float mana, bool flip) {
    const int   UNITS     = (int)Player::MAX_MANA;
    const float VIS_LEFT  = 7.60f;
    const float VIS_RIGHT = 0.40f;
    const float CHAN_W    = VIS_LEFT - VIS_RIGHT;

    const float FILL_H = 0.32f;
    const float TOP_Y  = 0.25f - FILL_H * 0.20f;
    const float cylR   = FILL_H * 0.5f;
    const float glassR = cylR * 1.18f;

    float fillW  = CHAN_W * (mana / (float)Player::MAX_MANA);
    float emptyW = CHAN_W - fillW;

    // In normal mode  fill  = [VIS_LEFT-fillW, VIS_LEFT],  empty = [VIS_RIGHT, VIS_LEFT-fillW]
    // In flipped mode fill  = [VIS_RIGHT, VIS_RIGHT+fillW], empty = [VIS_RIGHT+fillW, VIS_LEFT]
    float fillX  = flip ? VIS_RIGHT               : VIS_LEFT - fillW;
    float emptyX = flip ? VIS_RIGHT + fillW        : VIS_RIGHT;
    float capX   = flip ? VIS_RIGHT + fillW        : VIS_LEFT - fillW; // meniscus position

    // ── 1. Empty inner tube ─────────────────────────────────────────────
    if (emptyW >= 0.02f) {
        mdlCyl.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {18, 4, 32, 255};
        DrawModelEx(mdlCyl, {emptyX, TOP_Y, edgeZ}, {0, 0, 1}, -90.0f,
                    {cylR * 2.f, emptyW, cylR * 2.f}, WHITE);
    }

    // ── 2. Magenta fill ─────────────────────────────────────────────────
    if (fillW >= 0.02f) {
        mdlCyl.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {255, 20, 210, 255};
        DrawModelEx(mdlCyl, {fillX, TOP_Y, edgeZ}, {0, 0, 1}, -90.0f,
                    {cylR * 2.f, fillW, cylR * 2.f}, WHITE);
        // Meniscus cap at the moving boundary between fill and empty
        DrawSph({capX, TOP_Y, edgeZ}, cylR * 0.88f, {255, 20, 210, 255});
    }

    // ── 3. Dividers ─────────────────────────────────────────────────────
    for (int i = 1; i < UNITS; i++) {
        float t  = (float)i / (float)UNITS;
        float dx = VIS_LEFT - t * CHAN_W;
        DrawCube({dx, TOP_Y, edgeZ}, 0.04f, FILL_H * 1.05f, FILL_H * 1.05f,
                 {8, 4, 20, 255});
    }

    // ── 4. Glass tube — unlit, no depth write ───────────────────────────
    // DrawCylinderEx is raylib's unlit primitive: no per-vertex lighting so
    // the camera wobble won't cause specular jitter/flickering on the glass.
    // rlDisableDepthMask lets fill/empty underneath show through.
    // Glass end caps are always drawn here, outside any mana conditional.
    Color glassCol = {215, 238, 255, 65};
    rlDisableDepthMask();
    DrawCylinderEx({VIS_RIGHT, TOP_Y, edgeZ}, {VIS_LEFT, TOP_Y, edgeZ},
                   glassR, glassR, 16, glassCol);
    // Caps at 88% of tube radius — compensates for sphere vs cylinder polygon rounding
    DrawSphere({VIS_LEFT,  TOP_Y, edgeZ}, glassR * 0.88f, glassCol);
    DrawSphere({VIS_RIGHT, TOP_Y, edgeZ}, glassR * 0.88f, glassCol);
    rlEnableDepthMask();
}

// ─────────────────────────────────────────────
//  DrawBoard
// ─────────────────────────────────────────────
void Game::DrawBoard() {
    // ── Thick wooden border ───────────────────
    // Tall enough to show the mana channels on the top of the front/back strips.
    // Border top at y = 0.25, giving the front strip a nice wide surface.
    Color wood     = {95, 48, 18, 255};
    Color woodDark = {60, 28, 8,  255};
    DrawBox({4.f, 0.0f, 4.f}, 8.80f, 0.50f, 8.80f, wood);     // main frame
    DrawBox({4.f, 0.0f, 4.f}, 8.20f, 0.54f, 8.20f, woodDark); // inset darker fill

    // ── Tiles ────────────────────────────────
    // Pre-compute which kings are in check so we can highlight their squares.
    GridPos lightKingPos = {-1,-1}, darkKingPos = {-1,-1};
    for (auto& p : board.pieces) {
        if (p->isDead || p->type != PieceType::KING) continue;
        if (p->color == PieceColor::Light) lightKingPos = p->gridPos;
        else                                darkKingPos  = p->gridPos;
    }
    bool lightInCheck = (lightKingPos.x >= 0) && IsInCheck(PieceColor::Light);
    bool darkInCheck  = (darkKingPos.x  >= 0) && IsInCheck(PieceColor::Dark);

    // Raised 0.03 above the border top so the rim is visible around the grid.
    // Priority (lowest → highest): base → check-red → valid-move-green → selected-yellow.
    // Check red is deliberately below valid-move green so that a square that is
    // both the king-in-check square AND a valid destination still shows green
    // (i.e. you can clearly see you can move there).
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            GridPos gp  = {x, y};
            Color   col = SquareColor(x, y);
            if ((lightInCheck && gp == lightKingPos) ||
                (darkInCheck  && gp == darkKingPos))
                col = {220, 45, 45, 255};          // check warning
            if (IsValidDest(gp)) {
                // Green = can afford to move now; yellow = valid but waiting for mana
                bool canAfford = false;
                if (selectedPiece) {
                    const Player& pl = (selectedPiece->color == PieceColor::Light)
                                       ? white : black;
                    canAfford = pl.mana >= (float)selectedPiece->ManaCost();
                }
                col = canAfford ? Color{80, 200, 95, 255}   // green  — ready
                                : Color{210, 170, 40, 255};  // yellow — waiting
            }
            if (selectedPiece && selectedPiece->gridPos == gp)
                col = {220, 210, 65, 255};          // selected piece
            DrawBox({x + 0.5f, 0.22f, y + 0.5f}, 0.97f, 0.12f, 0.97f, col);
        }
    }

    // ── Mana channel — only the local player's own bar ───────────────────
    // Standalone: show both (same-screen play).
    // Multiplayer: show only the local side, and flip the dark bar so it
    //              reads left-empty → right-full from the dark player's view.
    if (netMode == NetMode::STANDALONE) {
        DrawManaChannel(-0.20f, white.mana, false);
        DrawManaChannel( 8.20f, black.mana, false);
    } else if (localColor == PieceColor::Light) {
        DrawManaChannel(-0.20f, white.mana, false);
    } else {
        DrawManaChannel( 8.20f, black.mana, true);   // flipped for dark player
    }
}

// ─────────────────────────────────────────────
//  DrawPiece  — low-poly PS1-style geometry
// ─────────────────────────────────────────────
void Game::DrawPiece(const Piece& p) {
    Color rim;
    Color base = PieceColors(p, rim);

    Vector3 wp  = p.worldPos;
    // Tile top = centre(0.22) + half-height(0.06) = 0.28.
    // worldPos.y == 0 at rest, so add 0.28 to sit flush on the surface.
    float   by  = wp.y + 0.28f;

    // Selection ring
    if (&p == selectedPiece)
        DrawCylinder({wp.x, by, wp.z}, 0.48f, 0.48f, 0.02f, 12, {255,230,50,200});

    switch (p.type) {
        case PieceType::PAWN:
            DrawCyl({wp.x, by,        wp.z}, 0.28f, 0.07f, rim);   // disc base
            DrawCyl({wp.x, by+0.07f,  wp.z}, 0.13f, 0.24f, base);  // stem
            DrawCyl({wp.x, by+0.28f,  wp.z}, 0.19f, 0.04f, rim);   // collar
            DrawSph({wp.x, by+0.50f,  wp.z}, 0.19f,        base);  // head
            break;

        case PieceType::ROOK:
            DrawCyl({wp.x, by,        wp.z}, 0.30f, 0.07f, rim);   // disc base
            DrawCyl({wp.x, by+0.07f,  wp.z}, 0.22f, 0.38f, base);  // body
            DrawCyl({wp.x, by+0.45f,  wp.z}, 0.27f, 0.06f, rim);   // rim
            DrawCyl({wp.x, by+0.51f,  wp.z}, 0.25f, 0.13f, base);  // battlements body
            // Crenellations — 4 small blocks on top
            for (int i = 0; i < 4; i++) {
                float ang = i * (PI / 2.f);
                float bx  = wp.x + cosf(ang) * 0.15f;
                float bz  = wp.z + sinf(ang) * 0.15f;
                DrawBox({bx, by+0.64f+0.04f, bz}, 0.10f, 0.08f, 0.10f, rim);
            }
            break;

        case PieceType::KNIGHT:
            DrawCyl({wp.x, by,        wp.z}, 0.29f, 0.07f, rim);   // disc base
            DrawCyl({wp.x, by+0.07f,  wp.z}, 0.20f, 0.32f, base);  // body
            // Head block angled forward
            DrawBox({wp.x, by+0.52f,  wp.z+0.06f}, 0.30f, 0.26f, 0.36f, base); // skull
            DrawBox({wp.x, by+0.46f,  wp.z+0.15f}, 0.20f, 0.12f, 0.20f, rim);  // snout
            DrawBox({wp.x, by+0.66f,  wp.z-0.02f}, 0.28f, 0.06f, 0.10f, rim);  // brow
            break;

        case PieceType::BISHOP:
            DrawCyl({wp.x, by,        wp.z}, 0.28f, 0.07f, rim);   // disc base
            DrawCyl({wp.x, by+0.07f,  wp.z}, 0.18f, 0.16f, base);  // lower body
            DrawCyl({wp.x, by+0.23f,  wp.z}, 0.14f, 0.32f, base);  // upper body
            DrawCyl({wp.x, by+0.55f,  wp.z}, 0.20f, 0.05f, rim);   // collar
            DrawSph({wp.x, by+0.70f,  wp.z}, 0.13f,        base);  // ball
            DrawBox({wp.x, by+0.83f,  wp.z}, 0.06f, 0.14f, 0.06f, rim); // spike
            break;

        case PieceType::QUEEN:
            DrawCyl({wp.x, by,        wp.z}, 0.30f, 0.07f, rim);   // disc base
            DrawCyl({wp.x, by+0.07f,  wp.z}, 0.21f, 0.48f, base);  // body
            DrawCyl({wp.x, by+0.55f,  wp.z}, 0.27f, 0.06f, rim);   // crown base
            // Crown points — 5 spheres in a ring + 1 on top
            for (int i = 0; i < 5; i++) {
                float ang = i * (2.f*PI/5.f);
                float cx  = wp.x + cosf(ang)*0.18f;
                float cz  = wp.z + sinf(ang)*0.18f;
                DrawSph({cx, by+0.72f, cz}, 0.07f, rim);
            }
            DrawSph({wp.x, by+0.82f, wp.z}, 0.11f, base); // top orb
            break;

        case PieceType::KING:
            DrawCyl({wp.x, by,        wp.z}, 0.30f, 0.07f, rim);   // disc base
            DrawCyl({wp.x, by+0.07f,  wp.z}, 0.22f, 0.48f, base);  // body
            DrawCyl({wp.x, by+0.55f,  wp.z}, 0.27f, 0.06f, rim);   // crown base
            DrawCyl({wp.x, by+0.61f,  wp.z}, 0.24f, 0.10f, base);  // crown band
            // Cross
            DrawBox({wp.x, by+0.79f,  wp.z}, 0.09f, 0.30f, 0.09f, base); // vertical
            DrawBox({wp.x, by+0.88f,  wp.z}, 0.24f, 0.09f, 0.09f, rim);  // horizontal
            break;
    }

    // Red hitbox dot for moving pieces
    if (p.isMoving) {
        Vector3 h = p.GetHitboxPos();
        DrawSphere({h.x, h.y + 0.36f, h.z}, 0.07f, {255,60,60,220});
    }
}

// ─────────────────────────────────────────────
//  DrawPieces
// ─────────────────────────────────────────────
void Game::DrawPieces() {
    // Non-jumping first, then jumping (renders above everything)
    for (auto& p : board.pieces) if (!p->isDead && !p->isJumping) DrawPiece(*p);
    for (auto& p : board.pieces) if (!p->isDead &&  p->isJumping) DrawPiece(*p);
}


// ─────────────────────────────────────────────
//  DrawUI
// ─────────────────────────────────────────────
void Game::DrawUI() {
    const int sw = RW;
    const int sh = RH;

    // ── Selected piece tooltip ─────────────────
    if (selectedPiece) {
        const Player& pl = (selectedPiece->color == PieceColor::Light) ? white : black;
        char buf[80];
        snprintf(buf, sizeof(buf), "%s  |  cost: %d  |  mana: %.1f",
            selectedPiece->TypeName(), selectedPiece->ManaCost(), pl.mana);
        int tw = MeasureText(buf, 7);
        DrawText(buf, sw/2 - tw/2, sh - 10, 7, {255, 230, 80, 220});
    }

    // ── Win banner ─────────────────────────────
    if (state != GameState::PLAYING) {
        const char* msg = (state == GameState::WHITE_WINS) ? "WHITE WINS!" : "BLACK WINS!";
        Color col = (state == GameState::WHITE_WINS)
                      ? Color{255, 215, 0, 255} : Color{200, 100, 255, 255};
        int tw = MeasureText(msg, 20);
        DrawRectangle(0, sh/2 - 20, sw, 38, {0, 0, 0, 180});
        DrawText(msg, sw/2 - tw/2, sh/2 - 12, 20, col);
        int rw = MeasureText("Press R to restart", 7);
        DrawText("Press R to restart", sw/2 - rw/2, sh/2 + 14, 7, {200, 200, 200, 255});
    }
}

// ─────────────────────────────────────────────
//  Check detection
// ─────────────────────────────────────────────
bool Game::IsInCheck(PieceColor color) const {
    // Find the king of the given color
    Piece* king = nullptr;
    for (auto& p : board.pieces) {
        if (!p->isDead && p->type == PieceType::KING && p->color == color) {
            king = p.get();
            break;
        }
    }
    if (!king) return false;

    // Check if any opponent piece has the king's square in its valid moves
    PieceColor opp = (color == PieceColor::Light) ? PieceColor::Dark : PieceColor::Light;
    for (auto& p : board.pieces) {
        if (p->isDead || p->color != opp) continue;
        for (auto& m : board.GetValidMoves(p.get()))
            if (m == king->gridPos) return true;
    }
    return false;
}

// ─────────────────────────────────────────────
//  Particle system
// ─────────────────────────────────────────────
void Game::SpawnDeathParticles(Vector3 pos) {
    pos.y += 0.28f;   // worldPos.y==0 is the simulation floor; add board-surface offset
    const int COUNT = 14;
    for (int i = 0; i < COUNT; i++) {
        Particle pt;
        pt.pos = pos;
        float angle = (float)i / (float)COUNT * 2.f * PI
                    + GetRandomValue(0, 100) * 0.01f * PI;
        float speed = 1.8f + GetRandomValue(0, 100) * 0.025f;
        pt.vel = {
            cosf(angle) * speed,
            1.2f + GetRandomValue(0, 100) * 0.025f,
            sinf(angle) * speed
        };
        pt.maxLife = 0.55f + GetRandomValue(0, 45) * 0.01f;
        pt.life    = pt.maxLife;
        particles.push_back(pt);
    }
}

void Game::UpdateParticles(float dt) {
    for (auto& pt : particles) {
        pt.pos.x += pt.vel.x * dt;
        pt.pos.y += pt.vel.y * dt;
        pt.pos.z += pt.vel.z * dt;
        pt.vel.y -= 7.f * dt;   // gravity
        pt.life  -= dt;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
                       [](const Particle& p){ return p.life <= 0.f; }),
        particles.end());
}

void Game::DrawParticles() {
    for (auto& pt : particles) {
        float t     = pt.life / pt.maxLife;          // 1 → 0
        float r     = 0.07f * t;
        auto  alpha = (unsigned char)(t * 230.f);
        // Hot orange-red that cools toward dark red as it fades
        Color col = {255,
                     (unsigned char)(40.f + 80.f * t),
                     20,
                     alpha};
        DrawSphere(pt.pos, r, col);
    }
}

// ─────────────────────────────────────────────
//  Draw  (called inside BeginTextureMode)
// ─────────────────────────────────────────────
void Game::Draw() {
    ClearBackground({0,0,0,255});
    DrawTexture(bgTexture, 0, 0, WHITE);

    BeginMode3D(camera);
        // Upload light uniforms once — shared by board AND pieces
        Vector3 lightDir = Vector3Normalize({-0.6f, 1.0f, -0.4f});
        Vector4 ambient  = { 0.30f, 0.28f, 0.26f, 1.0f };
        Vector3 camPos   = camera.position;
        SetShaderValue(litShader, locLightDir,  &lightDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(litShader, locAmbient,   &ambient,  SHADER_UNIFORM_VEC4);
        SetShaderValue(litShader, locCameraPos, &camPos,   SHADER_UNIFORM_VEC3);

        DrawBoard();
        DrawPieces();
        DrawParticles();
    EndMode3D();

    DrawUI();
}
