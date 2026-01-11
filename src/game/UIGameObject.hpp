#pragma once
#include "GameObject.hpp"
#include <SDL.h>

// UIGameObject: Drawn in screen space, not affected by camera
class UIGameObject : public GameObject {
public:
    UIGameObject(Vector2 screenPos, Vector2 size, const char* spritePath, SDL_Renderer* renderer, int zIndex = 0)
        : GameObject(screenPos, size, spritePath, renderer, zIndex) {}

   
};
