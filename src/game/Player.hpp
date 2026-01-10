#pragma once

#include <SDL.h>
#include <cmath>
#include "Vector2.hpp"
#include "GameObject.hpp"
#include "IAnimatable.hpp"
#include "ICollidable.hpp"
#include "IInteractable.hpp"

class Player : public IAnimatable, public ICollidable {
private:
    float speed = 200.0f;  // pixels per second
    Vector2 prevPosition;  // Track previous frame position
    Vector2 velocity = {0.0f, 0.0f};  // Current velocity
    bool moveUp = false, moveDown = false, moveLeft = false, moveRight = false;

public:
   

    // Construct from individual frame paths
    Player(
        Vector2 pos,
        Vector2 sizeMultiplier,
        const char* spritePaths[],
        int frameCount,
        SDL_Renderer* renderer,
        float animationStep,
        int zIndex = 0)
        : GameObject(pos, sizeMultiplier, spritePaths[0], renderer, zIndex),
          IAnimatable(pos, sizeMultiplier, spritePaths, frameCount, renderer, animationStep, zIndex),
          ICollidable(pos, sizeMultiplier, spritePaths[0], renderer, false, zIndex)
    {}

    void onKeyDown(SDL_Keycode key) override {
        switch (key) {
            case SDLK_w: moveUp = true; break;
            case SDLK_s: moveDown = true; break;
            case SDLK_a: moveLeft = true; break;
            case SDLK_d: moveRight = true; break;
            default: break;
        }
    }

    void onKeyUp(SDL_Keycode key) override {
        switch (key) {
            case SDLK_w: moveUp = false; break;
            case SDLK_s: moveDown = false; break;
            case SDLK_a: moveLeft = false; break;
            case SDLK_d: moveRight = false; break;
            default: break;
        }
    }

    void update(float dt) override {
        IAnimatable::update(dt);  // Call animation update
        Vector2* pos = getPosition();
        prevPosition = *pos;  // Save before movement

        float dx = 0.0f;
        float dy = 0.0f;

        if (moveUp) dy -= 1.0f;
        if (moveDown) dy += 1.0f;
        if (moveLeft) dx -= 1.0f;
        if (moveRight) dx += 1.0f;

        if (dx != 0.0f || dy != 0.0f) {
            const float length = std::sqrt((dx * dx) + (dy * dy));
            dx = (dx / length) * speed;
            dy = (dy / length) * speed;

            velocity.x = dx;
            velocity.y = dy;
            pos->x += dx * dt;
            pos->y += dy * dt;
            startAnimation();
        } else {
            velocity.x = 0.0f;
            velocity.y = 0.0f;
            stopAnimation();
        }
    }

    Vector2 getVelocity() const {
        return velocity;
    }

    void setVelocity(Vector2 vel) {
        velocity = vel;
    }

    void applyVelocity(float dt) {
        Vector2* pos = getPosition();
        pos->x += velocity.x * dt;
        pos->y += velocity.y * dt;
    }

    // Method to move player externally (e.g., by boat)
    void moveExternally(float dx, float dy) {
        changePosition(dx, dy);
        // Update prevPosition so collision response doesn't revert this movement
        Vector2* pos = getPosition();
        prevPosition = *pos;
    }

    void updatePrevPosition() {
        Vector2* pos = getPosition();
        prevPosition = *pos;
    }

    void onCollisionEnter(ICollidable* other) override {
        Vector2* pos = getPosition();
        *pos = prevPosition;  // Revert to last valid position
        SDL_Log("Player collided!");
    }

    void onCollisionStay(ICollidable* other) override {
        // Optional: Handle continuous collision
        Vector2* pos = getPosition();
        *pos = prevPosition;  // Revert to last valid position
        
    }

    
};