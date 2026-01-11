#pragma once

#include <SDL.h>
#include "Vector2.hpp"
#include "GameObject.hpp"
#include "ParticleSystem.hpp"
#include <random>

class FishingHook : public GameObject {
private:
    Vector2 velocity;
    Vector2 lineOrigin; // Position where the line connects to the rod
    Vector2 targetPos; // Where the hook should stop (mouse pos)
    float gravity = 300.0f;
    bool isActive = false;
    bool destReached = false;
    ParticleSystem* attractParticles = nullptr;
    bool attractPending = false;
    float attractTimer = 0.0f;
    std::mt19937 rng{std::random_device{}()};

public:
    FishingHook(Vector2 pos, Vector2 sizeMultiplier, const char* spritePath, SDL_Renderer* renderer, int zIndex = 2)
        : GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex),
          velocity({0.0f, 0.0f}),
          lineOrigin({0.0f, 0.0f}),
          attractParticles(new ParticleSystem(renderer))
    {
        setVisible(false);

    }

    Vector2 getTargetPos() const {
        return targetPos;
    }

    Vector2* getTargetPosPtr() {
        return &targetPos;
    }

    // Ensure all positions are world coordinates
    // When casting, set position and targetPos in world coordinates
    void cast(Vector2 startPos, Vector2 direction, Vector2 mousePos, float castSpeed = 200.0f) {
        Vector2* pos = getPosition();
        pos->x = startPos.x;
        pos->y = startPos.y;
        // Store the origin point for line rendering (world coordinates)
        lineOrigin = startPos;
        // Set the target position (world coordinates)
        targetPos = mousePos;
        // Normalize direction and apply speed
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length > 0.0f) {
            velocity.x = (direction.x / length) * castSpeed;
            velocity.y = (direction.y / length) * castSpeed;
        }
        isActive = true;
        setVisible(true);
        // Schedule attract particles to spawn after a random delay
        // Make delays always longer than the previous range (0.5-2.5s)
        std::uniform_real_distribution<float> delayDist(2.6f, 5.0f);
        attractTimer = delayDist(rng);
        attractPending = true;
    }

    void update(float dt) override {
        if (!isActive) return;

        // Apply gravity
        velocity.y += gravity * dt;

        // Update position
        Vector2* pos = getPosition();
        pos->x += velocity.x * dt;
        pos->y += velocity.y * dt;

        // Check if hook has reached or passed the target position (mouse)
        float distToTarget = std::sqrt((pos->x - targetPos.x) * (pos->x - targetPos.x) + (pos->y - targetPos.y) * (pos->y - targetPos.y));
        if (distToTarget < 8.0f ||
            ((velocity.x > 0 && pos->x >= targetPos.x) || (velocity.x < 0 && pos->x <= targetPos.x)) &&
            ((velocity.y > 0 && pos->y >= targetPos.y) || (velocity.y < 0 && pos->y <= targetPos.y))) {
            // Snap to target and stop
            pos->x = targetPos.x;
            pos->y = targetPos.y;
            destReached = true;
            velocity = {0.0f, 0.0f};
        }

        // (removed immediate arrival burst; only delayed attract particles are used)

        // Handle attract particles spawn after delay
        if (attractPending) {
            attractTimer -= dt;
            if (attractTimer <= 0.0f) {
                // Spawn particles at a random distance from the hook and move them toward the hook
                Vector2 hookPos = getWorldPosition();
                std::uniform_real_distribution<float> radiusDist(40.0f, 140.0f);
                std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
                float radius = radiusDist(rng);
                float angle = angleDist(rng);
                Vector2 startCenter = { hookPos.x + std::cos(angle) * radius, hookPos.y + std::sin(angle) * radius };
                // Emit particles that move from startCenter toward the current hook position
                if (attractParticles) {
                    // Slower, fewer, and slightly tighter-spread attract particles
                    attractParticles->emit(startCenter, hookPos, 10, SDL_Color{0, 255, 0, 255}, 4.5f, 4, 12.0f);
                }
                attractPending = false;
            }
        }
        if (attractParticles) attractParticles->update(dt);
    }

    void retract() {
        isActive = false;
        destReached = false;
        if (attractParticles) attractParticles->getParticles().clear();
        setVisible(false);
        velocity = {0.0f, 0.0f};
    }

    bool getIsActive() const {
        return isActive;
    }

    Vector2 getLineOrigin() const {
        return lineOrigin;
    }

    void updateLineOrigin(Vector2 newOrigin) {
        lineOrigin = newOrigin;
    }

    void renderLine(SDL_Renderer* renderer, const Vector2& cameraOffset, float cameraZoom) {
        if (!isActive) return;

        Vector2 hookPos = getWorldPosition();
        Vector2* hookSize = getSize();
        Vector2 hookCenter = {
            hookPos.x + hookSize->x / 2.0f,
            hookPos.y + hookSize->y / 2.0f
        };

        // Transform to screen coordinates
        int x1 = static_cast<int>((lineOrigin.x - cameraOffset.x) * cameraZoom);
        int y1 = static_cast<int>((lineOrigin.y - cameraOffset.y) * cameraZoom);
        int x2 = static_cast<int>((hookCenter.x - cameraOffset.x) * cameraZoom);
        int y2 = static_cast<int>((hookCenter.y - cameraOffset.y) * cameraZoom);

        // Draw fishing line (brown color)
        SDL_SetRenderDrawColor(renderer, 139, 69, 19, 255);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }

    // Render particles (called from main render loop)
    void renderParticles(SDL_Renderer* renderer, const Vector2& cameraOffset, float cameraZoom) {
        if (attractParticles) attractParticles->render(renderer, cameraOffset, cameraZoom);
    }
};
