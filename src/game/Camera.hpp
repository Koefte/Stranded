#pragma once

#include <SDL.h>
#include <vector>

#include "Vector2.hpp"
#include "GameObject.hpp"
#include <algorithm>
#include "UIGameObject.hpp"


class Camera{
    private:
        Vector2 position;
        Vector2 displaySize;
        GameObject* toFollow;
        float zoomLevel;


    std::vector<GameObject*> sortByZIndex(std::vector<GameObject*> objs){
        std::vector<GameObject*> sorted = objs;
        std::sort(sorted.begin(), sorted.end(), [](GameObject* a, GameObject* b) {
            return a->getZIndex() < b->getZIndex();
        });
        return sorted;
    }

    public:
    Camera(Vector2 pos,Vector2 displaySize,float zoomLevel){
        this->position = pos;
        this->displaySize = displaySize;
        this->zoomLevel = zoomLevel;
    }
    void renderObject(SDL_Renderer* renderer, GameObject* obj) {
        if(!obj->getVisible()) return;
        
        Vector2 worldPos = obj->getWorldPosition();
        Vector2* objSize = obj->getSize();
        
        // Apply zoom to position and size
        SDL_Rect destRect = {
            static_cast<int>((worldPos.x - position.x) * zoomLevel),
            static_cast<int>((worldPos.y - position.y) * zoomLevel),
            static_cast<int>(objSize->x * zoomLevel),
            static_cast<int>(objSize->y * zoomLevel)
        };
        
        // Calculate rotation center (center of the object)
        SDL_Point center = {
            static_cast<int>(objSize->x * zoomLevel / 2),
            static_cast<int>(objSize->y * zoomLevel / 2)
        };
        
        // Render the object's sprite with rotation
        SDL_RenderCopyEx(renderer, obj->getSprite(), nullptr, &destRect, 
                        obj->getRotation(), &center, SDL_FLIP_NONE);
        
        // Render children
        for (GameObject* child : obj->getChildren()) {
            renderObject(renderer, child);
        }
    }

    void render(SDL_Renderer* renderer,std::vector<GameObject*> gameObjects){
        // Example rendering logic for the camera
        if(toFollow){
            Vector2 followWorldPos = toFollow->getWorldPosition();
            // Adjust display size by zoom level to center correctly
            float effectiveDisplayWidth = displaySize.x / zoomLevel;
            float effectiveDisplayHeight = displaySize.y / zoomLevel;
            position.x = followWorldPos.x + toFollow->getSize()->x / 2 - effectiveDisplayWidth / 2;
            position.y = followWorldPos.y + toFollow->getSize()->y / 2 - effectiveDisplayHeight / 2;
        }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue background
        SDL_RenderClear(renderer);

        gameObjects = sortByZIndex(gameObjects);

        for(GameObject* obj: gameObjects){
            if(dynamic_cast<UIGameObject*>(obj)) {
                    // Draw UI GameObjects without camera transformation
                    Vector2 worldPos = obj->getWorldPosition();
            Vector2* objSize = obj->getSize();
            
            // Apply zoom to position and size
            SDL_Rect destRect = {
                static_cast<int>(worldPos.x),
                static_cast<int>(worldPos.y),
                static_cast<int>(objSize->x),
                static_cast<int>(objSize->y)
            };
            
            // Calculate rotation center (center of the object)
            SDL_Point center = {
                static_cast<int>(objSize->x / 2),
                static_cast<int>(objSize->y / 2)
            };
            
            // Render the object's sprite with rotation
            SDL_RenderCopyEx(renderer, obj->getSprite(), nullptr, &destRect, 
                            obj->getRotation(), &center, SDL_FLIP_NONE);
            
            }else{
                renderObject(renderer, obj);
            }
        }

        // Additional rendering logic can be added here
        // Note: SDL_RenderPresent is called at the end of the main game loop
    }

    void follow(GameObject* obj){
        this->toFollow = obj;
    }

    // Zoom control methods
    void setZoom(float zoom){
        zoomLevel = std::max(0.1f, std::min(zoom, 10.0f)); // Clamp between 0.1 and 10
    }

    void zoomIn(float amount = 0.1f){
        setZoom(zoomLevel + amount);
    }

    void zoomOut(float amount = 0.1f){
        setZoom(zoomLevel - amount);
    }

    Vector2 getPosition() const {
        return position;
    }

    Vector2 getViewSize() const {
        return displaySize;
    }


    float getZoom() const {
        return zoomLevel;
    }

};