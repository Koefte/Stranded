#pragma once
#include <SDL.h>
#include "GameObject.hpp"

class Particle : public GameObject {
public:
    Vector2 pos;
    Vector2 vel;
    Vector2 startPos;
    Vector2 endPos;
    float t = 0.0f; // 0=start, 1=end
    float lifetime = 1.0f; // seconds
    float age = 0.0f;
    SDL_Color color{255,255,255,255};
    float size = 4.0f;
    bool alive = true;

    Particle(Vector2 pos,Vector2 endPos, float lifetime, SDL_Renderer* renderer, SDL_Color color, int zIndex)
        : GameObject(pos, {1.0f, 1.0f}, createParticleTexture(renderer, color), renderer, zIndex), pos(pos), endPos(endPos), color(color) ,startPos(pos), lifetime(lifetime) {}

    static SDL_Texture* createParticleTexture(SDL_Renderer* renderer, SDL_Color color) {
        const int PARTICLE_SIZE = 4;
        SDL_Surface* surface = SDL_CreateRGBSurface(0, PARTICLE_SIZE, PARTICLE_SIZE, 32, 0, 0, 0, 0);
        if (!surface) return nullptr;
        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a));
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return texture;
    }

    void update(float dt) override {
        age += dt;
        t = age / lifetime;
        if (t >= 1.0f) {
            t = 1.0f;
            pos = endPos;
            setPosition(pos);
            alive = false;
        } else {
            pos = startPos * (1.0f - t) + endPos * t;
            setPosition(pos);
            alive = true;
        }
    }
};