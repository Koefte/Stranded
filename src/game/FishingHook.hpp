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
    // If true, suppress playing the "arrival" sound for the current/previous spawn (used on recast)
    bool suppressArrivalSoundUntilNextSpawn = false;
    // Retract debounce to avoid flicker when snapshots briefly indicate inactive
    float retractDebounce = 0.15f; // seconds
    float pendingRetractTimer = 0.0f;
    // Pending explicit start positions for networked spawn
    std::vector<Vector2> pendingStartPositions;
    Vector2 pendingEndPosition{0.0f,0.0f};
    float pendingDuration = 0.0f;
    // Debug visualization for networked particle starts
    bool attractDebugDraw = false;
    std::vector<Vector2> debugPositions;
    float debugDrawDuration = 3.0f;
    float debugTimer = 0.0f;
    std::mt19937 rng{std::random_device{}()};
    int lastAttractAliveCount = 0;
    // Callback invoked when attract particles have finished arriving
    std::function<void()> onAttractArrival = nullptr;

    // Callback invoked when the hook reaches its destination (owner id unknown to the hook)
    std::function<void(const Vector2&)> onHookArrival = nullptr;

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

    void setOnHookArrival(std::function<void(const Vector2&)> cb) {
        onHookArrival = cb;
    }

    // Enable or disable a visual debug overlay that shows received particle start positions
    void setAttractDebug(bool enable) {
        attractDebugDraw = enable;
        if (!enable) {
            debugPositions.clear();
            debugTimer = 0.0f;
        }
    }

    // Retract debounce helpers used by snapshot syncing to avoid flicker
    void startRetractDebounce(float t) {
        pendingRetractTimer = t;
    }

    void cancelPendingRetract() {
        pendingRetractTimer = 0.0f;
    }

    // Ensure all positions are world coordinates
    // When casting, set position and targetPos in world coordinates
    void cast(Vector2 startPos, Vector2 direction, Vector2 mousePos, float castSpeed = 200.0f, bool playAttractSound = true) {
        // Cancel any pending retract (we're recasting so it must stay visible)
        pendingRetractTimer = 0.0f;
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
        SDL_Log("FishingHook %p cast target=(%.2f,%.2f) playAttractSound=%d", this, mousePos.x, mousePos.y, playAttractSound);
        // Schedule attract particles to spawn after a random delay (local-only)
        // Make delays always longer than the previous range (0.5-2.5s)
        if (!hasAttractSeed) {
            std::uniform_real_distribution<float> delayDist(2.6f, 5.0f);
            attractTimer = delayDist(rng);
            attractPending = true;
            attractPlaySound = playAttractSound;
            attractUseAbsoluteStart = false;
            // Reset suppression when a new local spawn is created
            suppressArrivalSoundUntilNextSpawn = false;
            SDL_Log("FishingHook %p local scheduled attract delay=%.2f", this, attractTimer);
        }
    }

    void update(float dt) override {
        // Handle pending retract debounce first
        if (pendingRetractTimer > 0.0f) {
            pendingRetractTimer -= dt;
            if (pendingRetractTimer <= 0.0f) {
                // Perform the actual retract now with hiding
                retract(true);
                pendingRetractTimer = 0.0f;
            }
        }

        if (!isActive) return;

        // Apply gravity
        velocity.y += gravity * dt;

        // Update position
        Vector2* pos = getPosition();
        pos->x += velocity.x * dt;
        pos->y += velocity.y * dt;

        // Check if hook has reached or passed the target position (mouse)
        float distToTarget = std::sqrt((pos->x - targetPos.x) * (pos->x - targetPos.x) + (pos->y - targetPos.y) * (pos->y - targetPos.y));
        bool prevDestReached = destReached;
        if (distToTarget < 8.0f ||
            ((velocity.x > 0 && pos->x >= targetPos.x) || (velocity.x < 0 && pos->x <= targetPos.x)) &&
            ((velocity.y > 0 && pos->y >= targetPos.y) || (velocity.y < 0 && pos->y <= targetPos.y))) {
            // Snap to target and stop
            pos->x = targetPos.x;
            pos->y = targetPos.y;
            destReached = true;
            velocity = {0.0f, 0.0f};
        }

        // If we've just reached the destination this frame, invoke arrival callback
        if (destReached && !prevDestReached) {
            SDL_Log("FishingHook %p arrived at (%.2f,%.2f)", this, pos->x, pos->y);
            if (onHookArrival) onHookArrival(getWorldPosition());
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
                if (!pendingStartPositions.empty()) {
                    SDL_Log("FishingHook %p spawnFromPositions count=%d", this, static_cast<int>(pendingStartPositions.size()));
                    // Use explicit particle start positions provided by the host (networked)
                    if (attractParticles) {
                        SDL_Color col = attractColor;
                        float dur = pendingDuration > 0.0f ? pendingDuration : (attractDuration > 0.0f ? attractDuration : 4.5f);
                        int zidx = attractZIndex > 0 ? attractZIndex : 4;
                        attractParticles->emitFromStarts(pendingStartPositions, hookPos, dur, col, zidx);
                        // Play attract spawn sound only if allowed for this scheduled spawn
                        if (attractPlaySound) SoundManager::instance().playSound("attract_spawn", 0, MIX_MAX_VOLUME);
                        lastAttractAliveCount = static_cast<int>(pendingStartPositions.size());
                        pendingStartPositions.clear();
                    }
                } else {
                    if (attractUseAbsoluteStart) {
                        startCenter = attractAbsoluteStart;
                    } else {
                        startCenter = { hookPos.x + std::cos(angle) * radius, hookPos.y + std::sin(angle) * radius };
                    }
                    SDL_Log("FishingHook %p spawning count=%d seed=%d start=(%.2f,%.2f) hook=(%.2f,%.2f)", this, attractCount, hasAttractSeed ? static_cast<int>(attractSeed) : -1, startCenter.x, startCenter.y, hookPos.x, hookPos.y);
                    // Emit particles that move from startCenter toward the current hook position
                    if (attractParticles) {
                        int count = attractCount > 0 ? attractCount : 10;
                        SDL_Color col = attractColor;
                        float dur = attractDuration > 0.0f ? attractDuration : 4.5f;
                        int zidx = attractZIndex > 0 ? attractZIndex : 4;
                        float spr = attractSpread;
                        if (hasAttractSeed) {
                            // Deterministic emit using seed so clients reproduce same noisy starts
                            attractParticles->emitFromSeed(attractSeed, startCenter, hookPos, count, col, dur, zidx, spr);
                            lastAttractAliveCount = count;
                        } else {
                            attractParticles->emit(startCenter, hookPos, count, col, dur, zidx, spr);
                            lastAttractAliveCount = count;
                        }
                        // Play attract spawn sound only if allowed for this scheduled spawn
                        if (attractPlaySound) SoundManager::instance().playSound("attract_spawn", 0, MIX_MAX_VOLUME);
                    }
                }
                attractPending = false;
                hasAttractSeed = false;
                SDL_Log("FishingHook %p finished scheduling spawn (pending now=%d)", this, attractPending);
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
                // Play arrival sound once, only if allowed for this spawn and not suppressed due to recast
                if (!suppressArrivalSoundUntilNextSpawn && attractPlaySound) {
                    SoundManager::instance().playSound("attract_arrival", 0, MIX_MAX_VOLUME);
                }
                // Invoke optional arrival callback
                if (onAttractArrival) {
                    SDL_Log("FishingHook %p invoking onAttractArrival callback", this);
                    onAttractArrival();
                }
                lastAttractAliveCount = 0;
                // Reset suppression after handling arrival
                suppressArrivalSoundUntilNextSpawn = false;
            } else {
                lastAttractAliveCount = alive;
            }
        }
        // Debug timer for drawing received start positions
        if (debugTimer > 0.0f) {
            debugTimer -= dt;
            if (debugTimer <= 0.0f) debugPositions.clear();
        }
    }

    void retract(bool hide = true) {
        SDL_Log("FishingHook %p retract (wasActive=%d) hide=%d", this, isActive, hide);
        isActive = false;
        destReached = false;
        if (attractParticles) {
            attractParticles->getParticles().clear();
        }
        // Cancel any pending attract spawn and explicit start positions so no future particles are emitted
        cancelPendingAttract();
        pendingStartPositions.clear();
        pendingDuration = 0.0f;
        attractPlaySound = false;
        // Suppress arrival sound from any in-flight particles (they were cleared on retract)
        suppressArrivalSoundUntilNextSpawn = true;
        lastAttractAliveCount = 0;
        // Cancel any pending retract timer (we're retracting now)
        pendingRetractTimer = 0.0f;
        if (hide) setVisible(false);
        velocity = {0.0f, 0.0f};
    }

    // Schedule an attract spawn using explicit particle start positions (networked)
    void scheduleAttractFromPositions(const std::vector<Vector2>& starts, const Vector2& end, float delay, SDL_Color color, float duration, int zIndex, bool playSound = false) {
        pendingStartPositions = starts;
        pendingEndPosition = end;
        pendingDuration = duration;
        attractPlaySound = playSound;
        // New spawn: clear suppression so arrival sound plays for this spawn
        suppressArrivalSoundUntilNextSpawn = false;
        attractPending = true;
        attractTimer = delay;
        // If debug drawing is enabled, snapshot these positions for visual verification
        if (attractDebugDraw) {
            debugPositions = pendingStartPositions;
            debugTimer = debugDrawDuration;
        }
    }

    // Schedule attract spawn deterministically from a seed (does not emit immediately)
    // If useAbsoluteStart==true, `absoluteStart` is used for particle origin; otherwise host/client compute relative start
    // Accept an explicit delay (host-determined). If delay < 0, compute delay deterministically from seed.
    void scheduleAttractFromSeed(uint32_t seed, int count, SDL_Color color, float duration, int zIndex, float spread = 12.0f, Vector2 absoluteStart = {0,0}, bool useAbsoluteStart = false, bool playSound = true, float delay = -1.0f) {
        SDL_Log("FishingHook %p scheduleAttractFromSeed seed=%u count=%d delay=%.2f useAbs=%d", this, seed, count, delay, useAbsoluteStart);
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

        // If the host provided an exact delay, use it; otherwise compute deterministically from the seed
        if (delay >= 0.0f) {
            attractTimer = delay;
        } else {
            std::mt19937 tmp(seed);
            std::uniform_real_distribution<float> delayDist(2.6f, 5.0f);
            attractTimer = delayDist(tmp);
        }

        // For non-absolute starts, compute radius/angle from seed so local computations match host
        std::mt19937 tmp2(seed);
        std::uniform_real_distribution<float> radiusDist(40.0f, 140.0f);
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
        attractRadius = radiusDist(tmp2);
        attractAngle = angleDist(tmp2);

        attractPending = true;
        // New spawn: clear suppression so arrival sound plays for this spawn
        suppressArrivalSoundUntilNextSpawn = false;

        SDL_Log("FishingHook %p attractTimer=%.2f radius=%.2f angle=%.2f", this, attractTimer, attractRadius, attractAngle);

        // If debug drawing is enabled, snapshot per-particle positions generated from the seed
        if (attractDebugDraw) {
            std::mt19937 seededRng(seed);
            std::uniform_real_distribution<float> noise(-attractSpread, attractSpread);
            debugPositions.clear();
            int c = attractCount > 0 ? attractCount : 10;
            Vector2 center = attractUseAbsoluteStart ? attractAbsoluteStart : Vector2{0,0};
            if (!attractUseAbsoluteStart) {
                // compute center using radius/angle we derived
                center = { getWorldPosition().x + std::cos(attractAngle) * attractRadius, getWorldPosition().y + std::sin(attractAngle) * attractRadius };
            }
            for (int i = 0; i < c; ++i) {
                debugPositions.emplace_back(Vector2{center.x + noise(seededRng), center.y + noise(seededRng)});
            }
            debugTimer = debugDrawDuration;
            SDL_Log("FishingHook %p debug snapshot %d positions", this, static_cast<int>(debugPositions.size()));
        }
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

        // Draw debug markers for received start positions (if enabled)
        if (attractDebugDraw && !debugPositions.empty()) {
            // Semi-transparent red squares
            SDL_BlendMode oldMode;
            SDL_GetRenderDrawBlendMode(renderer, &oldMode);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 192);
            for (auto &p : debugPositions) {
                int sx = static_cast<int>((p.x - cameraOffset.x) * cameraZoom);
                int sy = static_cast<int>((p.y - cameraOffset.y) * cameraZoom);
                int size = std::max(4, static_cast<int>(6 * cameraZoom));
                SDL_Rect r{ sx - size/2, sy - size/2, size, size };
                SDL_RenderFillRect(renderer, &r);
            }
            SDL_SetRenderDrawBlendMode(renderer, oldMode);
        }
    }

    // Spawn attract particles immediately (used by networked spawn)
    void spawnAttractParticles(const Vector2& startCenter, const Vector2& hookPos, int count, SDL_Color color, float duration, int zIndex, float spread = 12.0f) {
        if (attractParticles) {
            attractParticles->emit(startCenter, hookPos, count, color, duration, zIndex, spread);
            lastAttractAliveCount = count;
        }
    }

    // Set the hook state to arrived at the given world position (authoritative)
    void setArrivedAt(const Vector2& pos) {
        Vector2* p = getPosition();
        p->x = pos.x;
        p->y = pos.y;
        targetPos = pos;
        destReached = true;
        isActive = true; // remains visible until retracted
        velocity = {0.0f, 0.0f};
        setVisible(true);
        SDL_Log("FishingHook %p setArrivedAt (%.2f,%.2f)", this, pos.x, pos.y);
    }
};
