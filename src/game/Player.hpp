#pragma once

#include <SDL.h>
#include "Vector2.hpp"
#include "GameObject.hpp"

class Player : public GameObject {
private:
    const float speed = 5.0f;
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;

public:
    using GameObject::GameObject;

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

    void update() override {
        Vector2* pos = getPosition();
        if (moveUp) pos->y -= speed;
        if (moveDown) pos->y += speed;
        if (moveLeft) pos->x -= speed;
        if (moveRight) pos->x += speed;
    }
};