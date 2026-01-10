#pragma once
#include <SDL.h>
#include "GameObject.hpp"
#include "ICollidable.hpp"
#include <set>

class IInteractable : virtual public ICollidable {
    private:
    std::set<SDL_Keycode> interactKeys;
    public:
    IInteractable(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath, SDL_Renderer* renderer,bool isComplex, int zIndex,std::set<SDL_Keycode> interactKeys)
        : ICollidable(pos, sizeMultiplier, spritePath, renderer, isComplex, zIndex), interactKeys(interactKeys)
        , GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex)
    {
    }
    virtual void onInteract(SDL_Keycode key){
     // To be implemented in subclasses
    }
    std::set<SDL_Keycode> getInteractKeys(){
        return interactKeys;
    }
};