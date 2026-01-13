#pragma once

#include <SDL.h>
#include <vector>

#include "Vector2.hpp"
#include "GameObject.hpp"
#include <algorithm>
#include "UIGameObject.hpp"
#include "Player.hpp"
#include "Lighthouse.hpp"
#include <unordered_map>

// Externs from main.cpp used to draw healthbars and scene lighting
extern Player* player;
extern std::unordered_map<uint32_t, Player*> remotePlayers;
extern float g_sunIntensity;

// Lighthouse glow configuration (tweakable at runtime)
extern float g_lighthouseGlowBaseRadius;      // base radius (pixels) when it's bright
extern float g_lighthouseGlowExtraRadius;     // extra radius added when fully dark
extern float g_lighthouseGlowIntensityMultiplier; // multiplier applied to alpha/intensity
extern float g_dayTimeSeconds; // for subtle pulsing of lights


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
        
        // Sky color modulated by sun intensity (0.0 = night, 1.0 = day)
        SDL_SetRenderDrawColor(renderer,
            static_cast<Uint8>(100.0f * g_sunIntensity),
            static_cast<Uint8>(160.0f * g_sunIntensity),
            static_cast<Uint8>(255.0f * g_sunIntensity),
            255);
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

        // Draw lighthouse glow at night (radial gradient, additive)
        if (g_sunIntensity < 0.95f) {
            static SDL_Texture* glowTex = nullptr;
            static const int BASE_TEX_SIZE = 256; // base texture size (square)
            if (!glowTex) {
                // Create a square RGBA surface with radial alpha gradient (red)
                SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, BASE_TEX_SIZE, BASE_TEX_SIZE, 32, SDL_PIXELFORMAT_RGBA32);
                if (surf) {
                    int cx = BASE_TEX_SIZE / 2;
                    int cy = BASE_TEX_SIZE / 2;
                    float invRadius = 1.0f / static_cast<float>(BASE_TEX_SIZE / 2);
                    for (int y = 0; y < BASE_TEX_SIZE; ++y) {
                        Uint32* row = reinterpret_cast<Uint32*>((Uint8*)surf->pixels + y * surf->pitch);
                        for (int x = 0; x < BASE_TEX_SIZE; ++x) {
                            float dx = static_cast<float>(x - cx);
                            float dy = static_cast<float>(y - cy);
                            float d = std::sqrt(dx*dx + dy*dy) * invRadius; // 0..~1.414
                            float t = std::clamp(1.0f - d, 0.0f, 1.0f);
                            // Softer falloff for a stronger, tighter center
                            float alphaF = std::pow(t, 1.2f);
                            alphaF = std::min(1.0f, alphaF * 1.15f); // boost center a bit
                            Uint8 a = static_cast<Uint8>(alphaF * 255.0f);
                            Uint32 pixel = SDL_MapRGBA(surf->format, 255, 80, 80, a);
                            row[x] = pixel;
                        }
                    }
                    glowTex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_FreeSurface(surf);
                    if (glowTex) SDL_SetTextureBlendMode(glowTex, SDL_BLENDMODE_ADD);
                }
            }

            if (glowTex) {
                // Draw the glow for each lighthouse; size and alpha scale with darkness
                float darkness = (1.0f - g_sunIntensity); // 0..1
                // Increase intensity (more opaque at night) using configurable multiplier
                Uint8 baseAlpha = static_cast<Uint8>(std::clamp(darkness * 255.0f * g_lighthouseGlowIntensityMultiplier, 0.0f, 255.0f));
                // Subtle pulse to make it feel alive (low amplitude)
                float pulse = 0.95f + 0.05f * std::sin(g_dayTimeSeconds * 2.0f);

                for (GameObject* obj : gameObjects) {
                    Lighthouse* lh = dynamic_cast<Lighthouse*>(obj);
                    if (!lh) continue;
                    Vector2 worldPos = lh->getWorldPosition();
                    Vector2* sz = lh->getSize();
                    float topX = worldPos.x + sz->x * 0.5f;
                    float topY = worldPos.y + sz->y * 0.12f;
                    int sx = static_cast<int>((topX - position.x) * zoomLevel);
                    int sy = static_cast<int>((topY - position.y) * zoomLevel);

                        // Radius computed from configurable base and extra radius values
                    float radiusPx = (g_lighthouseGlowBaseRadius + g_lighthouseGlowExtraRadius * darkness) * zoomLevel * pulse;
                    int dstW = static_cast<int>(radiusPx * 2.0f);
                    int dstH = dstW;
                    SDL_Rect dst = { sx - dstW/2, sy - dstH/2, dstW, dstH };

                    SDL_SetTextureAlphaMod(glowTex, baseAlpha);
                    SDL_RenderCopy(renderer, glowTex, nullptr, &dst);
                }
            }
        }

        // Draw player health bars on top of the scene
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        auto drawHealthBar = [&](Player* p) {
            if (!p) return;
            Vector2 center = p->getCenteredPosition();
            int barW = static_cast<int>(40 * zoomLevel);
            int barH = static_cast<int>(6 * zoomLevel);
            int sx = static_cast<int>((center.x - position.x) * zoomLevel) - barW / 2;
            // Raise healthbar a bit above the player
            int sy = static_cast<int>((center.y - position.y) * zoomLevel) - static_cast<int>(18 * zoomLevel) - barH;
            float pct = (p->getMaxHp() > 0.0f) ? (p->getHp() / p->getMaxHp()) : 0.0f;
            // Background
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_Rect bg{ sx, sy, barW, barH };
            SDL_RenderFillRect(renderer, &bg);
            // Foreground (health)
            SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
            SDL_Rect fg{ sx + 1, sy + 1, std::max(0, static_cast<int>((barW - 2) * pct)), std::max(0, barH - 2) };
            SDL_RenderFillRect(renderer, &fg);
        };

        if (player) drawHealthBar(player);
        for (auto& kv : remotePlayers) {
            if (kv.second) drawHealthBar(kv.second);
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