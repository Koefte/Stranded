#pragma once

#include "GameObject.hpp"
#include <SDL.h>

// Simple projectile (harpoon shot) which moves toward a target and despawns on impact or timeout
class Projectile : public GameObject {
private:
    Vector2 velocity{0.0f, 0.0f};
    Vector2 target{0.0f, 0.0f};
    bool hasTarget = false;
    float speed = 360.0f; // pixels per second (reduced for better feel)
    float life = 3.0f; // seconds
    bool active = false;
public:
    Projectile(const Vector2& pos, const Vector2& sizeMultiplier, const char* spritePath, SDL_Renderer* renderer, int zIndex = 0)
        : GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex) {}

    // Fire towards an explicit world target; projectile will stop at target
    void fire(const Vector2& start, const Vector2& targ) {
        Vector2* p = getPosition();
        p->x = start.x; p->y = start.y;
        target = targ;
        hasTarget = true;
        Vector2 dir = { target.x - start.x, target.y - start.y };
        float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
        if (len > 0.001f) {
            dir.x /= len; dir.y /= len;
        } else {
            dir.x = 0.0f; dir.y = 0.0f;
        }
        velocity.x = dir.x * speed;
        velocity.y = dir.y * speed;
        active = true;
        setVisible(true);
        life = 3.0f;
    }

    bool isActive() const { return active; }

    Vector2 getTargetPos() const { return target; }

    void setState(const Vector2& pos, const Vector2& targ, bool act) {
        Vector2* p = getPosition();
        p->x = pos.x; p->y = pos.y;
        target = targ;
        hasTarget = true;
        Vector2 dir = { target.x - p->x, target.y - p->y };
        float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
        if (len > 0.001f) { dir.x /= len; dir.y /= len; }
        velocity.x = dir.x * speed;
        velocity.y = dir.y * speed;
        active = act;
        setVisible(act);
        life = 3.0f;
    }

    void update(float dt) override {
        if (!active) return;
        Vector2* p = getPosition();
        // Move by velocity but clamp to target; stop when reaching target
        float step = speed * dt;
        float dx = target.x - p->x;
        float dy = target.y - p->y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (hasTarget && dist <= step) {
            // Arrived (or would overshoot) -> snap to target and deactivate
            p->x = target.x;
            p->y = target.y;
            active = false;
            hasTarget = false;
            setVisible(false);
            return;
        }
        // Normal move
        p->x += velocity.x * dt;
        p->y += velocity.y * dt;
        life -= dt;
        if (life <= 0.0f) {
            active = false;
            hasTarget = false;
            setVisible(false);
        }
    }
};
