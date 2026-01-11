#pragma once

#include <SDL.h>
#include "Vector2.hpp"
#include "GameObject.hpp"
#include "ParticleSystem.hpp"

class FishingHook : public GameObject {
private:
    Vector2 velocity;
    Vector2 lineOrigin; // Position where the line connects to the rod
    Vector2 targetPos; // Where the hook should stop (mouse pos)
    float gravity = 300.0f;
    bool isActive = false;
    bool destReached = false;
    bool emittedSplash = false;
    ParticleSystem* splashParticles = nullptr;

public:
    FishingHook(Vector2 pos, Vector2 sizeMultiplier, const char* spritePath, SDL_Renderer* renderer, int zIndex = 2)
        : GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex),
          velocity({0.0f, 0.0f}),
          lineOrigin({0.0f, 0.0f}),
          splashParticles(new ParticleSystem(renderer))
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

        if(destReached && !emittedSplash){
            splashParticles->emit(
                getWorldPosition(),
                getWorldPosition(),
                10,
                SDL_Color{0, 150, 255, 255},
                5.0f,
                5
            );
            // Do not parent particles to the hook; ParticleSystem renders them independently
            emittedSplash = true;

        }
        if(emittedSplash) splashParticles->update(dt);
    }

    void retract() {
        isActive = false;
        destReached = false;
        emittedSplash = false;
        splashParticles->getParticles().clear();
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

    // Render splash particles (called from main render loop)
    void renderParticles(SDL_Renderer* renderer, const Vector2& cameraOffset, float cameraZoom) {
        if (splashParticles) splashParticles->render(renderer, cameraOffset, cameraZoom);
    }
};
