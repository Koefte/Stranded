#include <SDL.h>
#include <iostream>
#include <vector>
#include <set>

#include "Camera.hpp" 
#include "GameObject.hpp"
#include "IAnimatable.hpp"
#include "Vector2.hpp"
#include "Player.hpp"



static std::vector<GameObject*> gameObjects;
static Camera* camera;
static Player* player;

constexpr int WIN_WIDTH = 800;
constexpr int WIN_HEIGHT = 600;

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SDL2 Starter",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIN_WIDTH,
        WIN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    player = new Player({0.0f, 0.0f},{2.0f,2.0f}, "./sprites/Sprite.bmp", renderer, 1);
    
    camera = new Camera({0.0f, 0.0f}, {WIN_WIDTH, WIN_HEIGHT},1.0f);
    
    camera->follow(player);
    
    gameObjects.push_back(player);
    //From tileset example
    std::vector<GameObject*> gameObjectsFromTileset = GameObject::fromTileset("./tilesets/tilemap.json","./tilesets/tilemap.bmp", renderer);
    for(GameObject* obj: gameObjectsFromTileset){
        gameObjects.push_back(obj);
    }

    // Make animated object
    const char* animFrames[] = {
        "./sprites/1.bmp",
        "./sprites/2.bmp",
        "./sprites/3.bmp"
    };


    // Animation example
    IAnimatable* animatedObj = new IAnimatable({300.0f, 300.0f}, {2.0f,2.0f}, animFrames, 3, renderer, 0.2f, 1);
    gameObjects.push_back(animatedObj);

    // Collision example object
    ICollidable* collidableObj = new ICollidable({400.0f, 400.0f}, {2.0f,2.0f}, "./sprites/CollideTest.bmp", renderer, 1);
    gameObjects.push_back(collidableObj);

    Uint64 prev = SDL_GetPerformanceCounter();
    double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    std::set<std::pair<ICollidable*, ICollidable*>> collisionPairs;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (now - prev) / freq; // seconds since last frame
        prev = now;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            else if(event.type == SDL_KEYDOWN){
                for(GameObject* obj: gameObjects){
                    obj->onKeyDown(event.key.keysym.sym);
                }
            }
            else if(event.type == SDL_KEYUP){
                for(GameObject* obj: gameObjects){
                    obj->onKeyUp(event.key.keysym.sym);
                }
            }
            
        }
        for(GameObject* obj: gameObjects){
            obj->update(static_cast<float>(dt));
            if(ICollidable* collider = dynamic_cast<ICollidable*>(obj)){
                for(GameObject* otherObj: gameObjects){
                    if(otherObj == obj) continue;
                    if(ICollidable* otherCollider = dynamic_cast<ICollidable*>(otherObj)){
                        Rectangle boxA = collider->getCollisionBox();
                        Rectangle boxB = otherCollider->getCollisionBox();
                        if(collisionPairs.find({collider, otherCollider}) != collisionPairs.end() || collisionPairs.find({otherCollider, collider}) != collisionPairs.end()){
                            if(!boxA.intersects(boxB)){
                                collider->onCollisionLeave(otherCollider);
                                otherCollider->onCollisionLeave(collider);
                                collisionPairs.erase({collider, otherCollider});
                                collisionPairs.erase({otherCollider, collider});
                            } else {
                                collider->onCollisionStay(otherCollider);
                                otherCollider->onCollisionStay(collider);
                            }
                            continue;
                        }
                        if(boxA.intersects(boxB)){
                            SDL_Log("Checking collision between objects");
                            collider->onCollisionEnter(otherCollider);
                            otherCollider->onCollisionEnter(collider);
                            collisionPairs.insert({collider, otherCollider});
                        }
                    }
                }
            }
        }
        camera->render(renderer,gameObjects);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
