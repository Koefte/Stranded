#pragma once
#include <vector>
#include <random>
#include <SDL.h>
#include "Vector2.hpp"

#include "Particle.hpp"

class ParticleSystem  {

public:
    ParticleSystem(SDL_Renderer* renderer) : rng(std::random_device{}()), renderer(renderer) {}
    
    void emit(const Vector2& start, const Vector2& end, int count, SDL_Color color,float duration, int zIndex,float spread = 10.0f) {
        std::uniform_real_distribution<float> noise(-spread, spread);
        for (int i = 0; i < count; ++i) {
            // Apply small variation to start position (noise) instead of to the end position
            Vector2 noisyStart = start + Vector2{noise(rng), noise(rng)};
            particles.emplace_back(noisyStart, end, duration, renderer, color, zIndex);
        }
    }

    void update(float dt) {
        for (auto& p : particles) {
            p.update(dt);
        }
    }

    void render(SDL_Renderer* renderer, const Vector2& camPos, float zoom) {
        for ( auto& p : particles) {
            if (!p.alive) continue;
            SDL_Texture* tex = p.getSprite();
            if (!tex) continue;
            Vector2 worldPos = p.pos; // Particle stores world position
            Vector2* sz = p.getSize();
            SDL_Rect dstRect = {
                static_cast<int>((worldPos.x - camPos.x) * zoom),
                static_cast<int>((worldPos.y - camPos.y) * zoom),
                static_cast<int>(sz->x * zoom),
                static_cast<int>(sz->y * zoom)
            };
            SDL_Point center = { static_cast<int>(sz->x * zoom / 2), static_cast<int>(sz->y * zoom / 2) };
            SDL_RenderCopyEx(renderer, tex, nullptr, &dstRect, p.getRotation(), &center, SDL_FLIP_NONE);
        }
    }

    std::vector<Particle>& getParticles() {
        return particles;
    }

   
   

private:
    std::vector<Particle> particles;
    std::mt19937 rng;
    SDL_Renderer* renderer;
};