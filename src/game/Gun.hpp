#pragma once

#include "GameObject.hpp"
#include "Projectile.hpp"
#include <SDL.h>
#include <algorithm>

class Gun : public GameObject {
private:
    Projectile* projectile = nullptr;
    SDL_Renderer* renderer = nullptr;
    // Cooldown between shots (seconds)
    float shotCooldown = 0.5f;
    float shotTimer = 0.0f; // time remaining until next shot allowed
public:
    Gun(const Vector2& pos, const Vector2& sizeMultiplier, const char* spritePath, SDL_Renderer* renderer, int zIndex = 0)
        : GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex), renderer(renderer) {
        // Create projectile but keep it invisible; it is not a child of the gun (projectile is world-space)
        projectile = new Projectile({0.0f,0.0f}, {1.0f,1.0f}, "./sprites/projectile.bmp", renderer, zIndex+1);
        projectile->setVisible(false);
    }

    ~Gun() {
        if (projectile) delete projectile;
    }

    Projectile* getProjectile() const { return projectile; }

    void setProjectile(Projectile* newProjectile) {
        projectile = newProjectile;
    }

    // Fire from the gun's tip toward world target. Returns true if a shot was fired.
    bool fireAt(const Vector2& worldTarget) {
        if (!projectile) return false;
        if (shotTimer > 0.0f) return false; // still cooling down
        Vector2 start = getWorldPosition();
        // Adjust start position slightly in front of the gun if desired
        projectile->fire(start, worldTarget);
        shotTimer = shotCooldown;
        return true;
    }

    // Update cooldown timer
    void update(float dt) override {
        if (shotTimer > 0.0f) {
            shotTimer -= dt;
            if (shotTimer < 0.0f) shotTimer = 0.0f;
        }
    }

    // Optional setters
    void setCooldown(float seconds) { shotCooldown = std::max(0.0f, seconds); }
    float getCooldown() const { return shotCooldown; }
    float getTimeUntilReady() const { return shotTimer; }

    // Keep projectile owned but ensure it's updated by global loop (projectile should be added to gameObjects elsewhere when used)
};
