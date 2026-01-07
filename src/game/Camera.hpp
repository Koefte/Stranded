#pragma once

#include <SDL.h>
#include <vector>

#include "Vector2.hpp"
#include "GameObject.hpp"
#include <algorithm>


class Camera{
    private:
        Vector2 position;
        Vector2 displaySize;
        GameObject* toFollow;


    std::vector<GameObject*> sortByZIndex(std::vector<GameObject*> objs){
        std::vector<GameObject*> sorted = objs;
        std::sort(sorted.begin(), sorted.end(), [](GameObject* a, GameObject* b) {
            return a->getZIndex() < b->getZIndex();
        });
        return sorted;
    }

    public:
    Camera(Vector2 pos,Vector2 displaySize){
        this->position = pos;
        this->displaySize = displaySize;
    }
    void render(SDL_Renderer* renderer,std::vector<GameObject*> gameObjects){
        // Example rendering logic for the camera
        if(toFollow){
            Vector2* followPos = toFollow->getPosition();
            position.x = followPos->x + toFollow->getSize()->x / 2 - displaySize.x / 2;
            position.y = followPos->y + toFollow->getSize()->y / 2 - displaySize.y / 2;
        }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue background
        SDL_RenderClear(renderer);

        gameObjects = sortByZIndex(gameObjects);

        for(GameObject* obj: gameObjects){
            Vector2* objPos = obj->getPosition();
            Vector2* objSize = obj->getSize();
            SDL_Rect destRect = {
                static_cast<int>(objPos->x - position.x),
                static_cast<int>(objPos->y - position.y),
                static_cast<int>(objSize->x),
                static_cast<int>(objSize->y)
            };
            // Render the object's sprite
            // Assuming GameObject has a method getSprite() that returns SDL_Texture*
            SDL_RenderCopy(renderer, obj->getSprite(), nullptr, &destRect);
        }

        // Additional rendering logic can be added here
        SDL_RenderPresent(renderer);
    }

    void follow(GameObject* obj){
        this->toFollow = obj;
    }

};