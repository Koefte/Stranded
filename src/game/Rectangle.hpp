#pragma once
#include "Vector2.hpp"

struct Rectangle{
    Vector2 begin;
    Vector2 end;
    float dist(const Rectangle& other) const{
        float dx = 0.0f;
        if(end.x < other.begin.x){
            dx = other.begin.x - end.x;
        } else if(begin.x > other.end.x){
            dx = begin.x - other.end.x;
        }

        float dy = 0.0f;
        if(end.y < other.begin.y){
            dy = other.begin.y - end.y;
        } else if(begin.y > other.end.y){
            dy = begin.y - other.end.y;
        }

        return sqrtf(dx * dx + dy * dy);
    }
    bool intersects(const Rectangle& other) const{
        return !(end.x < other.begin.x || 
                 begin.x > other.end.x || 
                 end.y < other.begin.y || 
                 begin.y > other.end.y);
    }
};