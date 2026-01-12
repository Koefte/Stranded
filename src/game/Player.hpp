   
#pragma once

#include <SDL.h>
#include <cmath>
#include "Vector2.hpp"
#include "GameObject.hpp"

// When host casts locally, broadcast a compact seed-based particle packet to clients
extern void hostBroadcastParticleForHook(const Vector2& hookTip);
#include "IAnimatable.hpp"
#include "ICollidable.hpp"
#include "IInteractable.hpp"
#include "Rod.hpp"
#include "FishingHook.hpp"
#include "Gun.hpp"
#include <vector>

// Access global gameObjects so Player can register projectiles
extern std::vector<GameObject*> gameObjects;

class Player : public IAnimatable, public ICollidable {
private:
    float speed = 200.0f;  // pixels per second
    Vector2 prevPosition;  // Track previous frame position
    Vector2 velocity = {0.0f, 0.0f};  // Current velocity
    bool moveUp = false, moveDown = false, moveLeft = false, moveRight = false;
    SDL_Renderer* renderer;
    Rod* rod = nullptr;
    FishingHook* fishingHook = nullptr;
    Gun* gun = nullptr;

    // Player equipment enum (public API via equip/getEquipment)
    
    bool walkingSoundPlaying = false;
    bool isRemote = false; // when true, this player should not play local-only sounds like walking
    
public:
    enum Equipment {EQUIP_NONE = 0, EQUIP_ROD = 1, EQUIP_HARPOON = 2};
   

    // Construct from individual frame paths
    Player(
        Vector2 pos,
        Vector2 sizeMultiplier,
        const char* spritePaths[],
        int frameCount,
        SDL_Renderer* renderer,
        float animationStep,
        int zIndex = 0)
        : GameObject(pos, sizeMultiplier, spritePaths[0], renderer, zIndex),
          IAnimatable(pos, sizeMultiplier, spritePaths, frameCount, renderer, animationStep, zIndex),
          ICollidable(pos, sizeMultiplier, spritePaths[0], renderer, false, zIndex),
          renderer(renderer)
    {
        // Create rod as a child object with local position offset
        rod = new Rod({-7.0f, 12.0f}, {2.0f, 2.0f}, "./sprites/Rod.bmp", renderer, zIndex + 1);
        addChild(rod);
        rod->hide();
        
        // Create fishing Rod projectile (not a child, moves independently)
        fishingHook = new FishingHook({0.0f, 0.0f}, {2.0f, 2.0f}, "./sprites/Hook.bmp", renderer, zIndex + 2);

        // Create harpoon gun as a child (hidden by default)
        gun = new Gun({-7.0f, 8.0f}, {1.0f,1.0f}, "./sprites/gun.bmp", renderer, zIndex + 1);
        addChild(gun);
        gun->hide();
    }

    // Public getter for the player's rod
    Rod* getRod() const {
        return rod;
    }

    FishingHook* getFishingProjectile() const {
        return fishingHook;
    }

    Gun* getGun() const { return gun; }

    // Mark this player as remote-controlled (do not play local-only sounds)
    void setRemote(bool remote) { isRemote = remote; }

    // Equipment API - equip selected tool; harpoon behavior is left as a placeholder
    // (enum is declared above so external code can reference Player::EQUIP_ROD / Player::EQUIP_HARPOON)
    Equipment currentEquipment = EQUIP_ROD;

    void equip(Equipment e) {
        currentEquipment = e;
        if (e == EQUIP_ROD) {
            if (rod) rod->show();
            if (gun) gun->hide();
            SDL_Log("Player: equipped Rod");
        } else if (e == EQUIP_HARPOON) {
            // Show gun and hide rod
            if (rod) rod->hide();
            if (gun) gun->show();
            SDL_Log("Player: equipped Harpoon");
        } else {
            if (rod) rod->hide();
            if (gun) gun->hide();
            SDL_Log("Player: equipped None");
        }
    }

    Equipment getEquipment() const { return currentEquipment; }

    // Convenience helpers so external code doesn't need to access enum constants directly
    void equipRod() { equip(EQUIP_ROD); }
    void equipHarpoon() { equip(EQUIP_HARPOON); }

    ~Player() {
        if (rod) {
            removeChild(rod);
            delete rod;
        }
        if (gun) {
            removeChild(gun);
            delete gun;
        }
        if (fishingHook) {
            delete fishingHook;
        }
    }

    bool isRodVisible() const {
        return rod ? rod->getVisible() : false;
    }

    void setRodVisible(bool visible) {
        if (rod) {
            if (visible) {
                rod->show();
            } else {
                rod->hide();
            }
        }
    }

    void onKeyDown(SDL_Keycode key) override {
        switch (key) {
            case SDLK_w: moveUp = true; break;
            case SDLK_s: moveDown = true; break;
            case SDLK_a: moveLeft = true; break;
            case SDLK_d: moveRight = true; break;
            default: break;
        }
    }

    void onKeyUp(SDL_Keycode key) override {
        switch (key) {
            case SDLK_w: moveUp = false; break;
            case SDLK_s: moveDown = false; break;
            case SDLK_a: moveLeft = false; break;
            case SDLK_d: moveRight = false; break;
            default: break;
        }
    }

    void setHooking(bool hooking) {
        if (rod) {
            if (hooking) {
                rod->show();
            } else {
                rod->hide();
                if (fishingHook) {
                    fishingHook->retract();
                }
            }
        }
    }

    void onMouseDown(int button, int mouseX, int mouseY, const Vector2& cameraOffset, float cameraZoom) {
        // Harpoon firing when equipped
        if (button == SDL_BUTTON_LEFT && currentEquipment == EQUIP_HARPOON && gun) {
            Vector2 worldMousePos = {
                (mouseX / cameraZoom) + cameraOffset.x,
                (mouseY / cameraZoom) + cameraOffset.y
            };
            Projectile* proj = gun->getProjectile();
            if (proj) {
                // Attempt to fire; gun->fireAt returns true only if cooldown allowed
                bool fired = gun->fireAt(worldMousePos);
                if (fired) {
                    // Ensure projectile is part of global updates
                    if (std::find(gameObjects.begin(), gameObjects.end(), proj) == gameObjects.end()) {
                        gameObjects.push_back(proj);
                    }
                    SoundManager::instance().playSound("shoot", 0, MIX_MAX_VOLUME);
                }
            }
            return; // consume click
        }

        if (button == SDL_BUTTON_LEFT && isRodVisible() && fishingHook && rod) {
            // Always retract before casting to allow recasting (don't hide to avoid flicker)
            fishingHook->retract(false);
            // Calculate world position from screen position
            Vector2 worldMousePos = {
                (mouseX / cameraZoom) + cameraOffset.x,
                (mouseY / cameraZoom) + cameraOffset.y
            };
            // Get rod's world position (where the rod tip is)
            Vector2 rodWorld = rod->getWorldPosition();
            Vector2* rodSize = rod->getSize();
            Vector2 rodTip = {
                rodWorld.x + rodSize->x / 2.0f,
                rodWorld.y + rodSize->y
            };
            // Calculate direction from rod tip to mouse
            Vector2 direction = {
                worldMousePos.x - rodTip.x,
                worldMousePos.y - rodTip.y
            };
            // Cast the fishing hook from the rod tip position, and stop at mouse
            fishingHook->cast(rodTip, direction, worldMousePos);
            // Play cast sound
            SoundManager::instance().playSound("cast", 0, MIX_MAX_VOLUME);
            // If running as host, broadcast a compact seed packet so clients reproduce the spawn
            hostBroadcastParticleForHook(rodTip);
        }
    }


    void update(float dt) override {
        IAnimatable::update(dt);  // Always update animation
        
        // Update fishing hook's line origin so the line follows the rod.
        // The actual hook update is handled by the global gameObjects update loop to avoid double-updating.
        if (fishingHook && rod) {
            Vector2 rodWorld = rod->getWorldPosition();
            Vector2* rodSize = rod->getSize();
            Vector2 rodTip = {
                rodWorld.x + rodSize->x / 2.0f,
                rodWorld.y + rodSize->y
            };
            fishingHook->updateLineOrigin(rodTip);
        }

        // Despawn fishing hook if rod is disabled (not visible)
        if (fishingHook && fishingHook->getIsActive() && !isRodVisible()) {
            fishingHook->retract();
        }
        
        Vector2* pos = getPosition();
        prevPosition = *pos;  // Save before movement

        // Update child-only objects that need per-frame logic (e.g., gun cooldown)
        if (gun) gun->update(dt);

        float dx = 0.0f;
        float dy = 0.0f;

        if (moveUp) dy -= 1.0f;
        if (moveDown) dy += 1.0f;
        if (moveLeft) dx -= 1.0f;
        if (moveRight) dx += 1.0f;

        if (dx != 0.0f || dy != 0.0f) {
            const float length = std::sqrt((dx * dx) + (dy * dy));
            dx = (dx / length) * speed;
            dy = (dy / length) * speed;

            velocity.x = dx;
            velocity.y = dy;
            pos->x += dx * dt;
            pos->y += dy * dt;
            startAnimation();
            // Start walking sound if not already playing and only for local players
            if (!walkingSoundPlaying && !isRemote) {
                SoundManager::instance().playSound("walk", -1, MIX_MAX_VOLUME / 2);
                walkingSoundPlaying = true;
            }
        } else {
            velocity.x = 0.0f;
            velocity.y = 0.0f;
            stopAnimation();
            // Stop walking sound when movement stops (only stop if it was playing locally)
            if (walkingSoundPlaying) {
                if (!isRemote) SoundManager::instance().stopSound("walk");
                walkingSoundPlaying = false;
            }
        }
    }

    Vector2 getVelocity() const {
        return velocity;
    }

    void setVelocity(Vector2 vel) {
        velocity = vel;
    }

    void applyVelocity(float dt) {
        Vector2* pos = getPosition();
        pos->x += velocity.x * dt;
        pos->y += velocity.y * dt;
    }

    // Method to move player externally (e.g., by boat)
    void moveExternally(float dx, float dy) {
        changePosition(dx, dy);
        // Update prevPosition so collision response doesn't revert this movement
        Vector2* pos = getPosition();
        prevPosition = *pos;
    }

    void updatePrevPosition() {
        Vector2* pos = getPosition();
        prevPosition = *pos;
    }

    void onCollisionEnter(ICollidable* other) override {
        Vector2* pos = getPosition();
        *pos = prevPosition;  // Revert to last valid position
        SDL_Log("Player collided!");
    }

    void onCollisionStay(ICollidable* other) override {
        // Optional: Handle continuous collision
        Vector2* pos = getPosition();
        *pos = prevPosition;  // Revert to last valid position
        
    }

    
};