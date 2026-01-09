#include <SDL.h>
#include <SDL_net.h>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_map>
#include <cstring>
#include <algorithm>

#include "Camera.hpp" 
#include "GameObject.hpp"
#include "IAnimatable.hpp"
#include "Vector2.hpp"
#include "Player.hpp"


//GAME
static std::vector<GameObject*> gameObjects;
static Camera* camera;
static Player* player;
static SDL_Renderer* g_renderer = nullptr;


// Networking
static bool isHost = false;
static UDPsocket udpSocket = nullptr;
static IPaddress hostAddr;
static std::vector<IPaddress> clientAddrs;
static uint32_t clientId = 0;
static uint32_t inputSeq = 0;
static std::unordered_map<uint32_t, Player*> remotePlayers;

#pragma pack(push, 1)
struct InputPacket {
    uint32_t clientId;
    uint32_t seq;
    uint8_t moveFlags; // bits: 0=up, 1=down, 2=left, 3=right
};

struct PlayerState {
    uint32_t id;
    float x, y;
    float vx, vy;  // velocity
    uint8_t animFrame;
};

struct SnapshotHeader {
    uint32_t tick;
    uint32_t playerCount;
};
#pragma pack(pop)




constexpr int WIN_WIDTH = 800;
constexpr int WIN_HEIGHT = 600;

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
    Player* remote = new Player({0.0f, 0.0f}, {2.0f, 2.0f}, remoteSprites, 4, g_renderer, 0.1f, 1);
    remotePlayers[id] = remote;
    gameObjects.push_back(remote); // Immediately add to render list
    std::cout << "Created remote player with ID: " << id << "\n";
    return remote;
}

void sendInputPacket() {
    if (!udpSocket || isHost) return;
    
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    uint8_t moveFlags = 0;
    if (keys[SDL_SCANCODE_W]) moveFlags |= (1 << 0);
    if (keys[SDL_SCANCODE_S]) moveFlags |= (1 << 1);
    if (keys[SDL_SCANCODE_A]) moveFlags |= (1 << 2);
    if (keys[SDL_SCANCODE_D]) moveFlags |= (1 << 3);
    
    InputPacket pkt{clientId, inputSeq++, moveFlags};
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
    Vector2* pos = player->getPosition();
    Vector2 vel = player->getVelocity();
    states.push_back({0, pos->x, pos->y, vel.x, vel.y, 0});
    
    // Add remote players
    for (auto& [id, p] : remotePlayers) {
        Vector2* rpos = p->getPosition();
        Vector2 rvel = p->getVelocity();
        states.push_back({id, rpos->x, rpos->y, rvel.x, rvel.y, 0});
    }
    
    SnapshotHeader header{tick++, static_cast<uint32_t>(states.size())};
    size_t totalSize = sizeof(header) + states.size() * sizeof(PlayerState);
    
    UDPpacket* out = SDLNet_AllocPacket(static_cast<int>(totalSize));
    std::memcpy(out->data, &header, sizeof(header));
    std::memcpy(out->data + sizeof(header), states.data(), states.size() * sizeof(PlayerState));
    out->len = static_cast<uint16_t>(totalSize);
    
    for (auto& addr : clientAddrs) {
        out->address = addr;
        SDLNet_UDP_Send(udpSocket, -1, out);
    }
    SDLNet_FreePacket(out);
}

void receiveSnapshot(SDL_Renderer* renderer) {
    if (!udpSocket || isHost) return;
    
    UDPpacket* in = SDLNet_AllocPacket(2048);
    if (SDLNet_UDP_Recv(udpSocket, in)) {
        if (in->len >= sizeof(SnapshotHeader)) {
            SnapshotHeader header;
            std::memcpy(&header, in->data, sizeof(header));
            
            size_t expected = sizeof(header) + header.playerCount * sizeof(PlayerState);
            if (in->len >= expected) {
                PlayerState* states = reinterpret_cast<PlayerState*>(in->data + sizeof(header));
                
                for (uint32_t i = 0; i < header.playerCount; ++i) {
                    if (states[i].id == clientId) {
                        // Update local player from authoritative state
                        Vector2* pos = player->getPosition();
                        pos->x = states[i].x;
                        pos->y = states[i].y;
                        player->setVelocity({states[i].vx, states[i].vy});
                    } else {
                        // Update/create remote player
                        Player* remote = getOrCreateRemotePlayer(states[i].id);
                        if (!remote) continue;
                        Vector2* rpos = remote->getPosition();
                        rpos->x = states[i].x;
                        rpos->y = states[i].y;
                        remote->setVelocity({states[i].vx, states[i].vy});
                    }
                }
            }
        }
    }
    SDLNet_FreePacket(in);
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

    bool running = true;
    SDL_Event event;

    const char* playerSpritePaths[] = {
        "./sprites/Boy_Walk1.bmp",
        "./sprites/Boy_Walk2.bmp",
        "./sprites/Boy_Walk3.bmp",
        "./sprites/Boy_Walk4.bmp"
    };

    player = new Player({0.0f, 0.0f},{2.0f,2.0f}, playerSpritePaths,4, renderer,0.1f,1);
    
    camera = new Camera({0.0f, 0.0f}, {WIN_WIDTH, WIN_HEIGHT},2.0f);
    
    camera->follow(player);
    
    gameObjects.push_back(player);
    
    // Add remote players to game objects if they exist
    for (auto& [id, remote] : remotePlayers) {
        gameObjects.push_back(remote);
    }
    
    //From tileset example
    std::vector<GameObject*> gameObjectsFromTileset = GameObject::fromTileset("./tilesets/tilemap.json","./tilesets/tilemap.bmp", renderer);
    for(GameObject* obj: gameObjectsFromTileset){
        gameObjects.push_back(obj);
    }

    // Lighthouse with multi-rectangle hitbox
    // Sprite ~32x64 scaled by 3 => ~96x192
    Vector2 lighthousePos = {600.0f, 200.0f};
    Vector2 lighthouseSizeMultiplier = {3.0f, 3.0f};

    ICollidable* lighthouse = new ICollidable(
        lighthousePos,
        lighthouseSizeMultiplier,
        "./sprites/lighthouse.bmp",
        renderer,
        true,
        1,
        10  // Smaller minClusterSize for more detailed hitboxes
    );
    gameObjects.push_back(lighthouse);
    

    ICollidable* test = new ICollidable(
        {400.0f, 300.0f}, 
        {2.0f, 2.0f}, 
        "./sprites/CollideTest.bmp", 
        renderer,
        false,
        2
    );
    gameObjects.push_back(test);

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
            else if(event.type == SDL_KEYDOWN){
                
                // Only local player handles local input
                player->onKeyDown(event.key.keysym.sym);
                
            }
            else if(event.type == SDL_KEYUP){
                player->onKeyUp(event.key.keysym.sym);
            }
        }
        
        // Network: client sends input, host receives
        if (!isHost && udpSocket) {
            sendInputPacket();
        }
        if (isHost && udpSocket) {
            receiveInputs();
        }
        
        // Sync remote players from network
        if (!isHost && udpSocket) {
            receiveSnapshot(renderer);
            // Interpolate remote players using velocity between snapshots
            for (auto& [id, remote] : remotePlayers) {
                remote->applyVelocity(static_cast<float>(dt));
            }
        }
        
        for(GameObject* obj: gameObjects){
            obj->update(static_cast<float>(dt));
            if(ICollidable* collider = dynamic_cast<ICollidable*>(obj)){
                for(GameObject* otherObj: gameObjects){
                    if(otherObj == obj) continue;
                    if(ICollidable* otherCollider = dynamic_cast<ICollidable*>(otherObj)){
                        auto shapeA = collider->getCollisionBox();
                        auto shapeB = otherCollider->getCollisionBox();
                        
                        bool isColliding = checkCollision(shapeA, shapeB);
                        
                        if(collisionPairs.find({collider, otherCollider}) != collisionPairs.end() || collisionPairs.find({otherCollider, collider}) != collisionPairs.end()){
                            if(!isColliding){
                                collider->onCollisionLeave(otherCollider);
                                otherCollider->onCollisionLeave(collider);
                                collisionPairs.erase({collider, otherCollider});
                                collisionPairs.erase({otherCollider, collider});
                            } else {
                                collider->onCollisionStay(otherCollider);
                                otherCollider->onCollisionStay(collider);
                            }
                            continue;
                        }
                        if(isColliding){
                            SDL_Log("Checking collision between objects");
                            collider->onCollisionEnter(otherCollider);
                            otherCollider->onCollisionEnter(collider);
                            collisionPairs.insert({collider, otherCollider});
                        }
                    }
                }
            }
        }
        
        // Host broadcasts snapshot
        if (isHost && udpSocket) {
            broadcastSnapshot();
        }
        
        camera->render(renderer,gameObjects);

        
    }

    if (udpSocket) {
        SDLNet_UDP_Close(udpSocket);
        SDLNet_Quit();
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}