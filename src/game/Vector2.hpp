#pragma once

struct Vector2{
    float x;
    float y;
    Vector2 multiply(Vector2 other){
        return {x * other.x, y * other.y};
    }
    float dist(Vector2* other){
        float dx = other->x - x;
        float dy = other->y - y;
        return sqrtf(dx * dx + dy * dy);
    }

    float dist(Vector2 other){
        float dx = other.x - x;
        float dy = other.y - y;
        return sqrtf(dx * dx + dy * dy);
    }
};