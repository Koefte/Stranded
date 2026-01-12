#pragma once

#include "UIGameObject.hpp"
#include <SDL_ttf.h>
#include <string>
#include <SDL.h>
#include <iostream>

// Header-only Text UI element backed by SDL_ttf
class Text : public UIGameObject {
public:
    Text(Vector2 screenPos, const std::string& textInit, const char* fontPath, int fontSize, SDL_Renderer* renderer, SDL_Color color = {255,255,255,255}, int zIndex = 0)
        : UIGameObject(screenPos, {1.0f, 1.0f}, nullptr, renderer, zIndex), text(textInit), color(color), renderer(renderer) {
        font = nullptr;
        if (fontPath) {
            font = TTF_OpenFont(fontPath, fontSize);
            if (!font) {
                std::cerr << "Warning: Failed to open font '" << fontPath << "': " << TTF_GetError() << "\n";
            }
        }
        rebuildTexture();
    }

    ~Text() {
        SDL_Texture* tex = getSprite();
        if (tex) {
            SDL_DestroyTexture(tex);
            setSprite(nullptr);
        }
        if (font) {
            TTF_CloseFont(font);
            font = nullptr;
        }
    }

    void setText(const std::string& newText) { text = newText; rebuildTexture(); }
    void setColor(SDL_Color newColor) { color = newColor; rebuildTexture(); }
    bool setFont(const char* fontPath, int fontSize) {
        if (font) {
            TTF_CloseFont(font);
            font = nullptr;
        }
        font = TTF_OpenFont(fontPath, fontSize);
        if (!font) {
            std::cerr << "Warning: Failed to open font '" << fontPath << "': " << TTF_GetError() << "\n";
            return false;
        }
        rebuildTexture();
        return true;
    }

private:
    void rebuildTexture() {
        SDL_Texture* old = getSprite();
        if (old) {
            SDL_DestroyTexture(old);
            setSprite(nullptr);
        }
        if (!font || !renderer) return;

        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (!surf) {
            std::cerr << "Warning: TTF_RenderUTF8_Blended failed: " << TTF_GetError() << "\n";
            return;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (!tex) {
            std::cerr << "Warning: SDL_CreateTextureFromSurface failed: " << SDL_GetError() << "\n";
            SDL_FreeSurface(surf);
            return;
        }

        // Ensure alpha blending is enabled so text renders with transparency
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        SDL_FreeSurface(surf);
        setSprite(tex);
    }

    TTF_Font* font = nullptr;
    SDL_Color color{255,255,255,255};
    std::string text;
    SDL_Renderer* renderer = nullptr;
};
