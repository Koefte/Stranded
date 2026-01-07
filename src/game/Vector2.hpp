#pragma once

struct Vector2{
    float x;
    float y;
    Vector2 multiply(Vector2 other){
        return {x * other.x, y * other.y};
    }
};