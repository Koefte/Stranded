#pragma once
#include <SDL.h>
#include "GameObject.hpp"
#include <random>
#include <cmath>

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
    // Meander parameters
    float phase = 0.0f;
    float freq = 1.0f;
    float amp = 4.0f;

    Particle(Vector2 pos,Vector2 endPos, float lifetime, SDL_Renderer* renderer, SDL_Color color, int zIndex)
        : GameObject(pos, {1.0f, 1.0f}, createParticleTexture(renderer, color), renderer, zIndex), pos(pos), endPos(endPos), color(color) ,startPos(pos), lifetime(lifetime) {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> phaseDist(0.0f, 2.0f * 3.14159265f);
        std::uniform_real_distribution<float> freqDist(1.0f, 3.0f);
        std::uniform_real_distribution<float> ampDist(2.0f, 12.0f);
        phase = phaseDist(rng);
        freq = freqDist(rng);
        amp = ampDist(rng);
    }

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
            return;
        }

        // Base linear interpolation along the path
        Vector2 base = startPos * (1.0f - t) + endPos * t;

        // Compute perpendicular meander offset that decays as particle approaches target
        Vector2 dir = { endPos.x - startPos.x, endPos.y - startPos.y };
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        Vector2 perp = {0.0f, 0.0f};
        if (len > 0.0001f) {
            perp.x = -dir.y / len;
            perp.y = dir.x / len;
        }
        float wobble = std::sin(age * freq + phase) * amp * (1.0f - t);
        pos = { base.x + perp.x * wobble, base.y + perp.y * wobble };
        setPosition(pos);
        alive = true;
    }
};