#pragma once

#include <SDL.h>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Vector2.hpp"

using json = nlohmann::json;

class GameObject{
    private:
    Vector2 position;
    Vector2 sizeMultiplier;
    Vector2 size;
    float rotation;
    SDL_Texture* sprite;
    int zIndex;
    public:
    GameObject(Vector2 pos,const char* spritePath, SDL_Renderer* renderer,int zIndex = 0)
        : GameObject(pos, {1, 1}, spritePath, renderer, zIndex)
    {
    }

    GameObject(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath, SDL_Renderer* renderer,Vector2 cutoutBegin,Vector2 cutoutEnd,int zIndex = 0){
        // Initialize position, sizeMultiplier, rotation
        this->position = pos;
        this->sizeMultiplier = sizeMultiplier;
        this->rotation = 0.0f;
        // Load sprite texture from file
        SDL_Surface* surface = SDL_LoadBMP(spritePath);
        if (!surface) {
            sprite = nullptr;
            this->size = {0.0f, 0.0f};
            this->zIndex = zIndex;
            return;
        }
        // Cutout the surface
        SDL_Rect cutoutRect;
        cutoutRect.x = static_cast<int>(cutoutBegin.x);
        cutoutRect.y = static_cast<int>(cutoutBegin.y);
        cutoutRect.w = static_cast<int>(cutoutEnd.x - cutoutBegin.x);
        cutoutRect.h = static_cast<int>(cutoutEnd.y - cutoutBegin.y);
        SDL_Surface* cutoutSurface = SDL_CreateRGBSurface(0, cutoutRect.w, cutoutRect.h, 32, 0, 0, 0, 0);
        if (cutoutSurface) {
            SDL_BlitSurface(surface, &cutoutRect, cutoutSurface, nullptr);
            sprite = SDL_CreateTextureFromSurface(renderer, cutoutSurface);
            this->size = {static_cast<float>(cutoutSurface->w) * sizeMultiplier.x, static_cast<float>(cutoutSurface->h) * sizeMultiplier.y};
            SDL_FreeSurface(cutoutSurface);
        } else {
            sprite = nullptr;
            this->size = {0.0f, 0.0f};
        }
        SDL_FreeSurface(surface);
        this->zIndex = zIndex;
    }

    GameObject(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath, SDL_Renderer* renderer,int zIndex = 0){
        this->position = pos;
        this->sizeMultiplier = sizeMultiplier;
        this->rotation = 0.0f;
        // Load sprite texture from file
        SDL_Surface* surface = SDL_LoadBMP(spritePath);
        if (surface) {
            sprite = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        } else {
            sprite = nullptr;
        }
        if (surface) {
            this->size = {static_cast<float>(surface->w) * sizeMultiplier.x, static_cast<float>(surface->h) * sizeMultiplier.y};
        } else {
            this->size = {0.0f, 0.0f};
        }
        this->zIndex = zIndex;
    }

    SDL_Texture* getSprite(){
        return sprite;
    }

    virtual void update(){
        // To be implemented in subclasses
    }

    virtual void onKeyDown(SDL_Keycode key){
        // To be implemented in subclasses
    }

    virtual void onKeyUp(SDL_Keycode key){
        // To be implemented in subclasses
    }

    static std::vector<GameObject*> fromTileset(const char* tilemapPath, const char* tilesetPath, SDL_Renderer* renderer){
        std::vector<GameObject*> tiles;
        std::ifstream file(tilemapPath);
        if (!file.is_open()) {
            SDL_Log("Failed to open tilemap file: %s", tilemapPath);
            return tiles;
        }

        json data;
        try {
            file >> data;
        } catch (const std::exception& e) {
            SDL_Log("Failed to parse tilemap JSON: %s", e.what());
            return tiles;
        }

        // Extract metadata
        int tileWidth = data["tileWidth"].get<int>();
        int tileHeight = data["tileHeight"].get<int>();
        int mapWidth = data["mapWidth"].get<int>();
        int mapHeight = data["mapHeight"].get<int>();

        // Load tiles
        const auto& tilesArray = data["tiles"];
        for (int y = 0; y < mapHeight; y++) {
            const auto& row = tilesArray[y];
            for (int x = 0; x < mapWidth; x++) {
                const auto& tile = row[x];
                
                if (tile.is_null()) continue;

                int tileX = tile["x"].get<int>();
                int tileY = tile["y"].get<int>();

                // Create a tileset sprite with the correct tile coordinates
                Vector2 pos = {
                    static_cast<float>(x * tileWidth),
                    static_cast<float>(y * tileHeight)
                };

                Vector2 cutoutBegin = {
                    static_cast<float>(tileX * tileWidth),
                    static_cast<float>(tileY * tileHeight)
                };
                Vector2 cutoutEnd = {
                    static_cast<float>((tileX + 1) * tileWidth),
                    static_cast<float>((tileY + 1) * tileHeight)
                };



                GameObject* gameObj = new GameObject(pos, Vector2{1,1}, tilesetPath, renderer, cutoutBegin, cutoutEnd, 0);
                tiles.push_back(gameObj);
            }
        }
        return tiles;
    }


    int getZIndex(){
        return zIndex;
    }

    Vector2* getPosition() {
        return &position;
    }

    Vector2* getSize() {
        return &size;
    }





};