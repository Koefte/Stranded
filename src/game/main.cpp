#include <SDL.h>
#include <SDL_net.h>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

#include "Camera.hpp" 
#include "GameObject.hpp"
#include "IAnimatable.hpp"
#include "Vector2.hpp"
#include "Player.hpp"
#include "Rectangle.hpp"
#include "Boat.hpp"
#include "DebugObject.hpp"
#include "UIGameObject.hpp"
#include "Particle.hpp"
#include "../audio/SoundManager.hpp"

//GAME
static std::vector<GameObject*> gameObjects;
static Camera* camera;
static Player* player;
static Boat* boat;
static SDL_Renderer* g_renderer = nullptr;
static std::set<std::pair<int,int>> generatedChunks;
static const int CHUNK_SIZE_PX = 512; // world-space pixels per chunk
static bool navigationUIActive = false;
static SDL_Texture* navigationClockTexture = nullptr;
static SDL_Texture* navigationIndicatorTexture = nullptr;


// CHUNK GENERATION
    static bool envCacheInit = false;
    static SDL_Texture* envTexture = nullptr;
    static int envTileW = 0;
    static int envTileH = 0;

// Fishing minigame state (timed click to catch a fish OR tug-of-the-deep)
static bool fishingMinigameActive = false;
static float fishingMinigameTimer = 0.0f;
static float fishingMinigameDuration = 4.5f; // seconds until failure (eased)

// Timed-click state
static float fishingMinigameIndicator = 0.0f; // 0..1 moving indicator
static float fishingMinigameIndicatorDir = 1.0f; // direction +1/-1
static float fishingMinigameIndicatorSpeed = 0.8f; // cycles per second across 0..1 (slowed for ease)
// Default window roughly 20% centered; actual window recalculated on start
static float fishingMinigameWindowStart = 0.40f;
static float fishingMinigameWindowEnd = 0.60f;

// Tug-of-the-deep state
enum MinigameType { MINIGAME_TIMED_CLICK = 0, MINIGAME_TUG_OF_THE_DEEP = 1 };
static MinigameType fishingMinigameType = MINIGAME_TIMED_CLICK;
static float tugProgress = 0.5f; // 0..1, player wins when low
static float tugTension = 0.0f; // 0..1, >=1 => fail
static float tugFishForce = 0.18f; // units per second pushing toward fish side
static float tugBurstRemaining = 0.0f; // seconds remaining for active fish burst
static float tugNextBurstTime = 0.0f;
static int tugStamina = 3; // quick pulls allowed
static float tugLastPullTime = 0.0f;
static float tugPlayerPullLevel = 0.0f; // instantaneous pull strength
static const float TUG_PULL_BASE = 0.12f; // base progress change per pull
static const float TUG_PULL_BONUS = 0.08f; // bonus for quick successive pulls
static const float TUG_MAX_FORCE = 0.6f; // used for tension normalization
static const float TUG_WIN_THRESHOLD = 0.20f; // progress <= this = success
static const float TUG_FAIL_THRESHOLD = 0.95f; // progress >= this = fail

static Vector2 fishingMinigameHookPos{0.0f,0.0f}; // world pos where minigame triggered
static SDL_Rect fishingMinigameScreenRect = {0,0,0,0}; // screen-space rect for minigame bar
static std::mt19937 fishingMinigameRng(std::random_device{}());
static int fishingMinigameAttempts = 0; // debug counter

// Fish collection / inventory state
static std::vector<GameObject*> fishesMovingToPlayer; // world fish moving towards player
// Inventory as a fixed 2D grid of UIFish icons (screen-space UIGameObjects)
static const int INV_COLS = 5;
static const int INV_ROWS = 3;
static const int INV_CELL_SIZE = 64;
static const int INV_PADDING = 12;
static std::vector<UIGameObject*> inventorySlots(INV_COLS * INV_ROWS, nullptr); // null = empty slot



static std::set<SDL_Keycode> pressedInteractKeys;
// Inventory UI state
static bool inventoryOpen = false;



// Networking
static bool isHost = false;
static UDPsocket udpSocket = nullptr;
static IPaddress hostAddr;
static std::vector<IPaddress> clientAddrs;
static uint32_t clientId = 0;
static uint32_t inputSeq = 0;
static std::unordered_map<uint32_t, Player*> remotePlayers;
static bool clientBoardingRequest = false;
static bool clientBoatMovementToggle = false;
static bool clientHookToggle = false;
// RNG for networked events
static std::mt19937 netRng(std::random_device{}());

#pragma pack(push, 1)
struct ChunkPacket {
    uint32_t magic; // 'CHNK'
    int32_t cx;
    int32_t cy;
    uint32_t seed;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ParticlePacket {
    uint32_t magic; // 'PART'
    uint32_t ownerId;
    uint32_t seed;
    float startX;
    float startY;
    float destX;
    float destY;
    float delay; // seconds until spawn (host-determined)
    uint8_t count;
    float duration;
    int32_t zIndex;
    float spread;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct HookArrivalPacket {
    uint32_t magic; // 'HKAR'
    uint32_t ownerId;
    float x;
    float y;
};
#pragma pack(pop)

// Header for packet containing explicit particle start positions
#pragma pack(push, 1)
struct ParticlePositionsHeader {
    uint32_t magic; // 'PPOS'
    uint32_t ownerId;
    float delay; // seconds until spawn
    uint8_t count; // number of particle start positions
    float duration; // particle lifetime
    int32_t zIndex;
    float destX;
    float destY;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct ParticlePos {
    float sx;
    float sy;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct InputPacket {
    uint32_t clientId;
    uint32_t seq;
    uint8_t moveFlags; // bits: 0=up, 1=down, 2=left, 3=right
    uint8_t boardBoat; // 0=no action, 1=toggle boarding
    uint8_t toggleBoatMovement; // 0=no action, 1=toggle start/stop
    uint8_t hasBoatControl; // 0=no, 1=has navigation direction update
    uint8_t toggleHook; // 0=no action, 1=toggle hook
    float boatNavDirX;
    float boatNavDirY;
    uint8_t mouseDown; // 0 = none, 1 = left click
    int32_t mouseX;
    int32_t mouseY;
    int32_t hookTargetX;
    int32_t hookTargetY;
    int32_t hookStartX;
    int32_t hookStartY;
};

struct BoatState {
    float x, y;
    float rotation;
    float navDirX, navDirY;
    uint8_t isMoving;
};

struct PlayerState {
    uint32_t id;
    float x, y;
    float vx, vy;  // velocity
    uint8_t animFrame;
    uint8_t isOnBoat; // 0 = not on boat, 1 = on boat
    uint8_t isHooking; // 0 = not hooking, 1 = hooking
    uint8_t fishingHookActive; // 0 = not active, 1 = active
    float fishingHookX, fishingHookY; // world position of hook
    float fishingHookTargetX, fishingHookTargetY; // mouse destination
};

struct SnapshotHeader {
    uint32_t tick;
    uint32_t playerCount;
    uint8_t hasBoat;
};
#pragma pack(pop)




constexpr int WIN_WIDTH = 800;
constexpr int WIN_HEIGHT = 600;

enum RENDER_LAYERS {
    LAYER_ENVIRONMENT = 0,
    LAYER_BOAT = 1,
    LAYER_LIGHTHOUSE = 2,
    LAYER_PLAYER = 3,
    LAYER_PARTICLE = 4,
    LAYER_UI = 5,
    LAYER_DEBUG = 6,
};

// Forward declare so getOrCreateRemotePlayer can reference it when installing callbacks
void hostBroadcastHookArrival(uint32_t ownerId, const Vector2& pos);

// Spawn a fish GameObject at the given world position
void onHook(const Vector2& pos);

bool initEnvironmentTiles(SDL_Renderer* renderer) {
        if(envCacheInit) return true;
        SDL_Surface* surface = SDL_LoadBMP("./sprites/water1.bmp");
        if (!surface) {
            SDL_Log("Failed to load environment tile: %s", SDL_GetError());
            return false;
        }
        envTileW = surface->w;
        envTileH = surface->h;
        envTexture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!envTexture) {
            SDL_Log("Failed to create texture for environment tile: %s", SDL_GetError());
            return false;
        }
        envCacheInit = true;
        return true;
}


std::vector<GameObject*> generateInitialEnvironment(SDL_Renderer* renderer, Rectangle area, uint32_t seed = 0) {
    std::vector<GameObject*> environment;
    if (!initEnvironmentTiles(renderer)) {
        return environment;
    }

    uint32_t prng = seed;
    // First pass: fill with environment tiles
    std::vector<Vector2> smallIslandPositions;
    for (int y = static_cast<int>(area.begin.y); y < static_cast<int>(area.end.y); y += envTileH) {
        for (int x = static_cast<int>(area.begin.x); x < static_cast<int>(area.end.x); x += envTileW) {
            bool makeSmallIsland = false;
            if (seed != 0) {
                prng = 1664525u * prng + 1013904223u; // LCG
                // Use a byte of PRNG and make small tile islands rarer (~0.78%)
                int tileRoll = (prng >> 8) & 0xFF;
                makeSmallIsland = (tileRoll < 2);
            } else {
                // Random non-seeded fallback: 1 in 128 (~0.78%)
                makeSmallIsland = (rand() % 128) == 0;
            }

            SDL_Texture* texture = envTexture;
            // Always add the water tile first so islands are drawn on top
            if (texture) {
                GameObject* tileObj = new GameObject(
                    {static_cast<float>(x), static_cast<float>(y)},
                    {1.0f, 1.0f},
                    texture,
                    renderer,
                    LAYER_ENVIRONMENT
                );
                environment.push_back(tileObj);
            }

            // Record island positions to place in a second pass so islands are drawn over tiles
            if (makeSmallIsland) {
                smallIslandPositions.push_back({static_cast<float>(x), static_cast<float>(y)});
            }
        }
    }

    // Decide on a deterministic or random large island BEFORE placing small islands so we can avoid overlap
    bool hasBigIsland = false;
    float bigPx = 0.0f, bigPy = 0.0f;
    int bigTilesWide = 0, bigTilesHigh = 0;

    // Tile grid dimensions for this chunk/area
    int areaTilesX = static_cast<int>((area.end.x - area.begin.x) / envTileW);
    int areaTilesY = static_cast<int>((area.end.y - area.begin.y) / envTileH);

    // Safety: if area has no tiles, return early
    if (areaTilesX <= 0 || areaTilesY <= 0) return environment;

    // Placement constraints - reduce density by increasing spacing and capping islands
    const int MIN_ISLAND_GAP_TILES = 2; // increased gap between island centers (in tiles)
    const int EDGE_BUFFER_TILES = 2; // larger buffer from chunk edges to avoid cross-chunk joins
    const int MAX_SMALL_ISLANDS_PER_CHUNK = 1; // cap small islands per chunk

    if (seed != 0) {
        // advance PRNG and use a byte to decide
        prng = 1664525u * prng + 1013904223u;
        int islandChance = (prng >> 16) & 0xFF; // 0..255
        // Make large islands rare (~1.6%)
        if (islandChance < 1) {
            prng = 1664525u * prng + 1013904223u;
            int tilesWide = 3 + ((prng >> 16) % 6); // 3..8
            prng = 1664525u * prng + 1013904223u;
            int tilesHigh = 3 + ((prng >> 16) % 6);

            // Respect edge buffer when choosing viable placement
            if (areaTilesX > tilesWide + 2 * EDGE_BUFFER_TILES && areaTilesY > tilesHigh + 2 * EDGE_BUFFER_TILES) {
                prng = 1664525u * prng + 1013904223u;
                int maxXOffset = areaTilesX - tilesWide - 2 * EDGE_BUFFER_TILES;
                int maxYOffset = areaTilesY - tilesHigh - 2 * EDGE_BUFFER_TILES;
                int offsetX = EDGE_BUFFER_TILES + ((prng >> 16) % (maxXOffset + 1));
                prng = 1664525u * prng + 1013904223u;
                int offsetY = EDGE_BUFFER_TILES + ((prng >> 16) % (maxYOffset + 1));

                bigPx = area.begin.x + offsetX * envTileW;
                bigPy = area.begin.y + offsetY * envTileH;
                bigTilesWide = tilesWide;
                bigTilesHigh = tilesHigh;
                hasBigIsland = true;
            }
        }
    } else {
        // Non-deterministic run-time chance for a large island when no seed is provided
        // ~1.6% chance (1 in 64)
        if ((rand() % 64) == 0) {
            int tilesWide = 3 + (rand() % 6);
            int tilesHigh = 3 + (rand() % 6);
            if (areaTilesX > tilesWide + 2 * EDGE_BUFFER_TILES && areaTilesY > tilesHigh + 2 * EDGE_BUFFER_TILES) {
                int offsetX = EDGE_BUFFER_TILES + (rand() % (areaTilesX - tilesWide - 2 * EDGE_BUFFER_TILES + 1));
                int offsetY = EDGE_BUFFER_TILES + (rand() % (areaTilesY - tilesHigh - 2 * EDGE_BUFFER_TILES + 1));
                bigPx = area.begin.x + offsetX * envTileW;
                bigPy = area.begin.y + offsetY * envTileH;
                bigTilesWide = tilesWide;
                bigTilesHigh = tilesHigh;
                hasBigIsland = true;
            }
        }
    }

    // Prepare occupancy grid for deterministic spacing and overlap prevention
    std::vector<uint8_t> occupancy(areaTilesX * areaTilesY, 0);

    // If we have a big island, mark its expanded footprint as occupied (including the MIN_ISLAND_GAP)
    if (hasBigIsland) {
        int bigTX = static_cast<int>((bigPx - area.begin.x) / envTileW);
        int bigTY = static_cast<int>((bigPy - area.begin.y) / envTileH);
        int minX = std::max(0, bigTX - MIN_ISLAND_GAP_TILES);
        int minY = std::max(0, bigTY - MIN_ISLAND_GAP_TILES);
        int maxX = std::min(areaTilesX - 1, bigTX + bigTilesWide - 1 + MIN_ISLAND_GAP_TILES);
        int maxY = std::min(areaTilesY - 1, bigTY + bigTilesHigh - 1 + MIN_ISLAND_GAP_TILES);
        for (int ty = minY; ty <= maxY; ++ty) {
            for (int tx = minX; tx <= maxX; ++tx) {
                occupancy[ty * areaTilesX + tx] = 1;
            }
        }
    }

    // Shuffle candidate small island positions deterministically when seed present; random otherwise
    std::vector<Vector2> candidates = smallIslandPositions;
    if (!candidates.empty()) {
        if (seed != 0) {
            std::mt19937 shuf(seed ^ 0x9E3779B9u);
            std::shuffle(candidates.begin(), candidates.end(), shuf);
        } else {
            std::mt19937 shuf(static_cast<uint32_t>(SDL_GetTicks()));
            std::shuffle(candidates.begin(), candidates.end(), shuf);
        }
    }

    int placedSmall = 0;
    for (const auto &pos : candidates) {
        int tx = static_cast<int>((pos.x - area.begin.x) / envTileW);
        int ty = static_cast<int>((pos.y - area.begin.y) / envTileH);
        if (tx < 0 || tx >= areaTilesX || ty < 0 || ty >= areaTilesY) continue;

        // Respect an edge buffer so islands do not sit on the very edge of a chunk
        if (tx < EDGE_BUFFER_TILES || tx >= areaTilesX - EDGE_BUFFER_TILES) continue;
        if (ty < EDGE_BUFFER_TILES || ty >= areaTilesY - EDGE_BUFFER_TILES) continue;

        // Check occupancy in a neighborhood of MIN_ISLAND_GAP_TILES
        bool blocked = false;
        for (int oy = -MIN_ISLAND_GAP_TILES; oy <= MIN_ISLAND_GAP_TILES && !blocked; ++oy) {
            for (int ox = -MIN_ISLAND_GAP_TILES; ox <= MIN_ISLAND_GAP_TILES; ++ox) {
                int nx = tx + ox;
                int ny = ty + oy;
                if (nx < 0 || nx >= areaTilesX || ny < 0 || ny >= areaTilesY) continue;
                if (occupancy[ny * areaTilesX + nx]) { blocked = true; break; }
            }
        }
        if (blocked) continue;

        // Place island and mark occupancy in its neighborhood to enforce spacing
        ICollidable* island = new ICollidable(
            {pos.x, pos.y},
            {1.0f, 1.0f},
            "./sprites/island.bmp",
            renderer,
            true,
            LAYER_ENVIRONMENT
        );
        environment.push_back(island);
        ++placedSmall;
        // If we've reached the per-chunk cap, stop placing more small islands
        if (placedSmall >= MAX_SMALL_ISLANDS_PER_CHUNK) {
            SDL_Log("Reached max small islands (%d) for this area (seed=%u); skipping remaining candidates.", MAX_SMALL_ISLANDS_PER_CHUNK, seed);
            // mark occupancy for this island before breaking to keep spacing consistent
            for (int oy = -MIN_ISLAND_GAP_TILES; oy <= MIN_ISLAND_GAP_TILES; ++oy) {
                for (int ox = -MIN_ISLAND_GAP_TILES; ox <= MIN_ISLAND_GAP_TILES; ++ox) {
                    int nx = tx + ox;
                    int ny = ty + oy;
                    if (nx < 0 || nx >= areaTilesX || ny < 0 || ny >= areaTilesY) continue;
                    occupancy[ny * areaTilesX + nx] = 1;
                }
            }
            break;
        }

        for (int oy = -MIN_ISLAND_GAP_TILES; oy <= MIN_ISLAND_GAP_TILES; ++oy) {
            for (int ox = -MIN_ISLAND_GAP_TILES; ox <= MIN_ISLAND_GAP_TILES; ++ox) {
                int nx = tx + ox;
                int ny = ty + oy;
                if (nx < 0 || nx >= areaTilesX || ny < 0 || ny >= areaTilesY) continue;
                occupancy[ny * areaTilesX + nx] = 1;
            }
        }
    }

    SDL_Log("Placed %d small islands (candidates %zu) in area [%.1f,%.1f]-[%.1f,%.1f] (seed=%u)", placedSmall, candidates.size(), area.begin.x, area.begin.y, area.end.x, area.end.y, seed);


    // Finally add the big island (if any) so it renders on top of tiles and small islands
    if (hasBigIsland) {
        ICollidable* bigIsland = new ICollidable(
            {bigPx, bigPy},
            {static_cast<float>(bigTilesWide), static_cast<float>(bigTilesHigh)},
            "./sprites/island.bmp",
            renderer,
            true, // detailed collider for a larger island
            LAYER_ENVIRONMENT
        );
        environment.push_back(bigIsland);
        SDL_Log("Chunk%s large island at (%.1f,%.1f) size (%dx%d) in area [%.1f,%.1f]-[%.1f,%.1f]",
                seed != 0 ? " seed" : " random", bigPx, bigPy, bigTilesWide, bigTilesHigh, area.begin.x, area.begin.y, area.end.x, area.end.y);
    }

    return environment;
}

Player* getOrCreateRemotePlayer(uint32_t id) {
    if (remotePlayers.find(id) != remotePlayers.end()) {
        return remotePlayers[id];
    }
    if (!g_renderer) {
        std::cerr << "Renderer not initialized for remote player creation\n";
        return nullptr;
    }
    const char* remoteSprites[] = {
        "./sprites/Boy_Walk1.bmp",
        "./sprites/Boy_Walk2.bmp",
        "./sprites/Boy_Walk3.bmp",
        "./sprites/Boy_Walk4.bmp"
    };
    Player* remote = new Player({0.0f, 0.0f}, {2.0f, 2.0f}, remoteSprites, 4, g_renderer, 0.1f, LAYER_PLAYER);
    remotePlayers[id] = remote;
    gameObjects.push_back(remote); // Add player
    if (remote->getFishingProjectile()) {
        gameObjects.push_back(remote->getFishingProjectile()); // Add their fishing hook for rendering
        // If running as host, set up hook arrival broadcast for this remote player
        if (isHost) {
            remote->getFishingProjectile()->setOnHookArrival([id](const Vector2& pos){
                hostBroadcastHookArrival(id, pos);
            });
        }
        // Spawn fish when attract arrival occurs for this remote player's hook
        remote->getFishingProjectile()->setOnAttractArrival([remote](){
            Vector2 p = remote->getFishingProjectile()->getWorldPosition();
            onHook(p);
        });
    }
    std::cout << "Created remote player with ID: " << id << "\n";
    return remote;
}

// Forward declarations
float hitBoxDistance(std::vector<Rectangle> shapeA, std::vector<Rectangle> shapeB);

// Host broadcast for authoritative hook arrivals
void hostBroadcastHookArrival(uint32_t ownerId, const Vector2& pos);

void sendInputPacket() {
    if (!udpSocket || isHost) return;
    
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    uint8_t moveFlags = 0;
    if (keys[SDL_SCANCODE_W]) moveFlags |= (1 << 0);
    if (keys[SDL_SCANCODE_S]) moveFlags |= (1 << 1);
    if (keys[SDL_SCANCODE_A]) moveFlags |= (1 << 2);
    if (keys[SDL_SCANCODE_D]) moveFlags |= (1 << 3);
    
    uint8_t boardBoat = clientBoardingRequest ? 1 : 0;
    clientBoardingRequest = false; // Reset after sending
    
    uint8_t toggleBoatMovement = clientBoatMovementToggle ? 1 : 0;
    clientBoatMovementToggle = false; // Reset after sending
    
    uint8_t toggleHook = clientHookToggle ? 1 : 0;
    clientHookToggle = false; // Reset after sending
    
    // Send boat navigation direction if we have control
    uint8_t hasBoatControl = navigationUIActive ? 1 : 0;
    Vector2 navDir = boat->getNavigationDirection();
    
    static uint8_t lastMouseDown = 0;
    static int32_t lastMouseX = 0, lastMouseY = 0;
    uint8_t mouseDown = 0;
    int32_t mouseX = 0, mouseY = 0;
    int buttons = SDL_GetMouseState(&mouseX, &mouseY);
    if ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0) {
        mouseDown = 1;
    }
    // Only send mouseDown if it changed (edge trigger)
    uint8_t sendMouseDown = 0;
    if (mouseDown && !lastMouseDown) {
        sendMouseDown = 1;
    }
    // If a local fishing minigame is active, suppress sending mouseDown to prevent server-side casts
    if (fishingMinigameActive) {
        if (sendMouseDown) {
            SDL_Log("Client: suppressing sendMouseDown due to active fishing minigame");
        }
        sendMouseDown = 0;
    }
    lastMouseDown = mouseDown;
    lastMouseX = mouseX;
    lastMouseY = mouseY;
    // Convert mouse screen coordinates to world coordinates using camera transform
    // (match Player::onMouseDown: world = mouse / zoom + cameraOffset)
    float zoom = camera ? camera->getZoom() : 1.0f;
    Vector2 camPos = camera ? camera->getPosition() : Vector2{0,0};
    float worldX = (static_cast<float>(mouseX) / zoom) + camPos.x;
    float worldY = (static_cast<float>(mouseY) / zoom) + camPos.y;
    // Debug: Log mouse and world coordinates
    //printf("Client: mouse (%d, %d) -> world (%.2f, %.2f)\n", mouseX, mouseY, worldX, worldY);
    // For the hook target, use the same as worldX/worldY (can be customized if needed)
    int32_t hookTargetX = static_cast<int32_t>(worldX);
    int32_t hookTargetY = static_cast<int32_t>(worldY);
    // Debug: Log the hook target being sent
    printf("Client: sending hook target (X: %d, Y: %d) [world: %.2f, %.2f]\n", hookTargetX, hookTargetY, worldX, worldY);
    
    // Calculate rod tip world position (match Player::onMouseDown and host computation)
    Rod* rod = player->getRod();
    Vector2 rodWorld = rod->getWorldPosition();
    Vector2* rodSize = rod->getSize();
    float hookStartX = rodWorld.x + rodSize->x / 2.0f;
    float hookStartY = rodWorld.y + rodSize->y;
    // Log the rod tip world position being sent
    InputPacket pkt{clientId, inputSeq++, moveFlags, boardBoat, toggleBoatMovement, hasBoatControl, toggleHook, navDir.x, navDir.y, sendMouseDown, static_cast<int32_t>(worldX), static_cast<int32_t>(worldY), hookTargetX, hookTargetY};
    pkt.hookStartX = static_cast<int32_t>(hookStartX);
    pkt.hookStartY = static_cast<int32_t>(hookStartY);
    UDPpacket* out = SDLNet_AllocPacket(sizeof(pkt));
    std::memcpy(out->data, &pkt, sizeof(pkt));
    out->len = sizeof(pkt);
    out->address = hostAddr;
    SDLNet_UDP_Send(udpSocket, -1, out);
    SDLNet_FreePacket(out);
}

void receiveInputs() {
    if (!udpSocket || !isHost) return;
    
    UDPpacket* in = SDLNet_AllocPacket(512);
    while (SDLNet_UDP_Recv(udpSocket, in)) {
        if (in->len >= sizeof(InputPacket)) {
            InputPacket pkt;
            std::memcpy(&pkt, in->data, sizeof(pkt));
            
            // Track new clients
            bool known = false;
            for (auto& addr : clientAddrs) {
                if (addr.host == in->address.host && addr.port == in->address.port) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                clientAddrs.push_back(in->address);
                std::cout << "New client connected: " << pkt.clientId << "\n";
            }
            
            // Apply input to remote player
            Player* remote = getOrCreateRemotePlayer(pkt.clientId);
            if (remote) {
                // Simulate input by directly calling key handlers
                if (pkt.moveFlags & (1 << 0)) remote->onKeyDown(SDLK_w); else remote->onKeyUp(SDLK_w);
                if (pkt.moveFlags & (1 << 1)) remote->onKeyDown(SDLK_s); else remote->onKeyUp(SDLK_s);
                if (pkt.moveFlags & (1 << 2)) remote->onKeyDown(SDLK_a); else remote->onKeyUp(SDLK_a);
                if (pkt.moveFlags & (1 << 3)) remote->onKeyDown(SDLK_d); else remote->onKeyUp(SDLK_d);
                
                // Handle boarding request
                if (pkt.boardBoat == 1) {
                    if (boat->isPlayerOnBoard(remote)) {
                        boat->leaveBoat(remote);
                    } else {
                        // Check if close enough to board
                        ICollidable* boatCollider = dynamic_cast<ICollidable*>(boat);
                        ICollidable* remoteCollider = dynamic_cast<ICollidable*>(remote);
                        if (boatCollider && remoteCollider) {
                            auto boatShape = boatCollider->getCollisionBox();
                            auto remoteShape = remoteCollider->getCollisionBox();
                            if (hitBoxDistance(boatShape, remoteShape) < 10.0f) {
                                boat->boardBoat(remote);
                            }
                        }
                    }
                }
                
                // Handle boat navigation direction update
                if (pkt.hasBoatControl == 1) {
                    float angle = atan2(pkt.boatNavDirY, pkt.boatNavDirX);
                    boat->setNavigationDirection(angle);
                }
                
                // Handle boat movement toggle (E key)
                if (pkt.toggleBoatMovement == 1) {
                    boat->onInteract(SDLK_e);
                }
                
                // Handle hook toggle
                if (pkt.toggleHook == 1) {
                    remote->onKeyDown(SDLK_r);
                }
                // Handle mouse click for fishing hook casting
                    if (pkt.mouseDown == 1 && remote->isRodVisible() && remote->getFishingProjectile()) {
                    // Use client-sent rod tip world position as the authoritative cast origin
                    Vector2 hookTip = { static_cast<float>(pkt.hookStartX), static_cast<float>(pkt.hookStartY) };
                    printf("Host: received rod tip for client %u (%.2f, %.2f)\n", pkt.clientId, hookTip.x, hookTip.y);
                    printf("Host: received hook target (X: %d, Y: %d)\n", pkt.hookTargetX, pkt.hookTargetY);
                    Vector2 target = { static_cast<float>(pkt.hookTargetX), static_cast<float>(pkt.hookTargetY) };
                    Vector2 direction = { target.x - hookTip.x, target.y - hookTip.y };
                    remote->getFishingProjectile()->retract(false);
                    remote->getFishingProjectile()->cast(hookTip, direction, target);
                    // Use seed-based broadcast so clients can deterministically generate particle positions locally
                    const int count = 10;
                    const float duration = 4.5f;
                    const int zidx = LAYER_PARTICLE;
                    const float spread = 12.0f;
                    // Compute spawn delay deterministically on host
                    std::uniform_real_distribution<float> delayDist(2.6f, 5.0f);
                    float delay = delayDist(netRng);
                    // Compute center for noisy starts deterministically on host
                    std::uniform_real_distribution<float> radiusDist(40.0f, 140.0f);
                    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
                    float radius = radiusDist(netRng);
                    float angle = angleDist(netRng);
                    Vector2 startCenter = { hookTip.x + std::cos(angle) * radius, hookTip.y + std::sin(angle) * radius };

                    // Create compact ParticlePacket that contains seed + center; clients will reproduce exact noisy starts
                    uint32_t seed = static_cast<uint32_t>(netRng());
                    ParticlePacket p{};
                    p.magic = 0x54524150; // 'PART'
                    p.ownerId = pkt.clientId;
                    p.seed = seed;
                    p.startX = startCenter.x;
                    p.startY = startCenter.y;
                    p.destX = hookTip.x;
                    p.destY = hookTip.y;
                    p.delay = delay;
                    p.count = static_cast<uint8_t>(count);
                    p.duration = duration;
                    p.zIndex = zidx;
                    p.spread = spread;
                    p.r = 0; p.g = 255; p.b = 0; p.a = 255;

                    size_t totalSize = sizeof(p);
                    UDPpacket* out = SDLNet_AllocPacket(static_cast<int>(totalSize));
                    std::memcpy(out->data, &p, sizeof(p));
                    out->len = static_cast<uint16_t>(totalSize);
                    for (auto& addr : clientAddrs) {
                        out->address = addr;
                        SDLNet_UDP_Send(udpSocket, -1, out);
                    }
                    SDLNet_FreePacket(out);

                    // Schedule host-side spawn using seed so host matches clients
                    if (remote->getFishingProjectile()) {
                        remote->getFishingProjectile()->cancelPendingAttract();
                        remote->getFishingProjectile()->scheduleAttractFromSeed(seed, count, SDL_Color{0,255,0,255}, duration, zidx, spread, startCenter, true, (pkt.clientId == clientId), delay);
                    }

                    // If this owner is the host itself (ownerId == clientId), also schedule on local player representation
                    if (pkt.clientId == clientId) {
                        if (player && player->getFishingProjectile()) {
                            player->getFishingProjectile()->cancelPendingAttract();
                            player->getFishingProjectile()->scheduleAttractFromSeed(seed, count, SDL_Color{0,255,0,255}, duration, zidx, spread, startCenter, true, true, delay);
                        }
                    }
                }
            }
        }
    }
    SDLNet_FreePacket(in);
}

// Broadcast a compact particle seed packet for a host-initiated cast
void hostBroadcastParticleForHook(const Vector2& hookTip) {
    if (!udpSocket || !isHost || clientAddrs.empty()) return;

    const int count = 10;
    const float duration = 4.5f;
    const int zidx = LAYER_PARTICLE;
    const float spread = 12.0f;
    std::uniform_real_distribution<float> delayDist(2.6f, 5.0f);
    float delay = delayDist(netRng);
    std::uniform_real_distribution<float> radiusDist(40.0f, 140.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
    float radius = radiusDist(netRng);
    float angle = angleDist(netRng);
    Vector2 startCenter = { hookTip.x + std::cos(angle) * radius, hookTip.y + std::sin(angle) * radius };

    uint32_t seed = static_cast<uint32_t>(netRng());
    ParticlePacket p{};
    p.magic = 0x54524150; // 'PART'
    p.ownerId = clientId; // host's ID
    p.seed = seed;
    p.startX = startCenter.x;
    p.startY = startCenter.y;
    p.destX = hookTip.x;
    p.destY = hookTip.y;
    p.delay = delay;
    p.count = static_cast<uint8_t>(count);
    p.duration = duration;
    p.zIndex = zidx;
    p.spread = spread;
    p.r = 0; p.g = 255; p.b = 0; p.a = 255;

    size_t totalSize = sizeof(p);
    UDPpacket* out = SDLNet_AllocPacket(static_cast<int>(totalSize));
    if (!out) return;
    std::memcpy(out->data, &p, sizeof(p));
    out->len = static_cast<uint16_t>(totalSize);
    for (auto& addr : clientAddrs) {
        out->address = addr;
        SDLNet_UDP_Send(udpSocket, -1, out);
    }
    SDLNet_FreePacket(out);

    // Schedule host-side spawn on local player so host sees the same behavior
    if (player && player->getFishingProjectile()) {
        player->getFishingProjectile()->cancelPendingAttract();
        player->getFishingProjectile()->scheduleAttractFromSeed(seed, count, SDL_Color{0,255,0,255}, duration, zidx, spread, startCenter, true, true, delay);
    }
}

// Broadcast authoritative hook arrival to clients
void hostBroadcastHookArrival(uint32_t ownerId, const Vector2& pos) {
    if (!udpSocket || !isHost || clientAddrs.empty()) return;

    HookArrivalPacket hp{};
    hp.magic = 0x52414B48; // 'HKAR'
    hp.ownerId = ownerId;
    hp.x = pos.x;
    hp.y = pos.y;

    size_t totalSize = sizeof(hp);
    UDPpacket* out = SDLNet_AllocPacket(static_cast<int>(totalSize));
    if (!out) return;
    std::memcpy(out->data, &hp, sizeof(hp));
    out->len = static_cast<uint16_t>(totalSize);
    for (auto& addr : clientAddrs) {
        out->address = addr;
        SDLNet_UDP_Send(udpSocket, -1, out);
    }
    SDLNet_FreePacket(out);

    SDL_Log("Host broadcast hook arrival for owner=%u at (%.2f,%.2f)", ownerId, pos.x, pos.y);
}

// Spawn a simple fish GameObject at the given world position or start a local minigame if this is our hook
void onHook(const Vector2& pos) {
    if (!g_renderer) return;
    // If this arrival corresponds to our local player's active hook, start the timed-click minigame instead
    if (player && player->getFishingProjectile() && player->getFishingProjectile()->getIsActive()) {
        Vector2 hookPos = player->getFishingProjectile()->getWorldPosition();
        float dx = hookPos.x - pos.x;
        float dy = hookPos.y - pos.y;
        float dist2 = dx*dx + dy*dy;
        if (dist2 < 4.0f * 4.0f) { // close enough to be our hook
            // Randomly select a minigame type
            std::uniform_int_distribution<int> mgDist(0,1);
            int pick = mgDist(fishingMinigameRng);
            fishingMinigameType = (pick == 0) ? MINIGAME_TIMED_CLICK : MINIGAME_TUG_OF_THE_DEEP;

            // Initialize common minigame parameters
            fishingMinigameActive = true;
            fishingMinigameTimer = 0.0f;
            fishingMinigameDuration = 4.5f; // eased timeout
            fishingMinigameHookPos = pos;

            // Compute initial screen rect immediately so clicks during the same frame register
            if (camera) {
                Vector2 hookWorld = fishingMinigameHookPos;
                Vector2 camPos = camera->getPosition();
                float zoom = camera->getZoom();
                const int barW = 200;
                const int barH = 20;
                int sx = static_cast<int>((hookWorld.x - camPos.x) * zoom) - barW/2;
                int sy = static_cast<int>((hookWorld.y - camPos.y) * zoom) - 48; // above hook
                sx = std::max(8, std::min(sx, WIN_WIDTH - barW - 8));
                sy = std::max(8, std::min(sy, WIN_HEIGHT - barH - 8));
                fishingMinigameScreenRect = { sx, sy, barW, barH };
            }

            if (fishingMinigameType == MINIGAME_TIMED_CLICK) {
                // Timed-click setup
                fishingMinigameIndicator = 0.0f;
                fishingMinigameIndicatorDir = 1.0f;
                std::uniform_real_distribution<float> centerDist(0.25f, 0.75f);
                float center = centerDist(fishingMinigameRng);
                float width = 0.20f; // 20% target window (easier)
                fishingMinigameWindowStart = std::max(0.0f, center - width/2.0f);
                fishingMinigameWindowEnd = std::min(1.0f, center + width/2.0f);
                SDL_Log("Fishing minigame started: TIMED_CLICK window=(%.3f-%.3f) screenrect=(%d,%d,%d,%d)", fishingMinigameWindowStart, fishingMinigameWindowEnd, fishingMinigameScreenRect.x, fishingMinigameScreenRect.y, fishingMinigameScreenRect.w, fishingMinigameScreenRect.h);
            } else {
                // Tug-of-the-deep setup
                tugProgress = 0.5f;
                tugTension = 0.0f;
                tugFishForce = 0.12f + (static_cast<float>(fishingMinigameRng()%30) / 300.0f); // 0.12 - 0.22
                std::uniform_real_distribution<float> burstDist(0.8f, 2.0f);
                tugNextBurstTime = fishingMinigameTimer + burstDist(fishingMinigameRng);
                tugBurstRemaining = 0.0f;
                tugStamina = 3;
                tugLastPullTime = -10.0f;
                tugPlayerPullLevel = 0.0f;
                SDL_Log("Fishing minigame started: TUG_OF_THE_DEEP fishForce=%.3f screenrect=(%d,%d,%d,%d)", tugFishForce, fishingMinigameScreenRect.x, fishingMinigameScreenRect.y, fishingMinigameScreenRect.w, fishingMinigameScreenRect.h);
            }

            // Do not spawn a free fish; wait for minigame result
            return;
        }
    }

    // Otherwise spawn a free fish in the world (remote player or missed minigame)
    
} 


// Helper function to check collision between two collision shapes (handles single and multi-rectangle)
bool checkCollision(const std::vector<Rectangle>& shapeA, 
                    const std::vector<Rectangle>& shapeB) {
    
    
    // Check all combinations of rectangles
    for (const auto& rectA : shapeA) {
        for (const auto& rectB : shapeB) {
            if (rectA.intersects(rectB)) {
                return true;
            }
        }
    }
    
    return false;
}



void broadcastSnapshot() {
    if (!udpSocket || !isHost || clientAddrs.empty()) return;
    
    static uint32_t tick = 0;
    std::vector<PlayerState> states;
    
    // Add local player
    Vector2 pos = player->getWorldPosition();
    Vector2 vel = player->getVelocity();
    uint8_t onBoat = boat->isPlayerOnBoard(player) ? 1 : 0;
    uint8_t hooking = player->isRodVisible() ? 1 : 0;
    uint8_t fishingHookActive = player->getFishingProjectile() && player->getFishingProjectile()->getIsActive() ? 1 : 0;
    float fishingHookX = 0.0f, fishingHookY = 0.0f, fishingHookTargetX = 0.0f, fishingHookTargetY = 0.0f;
    if (fishingHookActive) {
        Vector2 hookPos = player->getFishingProjectile()->getWorldPosition();
        fishingHookX = hookPos.x;
        fishingHookY = hookPos.y;
        fishingHookTargetX = player->getFishingProjectile()->getTargetPos().x;
        fishingHookTargetY = player->getFishingProjectile()->getTargetPos().y;
    }
    states.push_back({0, pos.x, pos.y, vel.x, vel.y, 0, onBoat, hooking, fishingHookActive, fishingHookX, fishingHookY, fishingHookTargetX, fishingHookTargetY});
    
    // Add remote players
    for (auto& [id, p] : remotePlayers) {
        Vector2 rpos = p->getWorldPosition();
        Vector2 rvel = p->getVelocity();
        uint8_t rOnBoat = boat->isPlayerOnBoard(p) ? 1 : 0;
        uint8_t rHooking = p->isRodVisible() ? 1 : 0;
        uint8_t rFishingHookActive = p->getFishingProjectile() && p->getFishingProjectile()->getIsActive() ? 1 : 0;
        float rFishingHookX = 0.0f, rFishingHookY = 0.0f, rFishingHookTargetX = 0.0f, rFishingHookTargetY = 0.0f;
        if (rFishingHookActive) {
            Vector2 hookPos = p->getFishingProjectile()->getWorldPosition();
            rFishingHookX = hookPos.x;
            rFishingHookY = hookPos.y;
            rFishingHookTargetX = p->getFishingProjectile()->getTargetPos().x;
            rFishingHookTargetY = p->getFishingProjectile()->getTargetPos().y;
        }
        states.push_back({id, rpos.x, rpos.y, rvel.x, rvel.y, 0, rOnBoat, rHooking, rFishingHookActive, rFishingHookX, rFishingHookY, rFishingHookTargetX, rFishingHookTargetY});
    }
    
    // Boat state
    BoatState boatState;
    Vector2 boatPos = boat->getWorldPosition();
    Vector2 navDir = boat->getNavigationDirection();
    boatState.x = boatPos.x;
    boatState.y = boatPos.y;
    boatState.rotation = boat->getRotation();
    boatState.navDirX = navDir.x;
    boatState.navDirY = navDir.y;
    boatState.isMoving = boat->getIsMoving() ? 1 : 0;
    
    SnapshotHeader header{tick++, static_cast<uint32_t>(states.size()), 1};
    size_t totalSize = sizeof(header) + sizeof(boatState) + states.size() * sizeof(PlayerState);
    
    UDPpacket* out = SDLNet_AllocPacket(static_cast<int>(totalSize));
    std::memcpy(out->data, &header, sizeof(header));
    std::memcpy(out->data + sizeof(header), &boatState, sizeof(boatState));
    std::memcpy(out->data + sizeof(header) + sizeof(boatState), states.data(), states.size() * sizeof(PlayerState));
    out->len = static_cast<uint16_t>(totalSize);
    
    for (auto& addr : clientAddrs) {
        out->address = addr;
        SDLNet_UDP_Send(udpSocket, -1, out);
    }
    SDLNet_FreePacket(out);
}

float hitBoxDistance(std::vector<Rectangle> shapeA, std::vector<Rectangle> shapeB) {
    float minDist = std::numeric_limits<float>::max();
    for (const auto& rectA : shapeA) {
        for (const auto& rectB : shapeB) {
            float dist = rectA.dist(rectB);
            if (dist < minDist) {
                minDist = dist;
            }
        }
    }
    return minDist;
}

int main(int argc, char* argv[]) {
    // Parse command-line args
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--host" && i + 1 < argc) {
            isHost = true;
            int port = std::stoi(argv[++i]);
            if (SDLNet_Init() < 0) {
                std::cerr << "SDLNet_Init failed: " << SDLNet_GetError() << "\n";
                return 1;
            }
            udpSocket = SDLNet_UDP_Open(port);
            if (!udpSocket) {
                std::cerr << "UDP_Open failed (host): " << SDLNet_GetError() << "\n";
                return 1;
            }
            clientId = 0; // Host is always ID 0
            std::cout << "Hosting on port " << port << "\n";
        } else if (std::string(argv[i]) == "--connect" && i + 2 < argc) {
            const char* ip = argv[++i];
            int port = std::stoi(argv[++i]);
            if (SDLNet_Init() < 0) {
                std::cerr << "SDLNet_Init failed: " << SDLNet_GetError() << "\n";
                return 1;
            }
            if (SDLNet_ResolveHost(&hostAddr, ip, port) < 0) {
                std::cerr << "ResolveHost failed: " << SDLNet_GetError() << "\n";
                return 1;
            }
            udpSocket = SDLNet_UDP_Open(0);
            if (!udpSocket) {
                std::cerr << "UDP_Open failed (client): " << SDLNet_GetError() << "\n";
                return 1;
            }
            // Generate unique client ID (never 0, that's reserved for host)
            clientId = 1 + (SDL_GetTicks() % 0xFFFFFFFE);
            std::cout << "Connecting to " << ip << ":" << port << " as client " << clientId << "\n";
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // Initialize audio subsystem (SDL_mixer)
    if (!SoundManager::instance().init()) {
        std::cerr << "Warning: audio initialization failed, continuing without sound.\n";
    } else {
        // Load placeholder sounds (replace paths with actual assets)
        SoundManager::instance().loadSound("walk", "./sounds/walk_loop.wav");
        SoundManager::instance().loadSound("cast", "./sounds/cast.wav");
        SoundManager::instance().loadSound("attract_spawn", "./sounds/attract_spawn.wav");
        SoundManager::instance().loadSound("attract_arrival", "./sounds/attract_arrival.wav");
        // Minigame sounds
        SoundManager::instance().loadSound("catch", "./sounds/catch.wav");
        SoundManager::instance().loadSound("escape", "./sounds/escape.wav");
    }

    SDL_Window* window = SDL_CreateWindow(
        ("Fish Game  " + std::string(isHost ? "(Host)" : "(Client)")).c_str(),// Add host client info to title
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIN_WIDTH,
        WIN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    g_renderer = renderer;

    // Load navigation clock texture
    SDL_Surface* navClockSurface = SDL_LoadBMP("./sprites/navigation_clock.bmp");
    if(navClockSurface){
        navigationClockTexture = SDL_CreateTextureFromSurface(renderer, navClockSurface);
        SDL_FreeSurface(navClockSurface);
        if(navigationClockTexture){
            SDL_Log("Navigation clock texture loaded successfully");
        } else {
            std::cerr << "Failed to create texture from navigation_clock.bmp: " << SDL_GetError() << "\n";
        }
    } else {
        std::cerr << "Failed to load navigation_clock.bmp: " << SDL_GetError() << "\n";
    }

    // Load navigation indicator texture
    SDL_Surface* navIndicatorSurface = SDL_LoadBMP("./sprites/navigation_indicator.bmp");
    if(navIndicatorSurface){
        navigationIndicatorTexture = SDL_CreateTextureFromSurface(renderer, navIndicatorSurface);
        SDL_FreeSurface(navIndicatorSurface);
        if(navigationIndicatorTexture){
            SDL_Log("Navigation indicator texture loaded successfully");
        } else {
            std::cerr << "Failed to create texture from navigation_indicator.bmp: " << SDL_GetError() << "\n";
        }
    } else {
        std::cerr << "Failed to load navigation_indicator.bmp: " << SDL_GetError() << "\n";
    }

    // Seed randomness for environment variation
    srand(static_cast<unsigned>(SDL_GetTicks()));

    bool running = true;
    SDL_Event event;

    const char* playerSpritePaths[] = {
        "./sprites/Boy_Walk1.bmp",
        "./sprites/Boy_Walk2.bmp",
        "./sprites/Boy_Walk3.bmp",
        "./sprites/Boy_Walk4.bmp"
    };

    player = new Player({0.0f, 0.0f},{2.0f,2.0f}, playerSpritePaths,4, renderer,0.1f,LAYER_PLAYER);
    // If running as host, broadcast hook arrival when our local hook arrives
    if (isHost && player->getFishingProjectile()) {
        player->getFishingProjectile()->setOnHookArrival([](const Vector2& pos){
            hostBroadcastHookArrival(clientId, pos);
        });
    }
    // Spawn fish when attract arrival occurs for local player's hook (always)
    if (player->getFishingProjectile()) {
        player->getFishingProjectile()->setOnAttractArrival([&](){
            Vector2 p = player->getFishingProjectile()->getWorldPosition();
            SDL_Log("Local player attract arrival - spawning fish at (%.2f,%.2f)", p.x, p.y);
            onHook(p);
        });
    }

    const char* boatSpritePaths[] = {
        "./sprites/Boat1.bmp",
        "./sprites/Boat2.bmp",
        "./sprites/Boat3.bmp",
        "./sprites/Boat4.bmp"
    };

    boat = new Boat({430.0f, 280.0f}, {3.0f, 3.0f}, boatSpritePaths, 4, renderer, 0.2f, LAYER_BOAT, std::set<SDL_Keycode>{SDLK_f,SDLK_e,SDLK_b}, &navigationUIActive);
   



    camera = new Camera({0.0f, 0.0f}, {WIN_WIDTH, WIN_HEIGHT},2.0f);
    
    camera->follow(player);

        
    
    gameObjects.push_back(player);
    gameObjects.push_back(boat);
    
    // Add remote players to game objects if they exist
    for (auto& [id, remote] : remotePlayers) {
        gameObjects.push_back(remote);
    }
    
    auto ensureChunksAround = [&](SDL_Renderer* rend, Vector2 playerPos, int radius){
        int cx = static_cast<int>(std::floor(playerPos.x / CHUNK_SIZE_PX));
        int cy = static_cast<int>(std::floor(playerPos.y / CHUNK_SIZE_PX));
        for(int dy = -radius; dy <= radius; ++dy){
            for(int dx = -radius; dx <= radius; ++dx){
                int nx = cx + dx;
                int ny = cy + dy;
                std::pair<int,int> key{nx, ny};
                if(generatedChunks.find(key) != generatedChunks.end()) continue;
                Rectangle area{{nx * static_cast<float>(CHUNK_SIZE_PX), ny * static_cast<float>(CHUNK_SIZE_PX)},
                               {(nx+1) * static_cast<float>(CHUNK_SIZE_PX), (ny+1) * static_cast<float>(CHUNK_SIZE_PX)}};
                // Deterministic per-chunk seed
                uint32_t seed = static_cast<uint32_t>((nx * 73856093) ^ (ny * 19349663) ^ 0x9E3779B9);
                auto env = generateInitialEnvironment(rend, area, seed);
                // Append to global render list
                gameObjects.insert(gameObjects.end(), env.begin(), env.end());
                generatedChunks.insert(key);

                // If host, broadcast chunk to clients
                if (isHost && udpSocket && !clientAddrs.empty()) {
                    ChunkPacket pkt;
                    pkt.magic = 0x4B4E4843; // 'CHNK'
                    pkt.cx = nx;
                    pkt.cy = ny;
                    pkt.seed = seed;
                    UDPpacket* out = SDLNet_AllocPacket(sizeof(pkt));
                    std::memcpy(out->data, &pkt, sizeof(pkt));
                    out->len = sizeof(pkt);
                    for (auto& addr : clientAddrs) {
                        out->address = addr;
                        SDLNet_UDP_Send(udpSocket, -1, out);
                    }
                    SDLNet_FreePacket(out);
                }
            }
        }
    };

    // Generate initial chunks around player
    ensureChunksAround(renderer, player->getWorldPosition(), 1);

    // Chunks added directly to gameObjects in ensureChunksAround

    
    Vector2 lighthousePos = {600.0f, 200.0f};
    Vector2 lighthouseSizeMultiplier = {6.0f, 6.0f};

    ICollidable* lighthouse = new ICollidable(
        lighthousePos,
        lighthouseSizeMultiplier,
        "./sprites/lighthouse_tower.bmp",
        renderer,
        true,
        LAYER_LIGHTHOUSE,
        10  // Smaller minClusterSize for more detailed hitboxes
    );

    GameObject* lighthouseGround = new GameObject(
        {lighthousePos.x, lighthousePos.y},
        lighthouseSizeMultiplier,
        "./sprites/lighthouse_ground.bmp",
        renderer,
        1
    );
    

    gameObjects.push_back(lighthouse);
    gameObjects.push_back(lighthouseGround);
    
    // Add fishing hooks to game objects
    if (player->getFishingProjectile()) {
        gameObjects.push_back(player->getFishingProjectile());
    }
    for (auto& [id, remote] : remotePlayers) {
        if (remote->getFishingProjectile()) {
            gameObjects.push_back(remote->getFishingProjectile());
        }
    }
    

    Uint64 prev = SDL_GetPerformanceCounter();
    double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    std::set<std::pair<ICollidable*, ICollidable*>> collisionPairs; // TODO: Make this work

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (now - prev) / freq; // seconds since last frame
        prev = now;

        // Update fishing minigame state (indicator movement and timeout) EARLY so input sees the visible indicator
        if (fishingMinigameActive) {
            if (!navigationUIActive && !inventoryOpen) {
                fishingMinigameTimer += static_cast<float>(dt);

                // Compute screen-space rectangle for the minigame bar near the hook position
                if (camera) {
                    Vector2 hookWorld = fishingMinigameHookPos;
                    Vector2 camPos = camera->getPosition();
                    float zoom = camera->getZoom();
                    const int barW = 200;
                    const int barH = 20;
                    int sx = static_cast<int>((hookWorld.x - camPos.x) * zoom) - barW/2;
                    int sy = static_cast<int>((hookWorld.y - camPos.y) * zoom) - 48; // above hook
                    // Clamp to window bounds
                    sx = std::max(8, std::min(sx, WIN_WIDTH - barW - 8));
                    sy = std::max(8, std::min(sy, WIN_HEIGHT - barH - 8));
                    fishingMinigameScreenRect = { sx, sy, barW, barH };
                }

                if (fishingMinigameType == MINIGAME_TIMED_CLICK) {
                    // Move indicator across 0..1, bounce at edges
                    fishingMinigameIndicator += fishingMinigameIndicatorDir * fishingMinigameIndicatorSpeed * static_cast<float>(dt);
                    if (fishingMinigameIndicator > 1.0f) {
                        fishingMinigameIndicator = 1.0f;
                        fishingMinigameIndicatorDir = -fishingMinigameIndicatorDir;
                    } else if (fishingMinigameIndicator < 0.0f) {
                        fishingMinigameIndicator = 0.0f;
                        fishingMinigameIndicatorDir = -fishingMinigameIndicatorDir;
                    }

                    // Timeout: failure if not clicked within duration
                    if (fishingMinigameTimer >= fishingMinigameDuration) {
                        SDL_Log("Fishing minigame: timeout (failed)");
                        SoundManager::instance().playSound("escape", 0, MIX_MAX_VOLUME);
                        if (player && player->getFishingProjectile()) player->getFishingProjectile()->retract();
                        fishingMinigameActive = false;
                    }
                } else if (fishingMinigameType == MINIGAME_TUG_OF_THE_DEEP) {
                    // Tug: fish force and occasional bursts
                    if (tugBurstRemaining > 0.0f) {
                        // burst is active, reduce remaining
                        tugBurstRemaining -= static_cast<float>(dt);
                    } else if (fishingMinigameTimer >= tugNextBurstTime) {
                        // trigger a short burst
                        tugBurstRemaining = 0.25f + (static_cast<float>(fishingMinigameRng()%20) / 100.0f); // 0.25 - 0.45s
                        tugNextBurstTime = fishingMinigameTimer + (0.8f + (static_cast<float>(fishingMinigameRng()%120) / 100.0f));
                    }
                    float activeFishForce = tugFishForce + (tugBurstRemaining > 0.0f ? 0.18f : 0.0f);

                    // Apply fish force to progress
                    tugProgress += activeFishForce * static_cast<float>(dt);

                    // Decay player's pull level over time
                    tugPlayerPullLevel = std::max(0.0f, tugPlayerPullLevel - static_cast<float>(dt) * 0.6f);

                    // Recover stamina slowly
                    if (tugLastPullTime + 1.0f < fishingMinigameTimer) {
                        tugStamina = std::min(3, tugStamina + 1);
                        tugLastPullTime = fishingMinigameTimer; // throttle recovery increment once
                    }

                    // Compute tension; when fish force exceeds player's pull, tension increases
                    float tensionDelta = activeFishForce - tugPlayerPullLevel;
                    tugTension += std::max(0.0f, tensionDelta) * 0.8f * static_cast<float>(dt);
                    tugTension = std::clamp(tugTension, 0.0f, 1.0f);

                    // Win/lose checks
                    if (tugProgress <= TUG_WIN_THRESHOLD) {
                        SDL_Log("Fishing minigame: TUG success! progress=%.3f", tugProgress);
                        // success will be handled where clicks are processed (spawn fish below)
                        // to keep consistent, set minigame inactive and spawn fish here
                        SoundManager::instance().playSound("catch", 0, MIX_MAX_VOLUME);
                        if (g_renderer) {
                            GameObject* caught = new GameObject(fishingMinigameHookPos, {2.0f,2.0f}, "./sprites/fish.bmp", g_renderer, LAYER_PARTICLE);
                            gameObjects.push_back(caught);
                            fishesMovingToPlayer.push_back(caught);
                            SDL_Log("Caught fish spawned at (%.2f,%.2f) and marked for collection", fishingMinigameHookPos.x, fishingMinigameHookPos.y);
                        }
                        if (player && player->getFishingProjectile()) player->getFishingProjectile()->retract();
                        fishingMinigameActive = false;
                    } else if (tugProgress >= TUG_FAIL_THRESHOLD || tugTension >= 1.0f) {
                        SDL_Log("Fishing minigame: TUG failed. progress=%.3f tension=%.3f", tugProgress, tugTension);
                        SoundManager::instance().playSound("escape", 0, MIX_MAX_VOLUME);
                        if (player && player->getFishingProjectile()) player->getFishingProjectile()->retract();
                        fishingMinigameActive = false;
                    }
                }
            }
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                // If a fishing minigame is active, intercept left-clicks to resolve it and suppress casting
                if (fishingMinigameActive && event.button.button == SDL_BUTTON_LEFT) {
                    // Recompute screen rect at click time to match what is drawn (camera may have moved since last frame)
                    if (camera) {
                        Vector2 hookWorld = fishingMinigameHookPos;
                        Vector2 camPos = camera->getPosition();
                        float zoom = camera->getZoom();
                        const int barW = 200;
                        const int barH = 20;
                        int sx = static_cast<int>((hookWorld.x - camPos.x) * zoom) - barW/2;
                        int sy = static_cast<int>((hookWorld.y - camPos.y) * zoom) - 48; // above hook
                        sx = std::max(8, std::min(sx, WIN_WIDTH - barW - 8));
                        sy = std::max(8, std::min(sy, WIN_HEIGHT - barH - 8));
                        fishingMinigameScreenRect = { sx, sy, barW, barH };
                    }

                    int mx = event.button.x;
                    int my = event.button.y;
                    // Compute the same pixel positions used for rendering and evaluate success in pixel-space to avoid rounding/timing mismatches
                    SDL_Rect barBg = fishingMinigameScreenRect;
                    int winX = static_cast<int>(barBg.x + fishingMinigameWindowStart * barBg.w);
                    int winW = static_cast<int>((fishingMinigameWindowEnd - fishingMinigameWindowStart) * barBg.w);
                    SDL_Rect winRect = { winX, barBg.y, winW, barBg.h };
                    int indX = static_cast<int>(barBg.x + fishingMinigameIndicator * barBg.w);

                    bool inside = (mx >= barBg.x && mx <= barBg.x + barBg.w && my >= barBg.y && my <= barBg.y + barBg.h);

                    // Handle minigame-specific click logic
                    if (fishingMinigameType == MINIGAME_TIMED_CLICK) {
                        bool success = false;
                        // Evaluate success using current indicator and window
                        if (fishingMinigameWindowStart <= fishingMinigameIndicator && fishingMinigameIndicator <= fishingMinigameWindowEnd) {
                            success = true;
                        }
                        fishingMinigameAttempts++;
                        if (success) {
                            SDL_Log("Fishing minigame: Success! indicator=%.3f window=(%.3f-%.3f)", fishingMinigameIndicator, fishingMinigameWindowStart, fishingMinigameWindowEnd);
                            SoundManager::instance().playSound("catch", 0, MIX_MAX_VOLUME);
                            if (g_renderer) {
                                GameObject* caught = new GameObject(fishingMinigameHookPos, {2.0f,2.0f}, "./sprites/fish.bmp", g_renderer, LAYER_PARTICLE);
                                gameObjects.push_back(caught);
                                fishesMovingToPlayer.push_back(caught);
                                SDL_Log("Caught fish spawned at (%.2f,%.2f) and marked for collection", fishingMinigameHookPos.x, fishingMinigameHookPos.y);
                            }
                        } else {
                            SDL_Log("Fishing minigame: Fail. indicator=%.3f window=(%.3f-%.3f)", fishingMinigameIndicator, fishingMinigameWindowStart, fishingMinigameWindowEnd);
                            SoundManager::instance().playSound("escape", 0, MIX_MAX_VOLUME);
                        }

                        // End minigame and retract
                        if (player && player->getFishingProjectile()) player->getFishingProjectile()->retract();
                        fishingMinigameActive = false;
                        continue; // consume click and suppress normal casting
                    } else if (fishingMinigameType == MINIGAME_TUG_OF_THE_DEEP) {
                        // Tug: clicks inside bar apply pull; clicks outside are consumed as fails (but do not necessarily end the minigame)
                        if (!inside) {
                            SDL_Log("TUG: Click outside bar consumed (no pull)");
                            // Consume click but keep minigame active
                            fishingMinigameAttempts++;
                            continue;
                        }

                        float nowT = fishingMinigameTimer;
                        float pull = TUG_PULL_BASE;
                        if (nowT - tugLastPullTime < 0.45f && tugStamina > 0) {
                            pull += TUG_PULL_BONUS;
                            tugStamina = std::max(0, tugStamina - 1);
                        }
                        // Apply pull
                        tugProgress -= pull;
                        tugPlayerPullLevel = std::max(tugPlayerPullLevel, pull);
                        tugLastPullTime = nowT;
                        fishingMinigameAttempts++;
                        SDL_Log("TUG: applied pull=%.3f progress=%.3f tension=%.3f stamina=%d", pull, tugProgress, tugTension, tugStamina);

                        // Immediate success check
                        if (tugProgress <= TUG_WIN_THRESHOLD) {
                            SDL_Log("Fishing minigame: TUG success! progress=%.3f", tugProgress);
                            SoundManager::instance().playSound("catch", 0, MIX_MAX_VOLUME);
                            if (g_renderer) {
                                GameObject* caught = new GameObject(fishingMinigameHookPos, {2.0f,2.0f}, "./sprites/fish.bmp", g_renderer, LAYER_PARTICLE);
                                gameObjects.push_back(caught);
                                fishesMovingToPlayer.push_back(caught);
                                SDL_Log("Caught fish spawned at (%.2f,%.2f) and marked for collection", fishingMinigameHookPos.x, fishingMinigameHookPos.y);
                            }
                            if (player && player->getFishingProjectile()) player->getFishingProjectile()->retract();
                            fishingMinigameActive = false;
                            continue;
                        }

                        // Immediate fail check (line tension or progress)
                        if (tugTension >= 1.0f || tugProgress >= TUG_FAIL_THRESHOLD) {
                            SDL_Log("Fishing minigame: TUG failed. progress=%.3f tension=%.3f", tugProgress, tugTension);
                            SoundManager::instance().playSound("escape", 0, MIX_MAX_VOLUME);
                            if (player && player->getFishingProjectile()) player->getFishingProjectile()->retract();
                            fishingMinigameActive = false;
                            continue;
                        }

                        // Otherwise keep the minigame active (do not retract yet) and consume the click
                        continue;
                    }
                }

                // Handle mouse click for fishing hook casting
                if (!navigationUIActive) {
                    player->onMouseDown(event.button.button, event.button.x, event.button.y, 
                                       camera->getPosition(), camera->getZoom());
                }
            }
            else if(event.type == SDL_KEYDOWN){
                // Inventory open while Tab is held
                if(event.key.keysym.sym == SDLK_TAB) {
                    inventoryOpen = true;
                }
                
                if(event.key.keysym.sym == SDLK_0){
                    SDL_Log("Player position: (%.2f, %.2f)", player->getPosition()->x, player->getPosition()->y);
                }

                // Only local player handles local input
                for(GameObject* obj: gameObjects){
                    if(IInteractable* interactable = dynamic_cast<IInteractable*>(obj)){
                        if(interactable->getInteractKeys().find(event.key.keysym.sym) != interactable->getInteractKeys().end()){
                            // Check if this key wasn't already pressed (prevent key repeat)
                            if(pressedInteractKeys.find(event.key.keysym.sym) == pressedInteractKeys.end()){
                                pressedInteractKeys.insert(event.key.keysym.sym);
                                
                                // Check if the interactable is also collidable
                                ICollidable* interactableCollider = dynamic_cast<ICollidable*>(interactable);
                                ICollidable* playerCollider = dynamic_cast<ICollidable*>(player);
                                
                                if(interactableCollider && playerCollider){
                                    auto shapeA = interactableCollider->getCollisionBox();
                                    auto shapeB = playerCollider->getCollisionBox();
                                    
                                    if (hitBoxDistance(shapeA, shapeB) < 10.0f) { // Simple distance check for proximity
                                        // Handle B key for boarding/leaving boat
                                        if(event.key.keysym.sym == SDLK_b){
                                            if (isHost) {
                                                // Host handles it directly
                                                if(boat->isPlayerOnBoard(player)){
                                                    boat->leaveBoat(player);
                                                } else {
                                                    boat->boardBoat(player);
                                                }
                                            } else {
                                                // Client sends request to server
                                                clientBoardingRequest = true;
                                            }
                                        } else if(event.key.keysym.sym == SDLK_e){
                                            // Handle E key for boat movement
                                            if (isHost) {
                                                interactable->onInteract(event.key.keysym.sym);
                                            } else {
                                                // Client sends request to server
                                                clientBoatMovementToggle = true;
                                            }
                                        } else {
                                            interactable->onInteract(event.key.keysym.sym);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Only pass key to player if navigation UI is not active
                if(!navigationUIActive){
                    // Handle hook toggle separately for networking
                    if (event.key.keysym.sym == SDLK_r) {
                        if (isHost) {
                            // Host handles it directly
                            player->onKeyDown(event.key.keysym.sym);
                        } else {
                            // Client sends request to server
                            clientHookToggle = true;
                            // Also apply locally for immediate feedback
                            player->onKeyDown(event.key.keysym.sym);
                        }
                    } else {
                        player->onKeyDown(event.key.keysym.sym);
                    }
                }
                
            }
            else if(event.type == SDL_KEYUP){
                // Remove key from pressed set when released
                pressedInteractKeys.erase(event.key.keysym.sym);
                // Close inventory when Tab is released
                if(event.key.keysym.sym == SDLK_TAB) {
                    inventoryOpen = false;
                }
                // Only pass key to player if navigation UI is not active
                if(!navigationUIActive){
                    player->onKeyUp(event.key.keysym.sym);
                }
            }
        }
        
        // Network: client sends input, host receives
        if (!isHost && udpSocket) {
            sendInputPacket();
        }
        if (isHost && udpSocket) {
            receiveInputs();
        }
        
        // Sync remote players and receive chunk spawns from host
        if (!isHost && udpSocket) {
            // Modified to process both snapshots and chunk packets
            UDPpacket* in = SDLNet_AllocPacket(2048);
            while (SDLNet_UDP_Recv(udpSocket, in)) {
                // Check for compact particle seed packet first
                if (in->len >= sizeof(ParticlePacket)) {
                    ParticlePacket pp;
                    std::memcpy(&pp, in->data, sizeof(ParticlePacket));
                    if (pp.magic == 0x54524150) { // 'PART'
                        Player* targetPlayer = nullptr;
                        if (pp.ownerId == clientId) {
                            targetPlayer = player;
                        } else {
                            targetPlayer = getOrCreateRemotePlayer(pp.ownerId);
                        }
                        if (targetPlayer && targetPlayer->getFishingProjectile()) {
                            SDL_Color col{pp.r, pp.g, pp.b, pp.a};
                            bool playSound = (pp.ownerId == clientId);
                            Vector2 center{pp.startX, pp.startY};
                            targetPlayer->getFishingProjectile()->cancelPendingAttract();
                            // Use seed-based scheduling so clients reproduce positions locally
                            targetPlayer->getFishingProjectile()->scheduleAttractFromSeed(pp.seed, pp.count, col, pp.duration, pp.zIndex, pp.spread, center, true, playSound, pp.delay);
                        }
                        continue; // processed
                    }
                }

                // Check for hook arrival packet
                if (in->len >= sizeof(HookArrivalPacket)) {
                    HookArrivalPacket hp;
                    std::memcpy(&hp, in->data, sizeof(HookArrivalPacket));
                    if (hp.magic == 0x52414B48) { // 'HKAR'
                        Player* targetPlayer = nullptr;
                        if (hp.ownerId == clientId) {
                            targetPlayer = player;
                        } else {
                            targetPlayer = getOrCreateRemotePlayer(hp.ownerId);
                        }
                        if (targetPlayer && targetPlayer->getFishingProjectile()) {
                            Vector2 pos{hp.x, hp.y};
                            // Authoritative set: apply arrived position immediately
                            targetPlayer->getFishingProjectile()->setArrivedAt(pos);
                        }
                        continue; // processed
                    }
                }

                // Check for chunk packet first
                if (in->len >= sizeof(ChunkPacket)) {
                    ChunkPacket cp;
                    std::memcpy(&cp, in->data, sizeof(ChunkPacket));
                    if (cp.magic == 0x4B4E4843) { // 'CHNK'
                        std::pair<int,int> key{cp.cx, cp.cy};
                        if (generatedChunks.find(key) == generatedChunks.end()) {
                            Rectangle area{{cp.cx * static_cast<float>(CHUNK_SIZE_PX), cp.cy * static_cast<float>(CHUNK_SIZE_PX)},
                                           {(cp.cx+1) * static_cast<float>(CHUNK_SIZE_PX), (cp.cy+1) * static_cast<float>(CHUNK_SIZE_PX)}};
                            auto env = generateInitialEnvironment(renderer, area, cp.seed);
                            gameObjects.insert(gameObjects.end(), env.begin(), env.end());
                            generatedChunks.insert(key);
                        }
                        continue; // processed
                    }
                }

                // Otherwise try to interpret as snapshot
                if (in->len >= sizeof(SnapshotHeader)) {
                    SnapshotHeader header;
                    std::memcpy(&header, in->data, sizeof(header));
                    
                    size_t offset = sizeof(header);
                    
                    // Read boat state if present
                    if (header.hasBoat && in->len >= offset + sizeof(BoatState)) {
                        BoatState boatState;
                        std::memcpy(&boatState, in->data + offset, sizeof(boatState));
                        boat->setBoatState(boatState.x, boatState.y, boatState.rotation, 
                                          boatState.navDirX, boatState.navDirY, boatState.isMoving != 0);
                        offset += sizeof(BoatState);
                    }
                    
                    size_t expected = offset + header.playerCount * sizeof(PlayerState);
                    if (in->len >= expected) {
                        PlayerState* states = reinterpret_cast<PlayerState*>(in->data + offset);
                        for (uint32_t i = 0; i < header.playerCount; ++i) {
                            if (states[i].id == clientId) {
                                // Handle boarding state first
                                bool wasOnBoat = boat->isPlayerOnBoard(player);
                                bool shouldBeOnBoat = states[i].isOnBoat != 0;
                                
                                // Handle boarding/leaving transitions
                                if (shouldBeOnBoat && !wasOnBoat) {
                                    // Need to board - use world position, boardBoat will convert to local
                                    Vector2* pos = player->getPosition();
                                    pos->x = states[i].x;
                                    pos->y = states[i].y;
                                    boat->boardBoat(player);
                                } else if (!shouldBeOnBoat && wasOnBoat) {
                                    // Need to leave - leaveBoat will convert to world position
                                    boat->leaveBoat(player);
                                    Vector2* pos = player->getPosition();
                                    pos->x = states[i].x;
                                    pos->y = states[i].y;
                                } else {
                                    // No state change, just update position
                                    Vector2* pos = player->getPosition();
                                    if (shouldBeOnBoat) {
                                        // On boat - server sends world pos, convert to local
                                        Vector2 boatWorld = boat->getWorldPosition();
                                        pos->x = states[i].x - boatWorld.x;
                                        pos->y = states[i].y - boatWorld.y;
                                    } else {
                                        // Not on boat - server sends world pos, use directly
                                        pos->x = states[i].x;
                                        pos->y = states[i].y;
                                    }
                                }
                                
                                player->setVelocity({states[i].vx, states[i].vy});
                                player->setRodVisible(states[i].isHooking != 0);
                                // Do NOT sync local player's fishing hook from network snapshot (client should control its own hook)
                            } else {
                                Player* remote = getOrCreateRemotePlayer(states[i].id);
                                if (!remote) continue;
                                
                                // Handle remote player boarding state
                                bool wasOnBoat = boat->isPlayerOnBoard(remote);
                                bool shouldBeOnBoat = states[i].isOnBoat != 0;
                                // (fishing hook syncing moved below to ensure boarding/position changes applied first)
                                
                                if (shouldBeOnBoat && !wasOnBoat) {
                                    Vector2* rpos = remote->getPosition();
                                    rpos->x = states[i].x;
                                    rpos->y = states[i].y;
                                    boat->boardBoat(remote);
                                } else if (!shouldBeOnBoat && wasOnBoat) {
                                    boat->leaveBoat(remote);
                                    Vector2* rpos = remote->getPosition();
                                    rpos->x = states[i].x;
                                    rpos->y = states[i].y;
                                } else {
                                    Vector2* rpos = remote->getPosition();
                                    if (shouldBeOnBoat) {
                                        Vector2 boatWorld = boat->getWorldPosition();
                                        rpos->x = states[i].x - boatWorld.x;
                                        rpos->y = states[i].y - boatWorld.y;
                                    } else {
                                        rpos->x = states[i].x;
                                        rpos->y = states[i].y;
                                    }
                                }
                                
                                remote->setVelocity({states[i].vx, states[i].vy});
                                remote->setRodVisible(states[i].isHooking != 0);
                                // Sync fishing hook for remote player only (after applying boarding/position)
                                if (remote->getFishingProjectile()) {
                                    if (states[i].fishingHookActive) {
                                        Vector2 hookPos = {states[i].fishingHookX, states[i].fishingHookY};
                                        Vector2 hookTarget = {states[i].fishingHookTargetX, states[i].fishingHookTargetY};
                                        if (!remote->getFishingProjectile()->getIsActive()) {
                                            // Use the correct target for remote cast
                                            Vector2 direction = {hookTarget.x - hookPos.x, hookTarget.y - hookPos.y};
                                            // When reproducing remote casts from snapshots, do not play attract sounds on this client
                                            remote->getFishingProjectile()->cast(hookPos, direction, hookTarget, 200.0f, false);
                                        }
                                        // Snapshot indicates active -> cancel any pending retract
                                        remote->getFishingProjectile()->cancelPendingRetract();
                                        // Always update position and ensure visible if active
                                        Vector2* pos = remote->getFishingProjectile()->getPosition();
                                        pos->x = hookPos.x;
                                        pos->y = hookPos.y;
                                        remote->getFishingProjectile()->setVisible(true);
                                    } else {
                                        if (remote->getFishingProjectile()->getIsActive()) {
                                            // Start a short debounce before retracting to avoid snapshot jitter flicker
                                            remote->getFishingProjectile()->startRetractDebounce(0.12f);
                                        }
                                    }
                                }
                            }
                        }
                        continue;
                    }
                }
            }
            SDLNet_FreePacket(in);
            // Interpolate remote players using velocity between snapshots
            for (auto& [id, remote] : remotePlayers) {
                remote->applyVelocity(static_cast<float>(dt));
            }
        }
        // Ensure environment chunks exist around current player location
        ensureChunksAround(renderer, player->getWorldPosition(), 1);

        // Skip game updates when navigation UI is active or inventory is open
        if(!navigationUIActive && !inventoryOpen){
            // Update all objects first
            for(GameObject* obj: gameObjects){
                obj->update(static_cast<float>(dt));
            }

            // Then handle collisions - process each unique pair only once
            std::vector<ICollidable*> colliders;
            for(GameObject* obj: gameObjects){
                if(ICollidable* collider = dynamic_cast<ICollidable*>(obj)){
                    colliders.push_back(collider);
                }
            }


            for(size_t i = 0; i < colliders.size(); ++i){
                for(size_t j = i + 1; j < colliders.size(); ++j){
                    ICollidable* collider = colliders[i];
                    ICollidable* otherCollider = colliders[j];
                    
                    auto shapeA = collider->getCollisionBox();
                    auto shapeB = otherCollider->getCollisionBox();
                    
                    bool isColliding = checkCollision(shapeA, shapeB);
                    
                    
                    
                    // Use consistent pair ordering (smaller pointer first)
                    auto pair = (collider < otherCollider) ? 
                        std::make_pair(collider, otherCollider) : 
                        std::make_pair(otherCollider, collider);
                    
                    bool wasColliding = collisionPairs.find(pair) != collisionPairs.end();
                    
                    if(wasColliding){
                        if(!isColliding){
                            // Collision ended
                            collider->onCollisionLeave(otherCollider);
                            otherCollider->onCollisionLeave(collider);
                            collisionPairs.erase(pair);
                        } else {
                            // Collision continuing
                            collider->onCollisionStay(otherCollider);
                            otherCollider->onCollisionStay(collider);
                        }
                    } else if(isColliding){
                        // New collision
                        collider->onCollisionEnter(otherCollider);
                        otherCollider->onCollisionEnter(collider);
                        collisionPairs.insert(pair);
                    }
                }
            }
            
            // Host broadcasts snapshot
            if (isHost && udpSocket) {
                broadcastSnapshot();
            }
        }
        
        // (moved) fishing minigame update runs earlier now to ensure input sees the visible indicator

        // Move any caught fishes toward the player's world position and spawn UI icons when they reach the player
        if (!fishesMovingToPlayer.empty() && player) {
            std::vector<GameObject*> remaining;
            for (auto* fish : fishesMovingToPlayer) {
                if (!fish) continue;
                Vector2* fpos = fish->getPosition();
                Vector2 playerPos = player->getWorldPosition();
                float dx = playerPos.x - fpos->x;
                float dy = playerPos.y - fpos->y;
                float dist = std::sqrt(dx*dx + dy*dy);
                float moveSpeed = 160.0f; // pixels per second
                float step = moveSpeed * static_cast<float>(dt);
                if (dist <= step + 1.0f) {
                    // Collected: remove fish from world and add to first empty inventory slot
                    auto it = std::find(gameObjects.begin(), gameObjects.end(), fish);
                    if (it != gameObjects.end()) gameObjects.erase(it);
                    // Find first empty slot
                    int slotIndex = -1;
                    for (int si = 0; si < INV_COLS * INV_ROWS; ++si) {
                        if (!inventorySlots[si]) { slotIndex = si; break; }
                    }
                    if (slotIndex >= 0 && renderer) {
                        int cols = INV_COLS; int rows = INV_ROWS; int cellSize = INV_CELL_SIZE; int padding = INV_PADDING;
                        int gridWidth = cols * cellSize + (cols - 1) * padding;
                        int gridHeight = rows * cellSize + (rows - 1) * padding;
                        int startX = (WIN_WIDTH - gridWidth) / 2;
                        int startY = (WIN_HEIGHT - gridHeight) / 2;
                        int srow = slotIndex / cols;
                        int scol = slotIndex % cols;
                        SDL_Rect dstRect = { startX + scol * (cellSize + padding), startY + srow * (cellSize + padding), cellSize, cellSize };
                        // Create a static UI icon in that slot (start hidden; shown only when inventoryOpen)
                        // Do NOT add to gameObjects - we'll render icons only while inventory is open
                        UIGameObject* icon = new UIGameObject({static_cast<float>(dstRect.x), static_cast<float>(dstRect.y)}, {1.0f,1.0f}, "./sprites/fish.bmp", renderer, LAYER_UI);
                        icon->getSize()->x = static_cast<float>(cellSize);
                        icon->getSize()->y = static_cast<float>(cellSize);
                        icon->setVisible(false); // hide until inventory is opened
                        inventorySlots[slotIndex] = icon;
                        SDL_Log("Fish added to inventory slot %d", slotIndex);
                        delete fish;
                    } else {
                        // Inventory full: just delete the fish
                        delete fish;
                        SDL_Log("Inventory full, fish discarded");
                    }
                } else {
                    // Move toward player
                    fpos->x += (dx / dist) * step;
                    fpos->y += (dy / dist) * step;
                    remaining.push_back(fish);
                }
            }
            fishesMovingToPlayer = std::move(remaining);
        }



        // Ensure inventory icons are only visible when the inventory UI is open
        for (int si = 0; si < INV_COLS * INV_ROWS; ++si) {
            if (inventorySlots[si]) inventorySlots[si]->setVisible(inventoryOpen);
        }
        camera->render(renderer,gameObjects);


        // Render fishing lines after game objects but before UI
        if (player->getFishingProjectile()) {
            player->getFishingProjectile()->renderLine(renderer, camera->getPosition(), camera->getZoom());
            player->getFishingProjectile()->renderParticles(renderer, camera->getPosition(), camera->getZoom());
        }
        for (auto& [id, remote] : remotePlayers) {
            if (remote->getFishingProjectile()) {
                remote->getFishingProjectile()->renderLine(renderer, camera->getPosition(), camera->getZoom());
                remote->getFishingProjectile()->renderParticles(renderer, camera->getPosition(), camera->getZoom());
            }
        }

        // Inventory UI rendering
        if (inventoryOpen) {
            // Darken the entire screen
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); // More opaque black
            SDL_Rect screenRect = {0, 0, WIN_WIDTH, WIN_HEIGHT};
            SDL_RenderFillRect(renderer, &screenRect);

            // Grid parameters
            const int cols = 5, rows = 3;
            const int cellSize = 64;
            const int padding = 12;
            int gridWidth = cols * cellSize + (cols - 1) * padding;
            int gridHeight = rows * cellSize + (rows - 1) * padding;
            int startX = (WIN_WIDTH - gridWidth) / 2;
            int startY = (WIN_HEIGHT - gridHeight) / 2;

            // Load the inventory sprite once
            static SDL_Texture* invTex = nullptr;
            if (!invTex) {
                SDL_Surface* surf = SDL_LoadBMP("./sprites/Inventory.bmp");
                if (surf) {
                    invTex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_FreeSurface(surf);
                }
            }

            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    SDL_Rect dstRect = {
                        startX + col * (cellSize + padding),
                        startY + row * (cellSize + padding),
                        cellSize, cellSize
                    };
                    // Draw slot background (optional)
                    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 220);
                    SDL_RenderFillRect(renderer, &dstRect);
                    // Draw the inventory sprite
                    if (invTex) {
                        SDL_RenderCopy(renderer, invTex, nullptr, &dstRect);
                    }
                    // If a slot is occupied, draw the fish icon explicitly (only when inventory UI is visible)
                    int slotIndex = row * cols + col;
                    if (slotIndex >= 0 && slotIndex < INV_COLS * INV_ROWS && inventorySlots[slotIndex]) {
                        // Load fish texture once
                        static SDL_Texture* invFishTex = nullptr;
                        if (!invFishTex) {
                            SDL_Surface* fishSurf = SDL_LoadBMP("./sprites/fish.bmp");
                            if (fishSurf) {
                                invFishTex = SDL_CreateTextureFromSurface(renderer, fishSurf);
                                SDL_FreeSurface(fishSurf);
                            }
                        }
                        if (invFishTex) {
                            SDL_RenderCopy(renderer, invFishTex, nullptr, &dstRect);
                        }
                        // Keep internal icon position/size in sync in case other code reads it
                        UIGameObject* icon = inventorySlots[slotIndex];
                        icon->setVisible(inventoryOpen);
                        Vector2* ipos = icon->getPosition();
                        ipos->x = static_cast<float>(dstRect.x);
                        ipos->y = static_cast<float>(dstRect.y);
                        Vector2* isz = icon->getSize();
                        isz->x = static_cast<float>(dstRect.w);
                        isz->y = static_cast<float>(dstRect.h);
                    }
                }
            }
        }

        // Render navigation UI overlay if active
        if(navigationUIActive){
            // Darken the entire screen
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180); // Semi-transparent black
            SDL_Rect screenRect = {0, 0, WIN_WIDTH, WIN_HEIGHT};
            SDL_RenderFillRect(renderer, &screenRect);
            
            // Draw navigation clock sprite in center of screen
            if(navigationClockTexture){
                int texW, texH;
                SDL_QueryTexture(navigationClockTexture, nullptr, nullptr, &texW, &texH);
                
                // Scale up the clock
                float scale = 8.0f;
                int scaledW = static_cast<int>(texW * scale);
                int scaledH = static_cast<int>(texH * scale);
                
                int centerX = WIN_WIDTH / 2;
                int centerY = WIN_HEIGHT / 2;
                SDL_Rect dstRect = {
                    centerX - scaledW / 2,
                    centerY - scaledH / 2,
                    scaledW,
                    scaledH
                };
                SDL_RenderCopy(renderer, navigationClockTexture, nullptr, &dstRect);
                
                // Draw navigation indicator based on mouse position
                if(navigationIndicatorTexture){
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    
                    // Calculate angle from center to mouse
                    float dx = static_cast<float>(mouseX - centerX);
                    float dy = static_cast<float>(mouseY - centerY);
                    float angle = atan2(dy, dx);
                    
                    // Update boat's navigation direction
                    boat->setNavigationDirection(angle);
                    
                    // Assume clock radius is approximately half the scaled texture width
                    int clockRadius = scaledW / 2 - 10; // Slightly inside the edge
                    
                    // Calculate indicator position on the clock circle
                    int indicatorX = centerX + static_cast<int>(clockRadius * cos(angle));
                    int indicatorY = centerY + static_cast<int>(clockRadius * sin(angle));
                    
                    // Draw the indicator sprite
                    int indW, indH;
                    SDL_QueryTexture(navigationIndicatorTexture, nullptr, nullptr, &indW, &indH);
                    SDL_Rect indicatorRect = {
                        indicatorX - indW / 2,
                        indicatorY - indH / 2,
                        indW,
                        indH
                    };
                    SDL_RenderCopy(renderer, navigationIndicatorTexture, nullptr, &indicatorRect);
                }
            }
        }

        // Draw fishing minigame overlay if active (draw near the hook)
        if (fishingMinigameActive) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            // Dim small area behind the bar for legibility
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
            SDL_RenderFillRect(renderer, &fishingMinigameScreenRect);

            // Bar background
            SDL_Rect barBg = fishingMinigameScreenRect;
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 220);
            SDL_RenderFillRect(renderer, &barBg);

            if (fishingMinigameType == MINIGAME_TIMED_CLICK) {
                // Draw success window relative to the bar
                int winX = static_cast<int>(barBg.x + fishingMinigameWindowStart * barBg.w);
                int winW = static_cast<int>((fishingMinigameWindowEnd - fishingMinigameWindowStart) * barBg.w);
                SDL_Rect winRect = { winX, barBg.y, winW, barBg.h };
                SDL_SetRenderDrawColor(renderer, 0, 200, 0, 200);
                SDL_RenderFillRect(renderer, &winRect);

                // Draw indicator
                int indX = static_cast<int>(barBg.x + fishingMinigameIndicator * barBg.w);
                SDL_Rect indRect = { indX - 3, barBg.y - 6, 6, barBg.h + 12 };
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
                SDL_RenderFillRect(renderer, &indRect);

                // Small instruction placeholder under the bar
                SDL_Rect txt = { barBg.x, barBg.y + barBg.h + 6, barBg.w, 18 };
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
                SDL_RenderFillRect(renderer, &txt);
            } else if (fishingMinigameType == MINIGAME_TUG_OF_THE_DEEP) {
                // Draw tug progress marker
                int markX = static_cast<int>(barBg.x + tugProgress * barBg.w);
                SDL_Rect marker = { markX - 6, barBg.y - 6, 12, barBg.h + 12 };
                SDL_SetRenderDrawColor(renderer, 220, 220, 255, 220);
                SDL_RenderFillRect(renderer, &marker);

                // Draw tension overlay (red) proportional to tugTension
                if (tugTension > 0.01f) {
                    Uint8 alpha = static_cast<Uint8>(std::min(220.0f, tugTension * 220.0f));
                    SDL_SetRenderDrawColor(renderer, 200, 0, 0, alpha);
                    SDL_RenderFillRect(renderer, &barBg);
                }

                // Draw stamina boxes above the bar
                for (int si = 0; si < 3; ++si) {
                    SDL_Rect sbox = { barBg.x + si * 18, barBg.y - 22, 14, 12 };
                    if (si < tugStamina) SDL_SetRenderDrawColor(renderer, 100, 200, 100, 220);
                    else SDL_SetRenderDrawColor(renderer, 60, 60, 60, 180);
                    SDL_RenderFillRect(renderer, &sbox);
                }

                // Small instruction placeholder
                SDL_Rect txt = { barBg.x, barBg.y + barBg.h + 6, barBg.w, 18 };
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
                SDL_RenderFillRect(renderer, &txt);

                // Draw small progress text (debug)
                // (Optional) could render text; for now a debug indicator using tiny rects
                int progW = static_cast<int>(barBg.w * (1.0f - tugProgress));
                SDL_Rect prog = { barBg.x, barBg.y + barBg.h + 26, progW, 6 };
                SDL_SetRenderDrawColor(renderer, 60, 160, 220, 220);
                SDL_RenderFillRect(renderer, &prog);
            }
        }

        // Present the final frame once
        SDL_RenderPresent(renderer);
    }

    if (udpSocket) {
        SDLNet_UDP_Close(udpSocket);
        SDLNet_Quit();
    }
    
    if(navigationClockTexture){
        SDL_DestroyTexture(navigationClockTexture);
    }
    if(navigationIndicatorTexture){
        SDL_DestroyTexture(navigationIndicatorTexture);
    }
    
    // Shutdown audio
    SoundManager::instance().quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}