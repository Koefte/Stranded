#include <SDL.h>
#include <iostream>
#include <vector>

#include "Camera.hpp" 
#include "GameObject.hpp"
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

    player = new Player({0.0f, 0.0f},{4.0f,4.0f}, "./sprites/Sprite.bmp", renderer, 1);
    
    camera = new Camera({0.0f, 0.0f}, {WIN_WIDTH, WIN_HEIGHT});
    
    camera->follow(player);
    
    gameObjects.push_back(player);
    gameObjects.push_back(new GameObject({0.0f, 0.0f}, "./sprites/tree.bmp", renderer));
    std::vector<GameObject*> gameObjectsFromTileset = GameObject::fromTileset("./tilesets/tilemap.json","./tilesets/tilemap.bmp", renderer);
    for(GameObject* obj: gameObjectsFromTileset){
        gameObjects.push_back(obj);
    }
    while (running) {
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
            obj->update();
        }
        camera->render(renderer,gameObjects);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
