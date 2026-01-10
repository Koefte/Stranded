#pragma once

#include <SDL.h>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Vector2.hpp"
#include "Rectangle.hpp"

using json = nlohmann::json;

class GameObject{
    private:
    Vector2 position; // Local position relative to parent
    Vector2 sizeMultiplier;
    Vector2 size;
    float rotation;
    SDL_Texture* sprite;
    int zIndex;
    GameObject* parent;
    std::vector<GameObject*> children;
    
    inline static bool envCacheInit = false;
    inline static int envTileW = 0;
    inline static int envTileH = 0;
    inline static SDL_Texture* envTextures[4] = {nullptr, nullptr, nullptr, nullptr};

    static bool initEnvironmentTiles(SDL_Renderer* renderer, const char* tilemapPath, const char* tilesetPath) {
        if (envCacheInit) return true;

        std::ifstream file(tilemapPath);
        if (!file.is_open()) {
            SDL_Log("Failed to open tilemap file: %s", tilemapPath);
            return false;
        }

        json data;
        try {
            file >> data;
        } catch (const std::exception& e) {
            SDL_Log("Failed to parse tilemap JSON: %s", e.what());
            return false;
        }

        envTileW = data["tileWidth"].get<int>();
        envTileH = data["tileHeight"].get<int>();
        int mapWidth = data["mapWidth"].get<int>();
        int mapHeight = data["mapHeight"].get<int>();

        SDL_Surface* tilesetSurface = SDL_LoadBMP(tilesetPath);
        if (!tilesetSurface) {
            SDL_Log("Failed to load tileset: %s", tilesetPath);
            return false;
        }

        int i = 0;
        const auto& tilesArray = data["tiles"];
        for (int y = 0; y < mapHeight && i < 4; y++) {
            const auto& row = tilesArray[y];
            for (int x = 0; x < mapWidth && i < 4; x++) {
                const auto& tile = row[x];
                if (tile.is_null()) continue;

                int tileX = tile["x"].get<int>();
                int tileY = tile["y"].get<int>();

                SDL_Rect cutoutRect;
                cutoutRect.x = tileX * envTileW;
                cutoutRect.y = tileY * envTileH;
                cutoutRect.w = envTileW;
                cutoutRect.h = envTileH;

                SDL_Surface* cutoutSurface = SDL_CreateRGBSurface(0, cutoutRect.w, cutoutRect.h, 32, 0, 0, 0, 0);
                if (!cutoutSurface) continue;
                SDL_BlitSurface(tilesetSurface, &cutoutRect, cutoutSurface, nullptr);
                envTextures[i] = SDL_CreateTextureFromSurface(renderer, cutoutSurface);
                SDL_FreeSurface(cutoutSurface);
                if (envTextures[i]) i++;
            }
        }

        SDL_FreeSurface(tilesetSurface);
        envCacheInit = (i == 4);
        if (!envCacheInit) {
            SDL_Log("Failed to initialize environment textures (found %d)", i);
        }
        return envCacheInit;
    }

    public:
    GameObject(Vector2 pos,const char* spritePath, SDL_Renderer* renderer,int zIndex = 0)
        : GameObject(pos, {1, 1}, spritePath, renderer, zIndex)
    {
    }

    GameObject(Vector2 pos, Vector2 sizeMultiplier, SDL_Texture* texture, SDL_Renderer* renderer, int zIndex = 0){
        this->position = pos;
        this->sizeMultiplier = sizeMultiplier;
        this->rotation = 0.0f;
        this->sprite = texture;
        this->zIndex = zIndex;
        this->parent = nullptr;
        
        // Query texture size
        if (texture) {
            int w, h;
            SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
            this->size = {static_cast<float>(w) * sizeMultiplier.x, static_cast<float>(h) * sizeMultiplier.y};
        } else {
            this->size = {0.0f, 0.0f};
        }
    }

    GameObject(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath, SDL_Renderer* renderer,Vector2 cutoutBegin,Vector2 cutoutEnd,int zIndex = 0){
        // Initialize position, sizeMultiplier, rotation
        this->position = pos;
        this->sizeMultiplier = sizeMultiplier;
        this->rotation = 0.0f;
        this->parent = nullptr;
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
        this->parent = nullptr;
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

    void setSprite(SDL_Texture* newSprite){
        this->sprite = newSprite;
    }

    virtual void update(float dt){
        // To be implemented in subclasses
    }

    virtual void onKeyDown(SDL_Keycode key){
        // To be implemented in subclasses
    }

    virtual void onKeyUp(SDL_Keycode key){
        // To be implemented in subclasses
    }

    static std::vector<GameObject*> fromTileset(const char* tilemapPath, const char* tilesetPath, SDL_Renderer* renderer) {
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
                
                SDL_Surface* surface = SDL_LoadBMP(tilesetPath);
                SDL_Rect cutoutRect;
                cutoutRect.x = static_cast<int>(cutoutBegin.x);
                cutoutRect.y = static_cast<int>(cutoutBegin.y);
                cutoutRect.w = static_cast<int>(cutoutEnd.x - cutoutBegin.x);
                cutoutRect.h = static_cast<int>(cutoutEnd.y - cutoutBegin.y);
                SDL_Surface* cutoutSurface = SDL_CreateRGBSurface(0, cutoutRect.w, cutoutRect.h, 32, 0, 0, 0, 0);

                if (cutoutSurface) {
                    SDL_BlitSurface(surface, &cutoutRect, cutoutSurface, nullptr);
                    SDL_FreeSurface(surface);
                } 

                GameObject* tileObj = new GameObject(
                    pos,
                    {1.0f, 1.0f},
                    tilesetPath,
                    renderer,
                    cutoutBegin,
                    cutoutEnd,
                    0
                );
                tiles.push_back(tileObj);

            }
        }
        return tiles;
    }

    static std::vector<GameObject*> generateInitialEnvironment(SDL_Renderer* renderer,const char* tilemapPath,const char* tilesetPath,Rectangle area, uint32_t seed = 0) {
        std::vector<GameObject*> environment;
        if (!initEnvironmentTiles(renderer, tilemapPath, tilesetPath)) {
            return environment;
        }

        uint32_t prng = seed;
        for (int y = static_cast<int>(area.begin.y); y < static_cast<int>(area.end.y); y += envTileH) {
            for (int x = static_cast<int>(area.begin.x); x < static_cast<int>(area.end.x); x += envTileW) {
                int randIndex = 0;
                if (seed != 0) {
                    prng = 1664525u * prng + 1013904223u; // LCG
                    randIndex = (prng >> 16) & 3;
                } else {
                    randIndex = rand() % 4;
                }
                SDL_Texture* texture = envTextures[randIndex];
                if (texture) {
                    GameObject* tileObj = new GameObject(
                        {static_cast<float>(x), static_cast<float>(y)},
                        {1.0f, 1.0f},
                        texture,
                        renderer,
                        0
                    );
                    environment.push_back(tileObj);
                }
            }
        }
        return environment;
    }


    int getZIndex(){
        return zIndex;
    }

    Vector2* getPosition() {
        return &position;
    }

    Vector2 getWorldPosition() const {
        if (parent) {
            Vector2 parentWorld = parent->getWorldPosition();
            return {parentWorld.x + position.x, parentWorld.y + position.y};
        }
        return position;
    }

    void changePosition(float dx, float dy) {
        position.x += dx;
        position.y += dy;
    }

    Vector2 getCenteredPosition() {
        Vector2 worldPos = getWorldPosition();
        return {worldPos.x + size.x / 2.0f, worldPos.y + size.y / 2.0f};
    }

    Vector2* getSize() {
        return &size;
    }

    void rotate(float angle) {
        rotation += angle;
    }

    float getRotation() const {
        return rotation;
    }

    void setRotation(float angle) {
        rotation = angle;
    }

    void addChild(GameObject* child) {
        if (child && child->parent != this) {
            // Remove from previous parent if any
            if (child->parent) {
                child->parent->removeChild(child);
            }
            children.push_back(child);
            child->parent = this;
        }
    }

    void removeChild(GameObject* child) {
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end()) {
            children.erase(it);
            child->parent = nullptr;
        }
    }

    GameObject* getParent() const {
        return parent;
    }

    const std::vector<GameObject*>& getChildren() const {
        return children;
    }

    void setParent(GameObject* newParent) {
        if (newParent) {
            newParent->addChild(this);
        } else if (parent) {
            parent->removeChild(this);
        }
    }

};