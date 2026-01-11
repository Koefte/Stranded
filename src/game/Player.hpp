   
#pragma once

#include <SDL.h>
#include <cmath>
#include "Vector2.hpp"
#include "GameObject.hpp"
#include "IAnimatable.hpp"
#include "ICollidable.hpp"
#include "IInteractable.hpp"
#include "Rod.hpp"
#include "FishingHook.hpp"

class Player : public IAnimatable, public ICollidable {
private:
    float speed = 200.0f;  // pixels per second
    Vector2 prevPosition;  // Track previous frame position
    Vector2 velocity = {0.0f, 0.0f};  // Current velocity
    bool moveUp = false, moveDown = false, moveLeft = false, moveRight = false;
    SDL_Renderer* renderer;
    Rod* rod = nullptr;
    FishingHook* fishingHook = nullptr;

public:
   

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
    }

    // Public getter for the player's rod
    Rod* getRod() const {
        return rod;
    }

    FishingHook* getFishingProjectile() const {
        return fishingHook;
    }

    ~Player() {
        if (rod) {
            removeChild(rod);
            delete rod;
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
            case SDLK_r: {
                if (rod) {
                    if (rod->getVisible()) {
                        rod->hide();
                    } else {
                        rod->show();
                    }
                }
                break;
            }
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
        if (button == SDL_BUTTON_LEFT && isRodVisible() && fishingHook && rod) {
            // Always retract before casting to allow recasting
            fishingHook->retract();
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
        }
    }


    void update(float dt) override {
        IAnimatable::update(dt);  // Always update animation
        
        // Update fishing hook and its line origin
        if (fishingHook && fishingHook->getIsActive() && rod) {
            // Update the line origin to follow the rod position
            Vector2 rodWorld = rod->getWorldPosition();
            Vector2* rodSize = rod->getSize();
            Vector2 rodTip = {
                rodWorld.x + rodSize->x / 2.0f,
                rodWorld.y + rodSize->y
            };
            fishingHook->updateLineOrigin(rodTip);
            fishingHook->update(dt);
        } else if (fishingHook) {
            fishingHook->update(dt);
        }

        // Despawn fishing hook if rod is disabled (not visible)
        if (fishingHook && fishingHook->getIsActive() && !isRodVisible()) {
            fishingHook->retract();
        }
        
        Vector2* pos = getPosition();
        prevPosition = *pos;  // Save before movement

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
        } else {
            velocity.x = 0.0f;
            velocity.y = 0.0f;
            stopAnimation();
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