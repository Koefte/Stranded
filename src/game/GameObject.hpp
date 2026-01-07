#pragma once

#include <SDL.h>
#include "Vector2.hpp"


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
        this->size = {static_cast<float>(surface->w) * sizeMultiplier.x, static_cast<float>(surface->h) * sizeMultiplier.y};
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