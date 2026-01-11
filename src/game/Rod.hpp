#pragma once

#include <SDL.h>
#include "Vector2.hpp"
#include "GameObject.hpp"

class Rod : public GameObject {
public:
    Rod(Vector2 localPos, Vector2 sizeMultiplier, const char* spritePath, SDL_Renderer* renderer, int zIndex = 1)
        : GameObject(localPos, sizeMultiplier, spritePath, renderer, zIndex)
    {
        setVisible(false); // Initially hidden
    }
};
