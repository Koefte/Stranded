#pragma once
#include "GameObject.hpp"
#include "Rectangle.hpp"

class ICollidable : virtual public GameObject {
    public:
    ICollidable(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath, SDL_Renderer* renderer,int zIndex = 0)
        : GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex)
    {
    }
    
    Rectangle getCollisionBox(){
        return Rectangle{{
            this->getPosition()->x,
            this->getPosition()->y,
        },{
            this->getPosition()->x + this->getSize()->x,
            this->getPosition()->y + this->getSize()->y
        }};
    } 
    
    virtual void onCollisionEnter(ICollidable* other){
        // To be implemented in subclasses
    }

    virtual void onCollisionLeave(ICollidable* other){
        // To be implemented in subclasses
    }

    virtual void onCollisionStay(ICollidable* other){
        // To be implemented in subclasses
    }
    
};