#pragma once
#include "GameObject.hpp"

class DebugObject : public GameObject {
    public:
    DebugObject(Vector2 pos,SDL_Renderer* renderer) : GameObject(pos, {1.0f, 1.0f}, "./sprites/debug.bmp", renderer, 1000) {
    }
};

