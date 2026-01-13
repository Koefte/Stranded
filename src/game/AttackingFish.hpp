#pragma once
#pragma once

#include "ICollidable.hpp"
#include "FishProjectile.hpp"
#include "Vector2.hpp"
#include <SDL.h>
#include <algorithm>
#include <random>

// AttackingFish: spawns, waits a short randomized delay, then throws a FishProjectile
// that chases the player. After throwing, the fish switches to "AttackingFish2.bmp".
class AttackingFish : public ICollidable {
private:
    SDL_Renderer* renderer = nullptr;
    float throwTimer = 1.0f;
    bool hasThrown = false;
public:
    AttackingFish(const Vector2& pos, SDL_Renderer* renderer, int zIndex = 4)
        : ICollidable(pos, {2.0f, 2.0f}, "./sprites/AttackingFish1.bmp", renderer, false, zIndex), renderer(renderer) ,
          GameObject(pos, {2.0f, 2.0f}, "./sprites/AttackingFish1.bmp", renderer, zIndex)
    {
        // Randomize initial throw delay slightly
        std::mt19937 rng(static_cast<uint32_t>(SDL_GetTicks()));
        std::uniform_real_distribution<float> dist(0.5f, 1.8f);
        throwTimer = dist(rng);
    }

    void update(float dt) override {
        if (hasThrown) return;
        throwTimer -= dt;
        if (throwTimer <= 0.0f) {
            // Fire a chasing fish projectile at the global player if present
            extern Player* player; // declared in main.cpp
            if (player) {
                FishProjectile* fp = new FishProjectile(getWorldPosition(), {1.0f,1.0f}, "./sprites/FishProjectile.bmp", renderer, 4);
                fp->fire(getWorldPosition(), player);
                // Ensure projectile participates in world updates and collision
                extern std::vector<GameObject*> gameObjects;
                if (std::find(gameObjects.begin(), gameObjects.end(), fp) == gameObjects.end()) {
                    gameObjects.push_back(fp);
                }
                SDL_Log("AttackingFish: fired projectile at player (pos %.2f,%.2f)", getWorldPosition().x, getWorldPosition().y);
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