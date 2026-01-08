#pragma once

#include <SDL.h>
#include "Vector2.hpp"
#include "GameObject.hpp"
#include "ICollidable.hpp"

class Player : public ICollidable {
private:
    float speed = 5.0f;
    Vector2 prevPosition;  // Track previous frame position
    bool moveUp = false, moveDown = false, moveLeft = false, moveRight = false;

public:
    using ICollidable::ICollidable;

    void onKeyDown(SDL_Keycode key) override {
        switch (key) {
            case SDLK_w: moveUp = true; break;
            case SDLK_s: moveDown = true; break;
            case SDLK_a: moveLeft = true; break;
            case SDLK_d: moveRight = true; break;
            default: break;
        }
    }

    void onKeyUp(SDL_Keycode key) {
        switch (key) {
            case SDLK_w: moveUp = false; break;
            case SDLK_s: moveDown = false; break;
            case SDLK_a: moveLeft = false; break;
            case SDLK_d: moveRight = false; break;
            default: break;
        }
    }

    void update(float dt) override {
        Vector2* pos = getPosition();
        prevPosition = *pos;  // Save before movement
        
        if (moveUp) pos->y -= speed;
        if (moveDown) pos->y += speed;
        if (moveLeft) pos->x -= speed;
        if (moveRight) pos->x += speed;
    }

    void onCollisionEnter(ICollidable* other) override {
        Vector2* pos = getPosition();
        *pos = prevPosition;  // Revert to last valid position
        SDL_Log("Player collided!");
    }
};