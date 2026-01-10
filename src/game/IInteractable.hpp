#pragma once
#include <SDL.h>
#include "GameObject.hpp"
#include "ICollidable.hpp"

class IInteractable : virtual public ICollidable {
    private:
    SDL_Keycode interactKey;
    public:
    IInteractable(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath, SDL_Renderer* renderer,bool isComplex, int zIndex,SDL_Keycode interactKey)
        : ICollidable(pos, sizeMultiplier, spritePath, renderer, isComplex, zIndex), interactKey(interactKey)
        , GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex)
    {
    }
    virtual void onInteract(){
     // To be implemented in subclasses
    }
    SDL_Keycode getInteractKey(){
        return interactKey;
    }
};