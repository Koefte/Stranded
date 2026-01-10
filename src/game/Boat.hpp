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
          navigationDirection({1.0f, 0.0f}),  // Default direction: east
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
    
    void boardBoat(Player* player) {
        if (player && player->getParent() != this) {
            // Store player's current world position
            Vector2 worldPos = player->getWorldPosition();
            
            // Store boat's world position before adding child
            Vector2 boatWorld = getWorldPosition();
            
            // Add player as child
            addChild(player);
            
            // Calculate local position (world position - boat's world position)
            player->getPosition()->x = worldPos.x - boatWorld.x;
            player->getPosition()->y = worldPos.y - boatWorld.y;
            
            SDL_Log("Player boarded the boat");
        }
    }
    
    void leaveBoat(Player* player) {
        if (player && player->getParent() == this) {
            // Store player's current world position
            Vector2 worldPos = player->getWorldPosition();
            
            // Remove player as child
            removeChild(player);
            
            // Set to absolute world position
            player->getPosition()->x = worldPos.x;
            player->getPosition()->y = worldPos.y;
            
            SDL_Log("Player left the boat");
        }
    }
    
    bool isPlayerOnBoard(Player* player) const {
        return player && player->getParent() == this;
    }
    
    Vector2 getNavigationDirection() const {
        return navigationDirection;
    }
    
    bool getIsMoving() const {
        return isMoving;
    }
    
    void setBoatState(float x, float y, float rot, float navDirX, float navDirY, bool moving) {
        getPosition()->x = x;
        getPosition()->y = y;
        setRotation(rot);
        navigationDirection.x = navDirX;
        navigationDirection.y = navDirY;
        if (moving && !isMoving) {
            isMoving = true;
            startAnimation();
        } else if (!moving && isMoving) {
            isMoving = false;
            stopAnimation();
        }
    }
    
    void onInteract(SDL_Keycode key) override {
        // Only respond to F, E, and B keys
        if(key != SDLK_f && key != SDLK_e && key != SDLK_b){
            return;
        }
        
        SDL_Log("Boat interacted with key: %d", key);
        if(key == SDLK_f){
            *navigationUIActive = !(*navigationUIActive);
            SDL_Log("Navigation UI %s", *navigationUIActive ? "opened" : "closed");
        }
        else if(key == SDLK_e){
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
        else if(key == SDLK_b){
            // Board/leave is handled externally in main.cpp
            // This is just for key recognition
        }
    }
};