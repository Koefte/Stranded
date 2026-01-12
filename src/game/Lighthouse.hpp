#pragma once
#include "IInteractable.hpp"
#include "SDL.h"

class Lighthouse : public IInteractable {
public:
    Lighthouse(Vector2 pos, Vector2 sizeMultiplier, SDL_Renderer* renderer, int zIndex = 0)
        : IInteractable(pos, sizeMultiplier, "./sprites/lighthouse_tower.bmp", renderer, true, zIndex, {SDLK_e}),
          ICollidable(pos, sizeMultiplier, "./sprites/lighthouse_tower.bmp", renderer, true, zIndex),
          GameObject(pos, sizeMultiplier, "./sprites/lighthouse_tower.bmp", renderer, zIndex)
    {
    }

    void onInteract(SDL_Keycode key) override {
        
    }
};