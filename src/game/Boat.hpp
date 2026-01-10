#pragma once
#include <SDL.h>
#include "GameObject.hpp"
#include "ICollidable.hpp"
#include "IAnimatable.hpp"
#include "IInteractable.hpp"
#include "DebugObject.hpp"

class Boat : public IAnimatable,public IInteractable {
    public:
    Boat(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath[], int frameCount, SDL_Renderer* renderer,float animationStep, int zIndex,SDL_Keycode interactKey )
        : GameObject(pos, sizeMultiplier, spritePath[0], renderer, zIndex),
          IAnimatable(pos, sizeMultiplier, spritePath, frameCount, renderer, animationStep, zIndex),
          ICollidable(pos,sizeMultiplier,spritePath[0],renderer,true,zIndex),
          IInteractable(pos,sizeMultiplier,spritePath[0],renderer,true,zIndex,interactKey)
    {
    }
    void onInteract() override {
        SDL_Log("Boat interacted with!");
    }
};