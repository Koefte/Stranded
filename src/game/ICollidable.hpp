#pragma once
#include <vector>
#include <algorithm>
#include <iostream>
#include <utility>
#include <SDL.h>
#include <SDL_image.h>
#include "GameObject.hpp"
#include "Rectangle.hpp"

struct RowSpan{
    int minX;
    int maxX;
};

class ICollidable : virtual public GameObject {
    private:
    std::vector<Rectangle> collisionRectangles;
    bool isComplex;
    float originalSurfaceWidth;
    float originalSurfaceHeight;
    public:

    static std::vector<Rectangle> autoDetectHitboxes(SDL_Surface* surface, int minClusterSize = 50) {
        std::vector<Rectangle> hitboxes;
        
        if (!surface) {
            return hitboxes;
        }

        // Lock surface for pixel access
        if (SDL_MUSTLOCK(surface)) {
            SDL_LockSurface(surface);
        }

        int width = surface->w;
        int height = surface->h;
        SDL_PixelFormat* format = surface->format;
        
        // Helper lambda to check if a pixel is opaque
        auto isOpaque = [&](int x, int y) -> bool {
            if (x < 0 || x >= width || y < 0 || y >= height) return false;
            
            Uint8* pixels = (Uint8*)surface->pixels;
            Uint32 pixel = 0;
            
            switch (format->BytesPerPixel) {
                case 1:
                    pixel = pixels[y * surface->pitch + x];
                    break;
                case 2:
                    pixel = *((Uint16*)(pixels + y * surface->pitch + x * 2));
                    break;
                case 3: {
                    Uint8* p = pixels + y * surface->pitch + x * 3;
                    if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                        pixel = p[0] << 16 | p[1] << 8 | p[2];
                    else
                        pixel = p[0] | p[1] << 8 | p[2] << 16;
                    break;
                }
                case 4:
                    pixel = *((Uint32*)(pixels + y * surface->pitch + x * 4));
                    break;
            }
            
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
            
            // Consider pixel opaque if alpha > 128
            return a > 128;
        };

        // Scan each row to find horizontal spans of opaque pixels
        std::vector<std::vector<RowSpan>> rowSpans(height);
        
        for (int y = 0; y < height; y++) {
            int spanStart = -1;
            for (int x = 0; x < width; x++) {
                if (isOpaque(x, y)) {
                    if (spanStart == -1) {
                        spanStart = x;
                    }
                } else {
                    if (spanStart != -1) {
                        rowSpans[y].push_back({spanStart, x - 1});
                        spanStart = -1;
                    }
                }
            }
            // Close span at end of row if needed
            if (spanStart != -1) {
                rowSpans[y].push_back({spanStart, width - 1});
            }
        }

        // Group consecutive rows with similar widths into rectangles
        int startY = 0;
        while (startY < height) {
            // Skip empty rows
            if (rowSpans[startY].empty()) {
                startY++;
                continue;
            }
            
            // Process each span in the starting row
            for (size_t spanIdx = 0; spanIdx < rowSpans[startY].size(); spanIdx++) {
                const auto& startSpan = rowSpans[startY][spanIdx];
                int minX = startSpan.minX;
                int maxX = startSpan.maxX;
                int currentWidth = maxX - minX + 1;
                int endY = startY;
                
                // Try to extend downward, but only if width is very similar
                for (int y = startY + 1; y < height; y++) {
                    if (rowSpans[y].empty()) break;
                    
                    // Find matching span in this row
                    bool foundMatch = false;
                    for (const auto& span : rowSpans[y]) {
                        int spanWidth = span.maxX - span.minX + 1;
                        int widthDiff = std::abs(spanWidth - currentWidth);
                        int centerThis = (minX + maxX) / 2;
                        int centerSpan = (span.minX + span.maxX) / 2;
                        int centerDiff = std::abs(centerThis - centerSpan);
                        
                        // Only merge if width is similar (within 20%) and centers align (within 5 pixels)
                        if (widthDiff <= currentWidth / 5 && centerDiff <= 5) {
                            foundMatch = true;
                            minX = std::min(minX, span.minX);
                            maxX = std::max(maxX, span.maxX);
                            endY = y;
                            break;
                        }
                    }
                    
                    if (!foundMatch) break;
                }
                
                // Calculate rectangle area
                int area = (maxX - minX + 1) * (endY - startY + 1);
                
                // Only create rectangle if it's large enough
                if (area >= minClusterSize) {
                    Rectangle rect;
                    rect.begin = {static_cast<float>(minX), static_cast<float>(startY)};
                    rect.end = {static_cast<float>(maxX + 1), static_cast<float>(endY + 1)};
                    hitboxes.push_back(rect);
                }
            }
            
            startY++;
        }

        // Unlock surface
        if (SDL_MUSTLOCK(surface)) {
            SDL_UnlockSurface(surface);
        }
        
        return hitboxes;
    }

    

    ICollidable(Vector2 pos,Vector2 sizeMultiplier,const char* spritePath, SDL_Renderer* renderer,bool isComplex,int zIndex = 0, int minClusterSize = 50)
        : GameObject(pos, sizeMultiplier, spritePath, renderer, zIndex)
    {
        this->isComplex = isComplex;
        this->originalSurfaceWidth = 0.0f;
        this->originalSurfaceHeight = 0.0f;

        if (isComplex) {
            SDL_Surface* surface = IMG_Load(spritePath);
            if (!surface) {
                std::cerr << "Failed to load surface for hitbox detection: " << SDL_GetError() << "\n";
                return;
            }

            this->originalSurfaceWidth = static_cast<float>(surface->w);
            this->originalSurfaceHeight = static_cast<float>(surface->h);
            collisionRectangles = autoDetectHitboxes(surface, minClusterSize);
            SDL_FreeSurface(surface);
        }
    }

    
    std::vector<Rectangle> getCollisionBox(){
        Vector2* pos = this->getPosition();
        Vector2* size = this->getSize();
        
        if(!isComplex){
            return {Rectangle{*pos, {pos->x + size->x, pos->y + size->y}}};
        }
        
        // Transform collision rectangles to world coordinates
        std::vector<Rectangle> transformed;
        transformed.reserve(collisionRectangles.size());
        
        // Get the original surface dimensions to calculate scale
        Vector2 scale = {size->x / originalSurfaceWidth, size->y / originalSurfaceHeight};
        
        for (const auto& rect : collisionRectangles) {
            Rectangle worldRect;
            worldRect.begin = {
                pos->x + rect.begin.x * scale.x,
                pos->y + rect.begin.y * scale.y
            };
            worldRect.end = {
                pos->x + rect.end.x * scale.x,
                pos->y + rect.end.y * scale.y
            };
            transformed.push_back(worldRect);
        }
        
        return transformed;
    } 
    
    virtual void onCollisionEnter(ICollidable* other){
        // To be implemented in subclasses
    }

    virtual void onCollisionLeave(ICollidable* other){
        // To be implemented in subclasses
    }

    virtual void onCollisionStay(ICollidable* other){
        // To be implemented in subclasses
    }
    
};


