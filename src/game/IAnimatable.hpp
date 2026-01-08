#pragma once

#include <map>
#include "GameObject.hpp"

class IAnimatable : public GameObject{
    private:
    std::map<int, SDL_Texture*> animationFrames;
    int currentFrame;
    float animationStep;
    float elapsed;
    public:
    IAnimatable(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath[], int frameCount, SDL_Renderer* renderer,float animationStep, int zIndex = 0)
        : GameObject(pos, sizeMultiplier, spritePath[0], renderer, zIndex)
    {
        for(int i = 0; i < frameCount; i++){
            SDL_Surface* surface = SDL_LoadBMP(spritePath[i]);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                animationFrames[i] = texture;
                SDL_FreeSurface(surface);
            } else {
                animationFrames[i] = nullptr;
            }
        }
        currentFrame = 0;
        this->animationStep = animationStep;
        this->elapsed = 0.0f;
    }


    void update(float dt) override {
        if (animationFrames.empty()) return;

        elapsed += dt;
        while (elapsed >= animationStep) {
            elapsed -= animationStep;
            currentFrame = (currentFrame + 1) % static_cast<int>(animationFrames.size());
        }

        SDL_Texture* currentTexture = animationFrames[currentFrame];
        if (currentTexture) {
            this->setSprite(currentTexture);
        }
    }
};