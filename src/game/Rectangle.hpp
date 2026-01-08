#pragma once
#include "Vector2.hpp"

struct Rectangle{
    Vector2 begin;
    Vector2 end;
    bool intersects(const Rectangle& other){
        return !(end.x < other.begin.x || 
                 begin.x > other.end.x || 
                 end.y < other.begin.y || 
                 begin.y > other.end.y);
    }
};