#pragma once

#include "GameObject.hpp"
#include "Projectile.hpp"
#include <SDL.h>

class Gun : public GameObject {
private:
    Projectile* projectile = nullptr;
    SDL_Renderer* renderer = nullptr;
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

    // Fire from the gun's tip toward world target
    void fireAt(const Vector2& worldTarget) {
        if (!projectile) return;
        Vector2 start = getWorldPosition();
        // Adjust start position slightly in front of the gun if desired
        projectile->fire(start, worldTarget);
    }

    // Keep projectile owned but ensure it's updated by global loop (projectile should be added to gameObjects elsewhere when used)
};
