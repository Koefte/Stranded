#pragma once

#include "ICollidable.hpp"
#include "Vector2.hpp"
#include <SDL.h>
#include <cmath>
#include "Player.hpp"

// Forward declaration of onHurt implemented in main.cpp
extern void onHurt(Player* p);

class FishProjectile : public ICollidable {
private:
    Vector2 velocity{0.0f,0.0f};
    // Reduce speed so the projectile is less punishing
    float speed = 110.0f;
    float life = 3.0f; // self-destruct after 3s
    bool active = false;
    Player* target = nullptr;
    SDL_Renderer* renderer = nullptr;
public:
    FishProjectile(const Vector2& pos, const Vector2& sizeMultiplier, const char* spritePath, SDL_Renderer* renderer, int zIndex = 4)
        : ICollidable(pos, sizeMultiplier, spritePath, renderer, false, zIndex), renderer(renderer),
         GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex)
    {
        setVisible(false);
    }

    void fire(const Vector2& start, Player* targetPlayer) {
        Vector2* p = getPosition();
        p->x = start.x; p->y = start.y;
        target = targetPlayer;
        active = true;
        setVisible(true);
        life = 3.0f;
    }

    bool getIsActive() const { return active; }

    void update(float dt) override {
        if (!active) return;
        Vector2* p = getPosition();
        if (target) {
            Vector2 tp = target->getCenteredPosition();
            Vector2 dir = { tp.x - p->x, tp.y - p->y };
            float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
            if (len > 0.001f) { dir.x /= len; dir.y /= len; }
            velocity.x = dir.x * speed;
            velocity.y = dir.y * speed;
        }
        p->x += velocity.x * dt;
        p->y += velocity.y * dt;
        life -= dt;
        if (life <= 0.0f) {
            active = false;
            setVisible(false);
            // Mark for deletion; main loop will remove and delete safely
            markForDeletion();
        }
    }

    void onCollisionEnter(ICollidable* other) override {
        // If we hit a player, call the hurt handler and mark for deletion
        Player* p = dynamic_cast<Player*>(other);
        if (p) {
            onHurt(p);
            active = false;
            setVisible(false);
            markForDeletion();
        }
    }
};