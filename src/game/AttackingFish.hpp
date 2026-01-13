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
    float nextFireTimer = 2.0f; // time until next shot
    int shotsRemaining = 10; // number of shots this fish can fire (can be tuned)
    uint32_t entityId = 0;
    uint32_t ownerPlayerId = 0; // player who triggered/spawned this fish
    std::mt19937 rng{static_cast<uint32_t>(SDL_GetTicks())};
    bool spriteChangedAfterFirstThrow = false;
public:
    AttackingFish(const Vector2& pos, SDL_Renderer* renderer, uint32_t entityId = 0, uint32_t ownerId = 0, int zIndex = 4)
        : ICollidable(pos, {2.0f, 2.0f}, "./sprites/AttackingFish1.bmp", renderer, true, zIndex), renderer(renderer), entityId(entityId), ownerPlayerId(ownerId)
         ,GameObject(pos, {2.0f, 2.0f}, "./sprites/AttackingFish1.bmp", renderer, zIndex)
    {
        // Randomize initial fire delay slightly
        std::uniform_real_distribution<float> dist(0.5f, 1.8f);
        nextFireTimer = dist(rng);
        // Default shots (tunable): allow the fish to fire a few times
        shotsRemaining = 3;
        // Center the fish at the given world position so visual matches spawn location
        Vector2 sz = *getSize();
        setPosition({ pos.x - sz.x / 2.0f, pos.y - sz.y / 2.0f });
    }

    void update(float dt) override {
        if (shotsRemaining <= 0) return;
        nextFireTimer -= dt;
        if (nextFireTimer <= 0.0f && shotsRemaining > 0) {
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
                SDL_Log("AttackingFish: host created projectile at (%.2f,%.2f) targetOwner=%u shotsLeft=%d", start.x, start.y, ownerPlayerId, shotsRemaining - 1);
            }

            // Change sprite on the first throw
            if (!spriteChangedAfterFirstThrow) {
                setSprite("./sprites/AttackingFish2.bmp", renderer);
                spriteChangedAfterFirstThrow = true;
            }

            // Consume a shot and schedule reload if any remain
            shotsRemaining -= 1;
            if (shotsRemaining > 0) {
                std::uniform_real_distribution<float> dist(0.8f, 2.0f);
                nextFireTimer = dist(rng);
            } else {
                // No more shots; stop firing. Could mark for deletion here if desired.
                nextFireTimer = 0.0f;
            }
        }
    }

    void onCollisionEnter(ICollidable* other) override {
        // Default behavior: don't block (or you can choose to revert position)
    }

    // Accessors for network reconciliation
    uint32_t getEntityId() const { return entityId; }
    uint32_t getOwnerPlayerId() const { return ownerPlayerId; }

    // Adopt authoritative entity/owner ids when host assigns them
    void adoptSpawn(uint32_t newEntityId, uint32_t newOwnerId) {
        entityId = newEntityId;
        ownerPlayerId = newOwnerId;
        SDL_Log("AttackingFish: adopted eid=%u owner=%u", entityId, ownerPlayerId);
    }
};