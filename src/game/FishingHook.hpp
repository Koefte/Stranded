#pragma once

#include <SDL.h>
#include "Vector2.hpp"
#include "GameObject.hpp"
#include "ParticleSystem.hpp"
#include <random>
#include "../audio/SoundManager.hpp"
#include <functional>

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
    // Seeded attract parameters (if scheduled by network)
    bool hasAttractSeed = false;
    uint32_t attractSeed = 0;
    float attractRadius = 0.0f;
    float attractAngle = 0.0f;
    int attractCount = 0;
    SDL_Color attractColor{0,255,0,255};
    float attractDuration = 0.0f;
    int attractZIndex = 0;
    float attractSpread = 12.0f;
    // If host provided absolute start position, store it and use it when spawning
    bool attractUseAbsoluteStart = false;
    Vector2 attractAbsoluteStart{0.0f,0.0f};
    // Whether to play attract sounds for this scheduled spawn
    bool attractPlaySound = true;
    // Pending explicit start positions for networked spawn
    std::vector<Vector2> pendingStartPositions;
    Vector2 pendingEndPosition{0.0f,0.0f};
    float pendingDuration = 0.0f;
    std::mt19937 rng{std::random_device{}()};
    int lastAttractAliveCount = 0;
    // Callback invoked when attract particles have finished arriving
    std::function<void()> onAttractArrival = nullptr;

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

    // Register a callback to be invoked when attract particles finish
    void setOnAttractArrival(std::function<void()> cb) {
        onAttractArrival = cb;
    }

    // Ensure all positions are world coordinates
    // When casting, set position and targetPos in world coordinates
    void cast(Vector2 startPos, Vector2 direction, Vector2 mousePos, float castSpeed = 200.0f, bool playAttractSound = true) {
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
        // Schedule attract particles to spawn after a random delay (local-only)
        // Make delays always longer than the previous range (0.5-2.5s)
        if (!hasAttractSeed) {
            std::uniform_real_distribution<float> delayDist(2.6f, 5.0f);
            attractTimer = delayDist(rng);
            attractPending = true;
            attractPlaySound = playAttractSound;
            attractUseAbsoluteStart = false;
        }
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
                // Compute spawn parameters. If scheduled via seed, use stored radius/angle; otherwise randomize now
                Vector2 hookPos = getWorldPosition();
                float radius = attractRadius;
                float angle = attractAngle;
                if (!hasAttractSeed) {
                    std::uniform_real_distribution<float> radiusDist(40.0f, 140.0f);
                    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
                    radius = radiusDist(rng);
                    angle = angleDist(rng);
                }
                Vector2 startCenter;
                if (attractUseAbsoluteStart) {
                    startCenter = attractAbsoluteStart;
                } else {
                    startCenter = { hookPos.x + std::cos(angle) * radius, hookPos.y + std::sin(angle) * radius };
                }
                // Emit particles that move from startCenter toward the current hook position
                if (attractParticles) {
                    int count = attractCount > 0 ? attractCount : 10;
                    SDL_Color col = attractColor;
                    float dur = attractDuration > 0.0f ? attractDuration : 4.5f;
                    int zidx = attractZIndex > 0 ? attractZIndex : 4;
                    float spr = attractSpread;
                    attractParticles->emit(startCenter, hookPos, count, col, dur, zidx, spr);
                    // Play attract spawn sound only if allowed for this scheduled spawn
                    if (attractPlaySound) SoundManager::instance().playSound("attract_spawn", 0, MIX_MAX_VOLUME);
                    lastAttractAliveCount = count;
                }
                attractPending = false;
                hasAttractSeed = false;
            }
        }
        if (attractParticles) {
            attractParticles->update(dt);
            // Check for arrival: previously had alive particles, now none alive
            int alive = 0;
            for (auto& p : attractParticles->getParticles()) {
                if (p.alive) ++alive;
            }
            if (lastAttractAliveCount > 0 && alive == 0) {
                // Play arrival sound once, only if allowed for this spawn
                if (attractPlaySound) SoundManager::instance().playSound("attract_arrival", 0, MIX_MAX_VOLUME);
                // Invoke optional arrival callback
                if (onAttractArrival) onAttractArrival();
                lastAttractAliveCount = 0;
            } else {
                lastAttractAliveCount = alive;
            }
        }
    }

    void retract() {
        isActive = false;
        destReached = false;
        if (attractParticles) attractParticles->getParticles().clear();
        setVisible(false);
        velocity = {0.0f, 0.0f};
    }

    // Schedule an attract spawn using explicit particle start positions (networked)
    void scheduleAttractFromPositions(const std::vector<Vector2>& starts, const Vector2& end, float delay, SDL_Color color, float duration, int zIndex, bool playSound = false) {
        pendingStartPositions = starts;
        pendingEndPosition = end;
        pendingDuration = duration;
        attractPlaySound = playSound;
        attractPending = true;
        attractTimer = delay;
    }

    // Schedule attract spawn deterministically from a seed (does not emit immediately)
    // If useAbsoluteStart==true, `absoluteStart` is used for particle origin; otherwise host/client compute relative start
    void scheduleAttractFromSeed(uint32_t seed, int count, SDL_Color color, float duration, int zIndex, float spread = 12.0f, Vector2 absoluteStart = {0,0}, bool useAbsoluteStart = false, bool playSound = true) {
        attractSeed = seed;
        hasAttractSeed = true;
        attractCount = count;
        attractColor = color;
        attractDuration = duration;
        attractZIndex = zIndex;
        attractSpread = spread;
        attractUseAbsoluteStart = useAbsoluteStart;
        attractAbsoluteStart = absoluteStart;
        attractPlaySound = playSound;
        // Seed temporary RNG to compute delay and radius/angle deterministically
        std::mt19937 tmp(seed);
        std::uniform_real_distribution<float> delayDist(2.6f, 5.0f);
        std::uniform_real_distribution<float> radiusDist(40.0f, 140.0f);
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
        attractTimer = delayDist(tmp);
        attractRadius = radiusDist(tmp);
        attractAngle = angleDist(tmp);
        attractPending = true;
    }

    void cancelPendingAttract() {
        attractPending = false;
        hasAttractSeed = false;
        attractTimer = 0.0f;
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

    // Spawn attract particles immediately (used by networked spawn)
    void spawnAttractParticles(const Vector2& startCenter, const Vector2& hookPos, int count, SDL_Color color, float duration, int zIndex, float spread = 12.0f) {
        if (attractParticles) {
            attractParticles->emit(startCenter, hookPos, count, color, duration, zIndex, spread);
            lastAttractAliveCount = count;
        }
    }
};
