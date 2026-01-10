#pragma once
#include <SDL.h>
#include "GameObject.hpp"
#include "ICollidable.hpp"
#include "IAnimatable.hpp"
#include "IInteractable.hpp"
#include "DebugObject.hpp"
#include "Player.hpp"

class Boat : public IAnimatable,public IInteractable {
    private:
    bool* navigationUIActive;
    Vector2 navigationDirection;
    float boatSpeed;
    bool isMoving;
    float lastDeltaTime;
    public:
    Boat(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath[], int frameCount, SDL_Renderer* renderer,float animationStep, int zIndex,std::set<SDL_Keycode> interactKeys, bool* navUIPtr )
        : GameObject(pos, sizeMultiplier, spritePath[0], renderer, zIndex),
          IAnimatable(pos, sizeMultiplier, spritePath, frameCount, renderer, animationStep, zIndex),
          ICollidable(pos,sizeMultiplier,spritePath[0],renderer,true,zIndex),
          IInteractable(pos,sizeMultiplier,spritePath[0],renderer,true,zIndex,interactKeys),
          navigationUIActive(navUIPtr),
          navigationDirection({0.0f, 0.0f}),
          boatSpeed(50.0f),
          isMoving(false),
          lastDeltaTime(0.0f)
    {
        stopAnimation();
    }
    void setNavigationDirection(float angle) {
        navigationDirection.x = cos(angle);
        navigationDirection.y = sin(angle);
    }
    
    void update(float dt) override {
        IAnimatable::update(dt);
        lastDeltaTime = dt;
        
        if(isMoving){
            changePosition(navigationDirection.x * boatSpeed * dt, navigationDirection.y * boatSpeed * dt);
        }
    }
    
    void onCollisionStay(ICollidable* other) override {
        // If boat is moving and colliding with player, move the player too
        if(isMoving){
            Player* playerObj = dynamic_cast<Player*>(other);
            if(playerObj){
                float dx = navigationDirection.x * boatSpeed * lastDeltaTime;
                float dy = navigationDirection.y * boatSpeed * lastDeltaTime;
                playerObj->changePosition(dx, dy);
                // Update player's prevPosition so their collision response doesn't revert this
                playerObj->updatePrevPosition();
            }
        }
    }
    
    void onInteract(SDL_Keycode key) override {
        // Only respond to F and E keys
        if(key != SDLK_f && key != SDLK_e){
            return;
        }
        
        SDL_Log("Boat interacted with key: %d", key);
        if(key == SDLK_f){
            *navigationUIActive = !(*navigationUIActive);
            SDL_Log("Navigation UI %s", *navigationUIActive ? "opened" : "closed");
        }
        if(key == SDLK_e){
          if(isMoving){
              SDL_Log("Boat stopping");
              isMoving = false;
              stopAnimation();
          } else {
              SDL_Log("Boat starting to move in direction (%.2f, %.2f)", navigationDirection.x, navigationDirection.y);
              isMoving = true;
              startAnimation();
          }
        }
    }
};