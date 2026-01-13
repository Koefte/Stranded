#pragma once

#include "ICollidable.hpp"
#include "FishProjectile.hpp"
#include "Vector2.hpp"
#include <SDL.h>
#include <SDL_net.h>
#include <algorithm>
#include <random>

// AttackingFish: spawns, waits a short randomized delay, then throws a FishProjectile
// that chases the player. After throwing, the fish switches to "AttackingFish2.bmp".
// Forward declarations for networking helpers in main.cpp
extern bool isHost;
extern UDPsocket udpSocket;
extern uint32_t getPlayerId(Player* p);
extern void hostBroadcastFishProjectile(uint32_t projectileId, uint32_t ownerEntityId, uint32_t targetPlayerId, float startX, float startY);
extern Player* getOrCreateRemotePlayer(uint32_t id);
extern uint32_t clientId;

class AttackingFish : public ICollidable {
private:
    SDL_Renderer* renderer = nullptr;
    float throwTimer = 1.0f;
    bool hasThrown = false;
    uint32_t entityId = 0;
    uint32_t ownerPlayerId = 0; // player who triggered/spawned this fish
public:
    AttackingFish(const Vector2& pos, SDL_Renderer* renderer, uint32_t entityId = 0, uint32_t ownerId = 0, int zIndex = 4)
        : ICollidable(pos, {2.0f, 2.0f}, "./sprites/AttackingFish1.bmp", renderer, true, zIndex), renderer(renderer), entityId(entityId), ownerPlayerId(ownerId)
         ,GameObject(pos, {2.0f, 2.0f}, "./sprites/AttackingFish1.bmp", renderer, zIndex)
    {
        // Randomize initial throw delay slightly
        std::mt19937 rng(static_cast<uint32_t>(SDL_GetTicks()));
        std::uniform_real_distribution<float> dist(0.5f, 1.8f);
        throwTimer = dist(rng);
        // Center the fish at the given world position so visual matches spawn location
        Vector2 sz = *getSize();
        setPosition({ pos.x - sz.x / 2.0f, pos.y - sz.y / 2.0f });
    }

    void update(float dt) override {
        if (hasThrown) return;
        throwTimer -= dt;
        if (throwTimer <= 0.0f) {
            // Fire a chasing fish projectile at the global player if present
            extern Player* player; // declared in main.cpp
        
            // Start projectile from the fish center (mouth)
            Vector2 start = { getWorldPosition().x + getSize()->x / 2.0f, getWorldPosition().y + getSize()->y / 2.0f };
            // Determine target player by ownerPlayerId
            Player* targetPlayer = nullptr;
            if (ownerPlayerId == clientId) targetPlayer = player;
            else targetPlayer = getOrCreateRemotePlayer(ownerPlayerId);

            // Only host (or single-player where udpSocket==NULL) should create authoritative projectile and broadcast it
            extern std::vector<GameObject*> gameObjects;
            if (isHost || udpSocket == nullptr) {
                FishProjectile* fp = new FishProjectile(start, {1.0f,1.0f}, "./sprites/FishProjectile.bmp", renderer, 4);
                fp->fire(start, targetPlayer ? targetPlayer : player);
                if (std::find(gameObjects.begin(), gameObjects.end(), fp) == gameObjects.end()) {
                    gameObjects.push_back(fp);
                }
                // If host, broadcast spawn to clients
                if (isHost && udpSocket) {
                    extern uint32_t nextProjectileId;
                    uint32_t pid = nextProjectileId++;
                    uint32_t targetPid = getPlayerId(targetPlayer ? targetPlayer : player);
                    hostBroadcastFishProjectile(pid, entityId, targetPid, start.x, start.y);
                }
                SDL_Log("AttackingFish: host created projectile at (%.2f,%.2f) targetOwner=%u", start.x, start.y, ownerPlayerId);
            }       
            // Change sprite to indicate it has thrown
            setSprite("./sprites/AttackingFish2.bmp", renderer);
            hasThrown = true;
        }
    }

    void onCollisionEnter(ICollidable* other) override {
        // Default behavior: don't block (or you can choose to revert position)
    }
};