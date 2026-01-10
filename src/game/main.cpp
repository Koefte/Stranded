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

#include "Camera.hpp" 
#include "GameObject.hpp"
#include "IAnimatable.hpp"
#include "Vector2.hpp"
#include "Player.hpp"
#include "Rectangle.hpp"
#include "Boat.hpp"
#include "DebugObject.hpp"


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
static std::set<SDL_Keycode> pressedInteractKeys;


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

#pragma pack(push, 1)
struct ChunkPacket {
    uint32_t magic; // 'CHNK'
    int32_t cx;
    int32_t cy;
    uint32_t seed;
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
    LAYER_DEBUG = 4
};

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
    gameObjects.push_back(remote); // Immediately add to render list
    std::cout << "Created remote player with ID: " << id << "\n";
    return remote;
}

// Forward declarations
float hitBoxDistance(std::vector<Rectangle> shapeA, std::vector<Rectangle> shapeB);

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
    
    InputPacket pkt{clientId, inputSeq++, moveFlags, boardBoat, toggleBoatMovement, hasBoatControl, toggleHook, navDir.x, navDir.y};
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
            }
        }
    }
    SDLNet_FreePacket(in);
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
    uint8_t hooking = player->isHooking() ? 1 : 0;
    states.push_back({0, pos.x, pos.y, vel.x, vel.y, 0, onBoat, hooking});
    
    // Add remote players
    for (auto& [id, p] : remotePlayers) {
        Vector2 rpos = p->getWorldPosition();
        Vector2 rvel = p->getVelocity();
        uint8_t rOnBoat = boat->isPlayerOnBoard(p) ? 1 : 0;
        uint8_t rHooking = p->isHooking() ? 1 : 0;
        states.push_back({id, rpos.x, rpos.y, rvel.x, rvel.y, 0, rOnBoat, rHooking});
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

    SDL_Window* window = SDL_CreateWindow(
        "Fish Game  ",
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
                auto env = GameObject::generateInitialEnvironment(rend, "./tilesets/tilemap.json", "./tilesets/World_GenAtlas.bmp", area, seed);
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
    if (player->getFishingHook()) {
        gameObjects.push_back(player->getFishingHook());
    }
    for (auto& [id, remote] : remotePlayers) {
        if (remote->getFishingHook()) {
            gameObjects.push_back(remote->getFishingHook());
        }
    }
    

    Uint64 prev = SDL_GetPerformanceCounter();
    double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    std::set<std::pair<ICollidable*, ICollidable*>> collisionPairs; // TODO: Make this work

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (now - prev) / freq; // seconds since last frame
        prev = now;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                // Handle mouse click for fishing hook casting
                if (!navigationUIActive) {
                    player->onMouseDown(event.button.button, event.button.x, event.button.y, 
                                       camera->getPosition(), camera->getZoom());
                }
            }
            else if(event.type == SDL_KEYDOWN){
                
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
                // Check for chunk packet first
                if (in->len >= sizeof(ChunkPacket)) {
                    ChunkPacket cp;
                    std::memcpy(&cp, in->data, sizeof(ChunkPacket));
                    if (cp.magic == 0x4B4E4843) { // 'CHNK'
                        std::pair<int,int> key{cp.cx, cp.cy};
                        if (generatedChunks.find(key) == generatedChunks.end()) {
                            Rectangle area{{cp.cx * static_cast<float>(CHUNK_SIZE_PX), cp.cy * static_cast<float>(CHUNK_SIZE_PX)},
                                           {(cp.cx+1) * static_cast<float>(CHUNK_SIZE_PX), (cp.cy+1) * static_cast<float>(CHUNK_SIZE_PX)}};
                            auto env = GameObject::generateInitialEnvironment(renderer, "./tilesets/tilemap.json", "./tilesets/World_GenAtlas.bmp", area, cp.seed);
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
                                player->setHooking(states[i].isHooking != 0);
                            } else {
                                Player* remote = getOrCreateRemotePlayer(states[i].id);
                                if (!remote) continue;
                                
                                // Handle remote player boarding state
                                bool wasOnBoat = boat->isPlayerOnBoard(remote);
                                bool shouldBeOnBoat = states[i].isOnBoat != 0;
                                
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
                                remote->setHooking(states[i].isHooking != 0);
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

        // Skip game updates when navigation UI is active
        if(!navigationUIActive){
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
        
        camera->render(renderer,gameObjects);

        // Render fishing lines after game objects but before UI
        if (player->getFishingHook()) {
            player->getFishingHook()->renderLine(renderer, camera->getPosition(), camera->getZoom());
        }
        for (auto& [id, remote] : remotePlayers) {
            if (remote->getFishingHook()) {
                remote->getFishingHook()->renderLine(renderer, camera->getPosition(), camera->getZoom());
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
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}