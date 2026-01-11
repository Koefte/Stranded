#pragma once

struct Vector2{
    float x;
    float y;

    Vector2 multiply(Vector2 other) const {
        return {x * other.x, y * other.y};
    }
    float dist(const Vector2& other) const {
        float dx = other.x - x;
        float dy = other.y - y;
        return sqrtf(dx * dx + dy * dy);
    }

    // Operator overloads
    Vector2 operator+(const Vector2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    Vector2 operator-(const Vector2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    Vector2 operator*(const Vector2& rhs) const { return {x * rhs.x, y * rhs.y}; }
    Vector2 operator/(const Vector2& rhs) const { return {x / rhs.x, y / rhs.y}; }
    Vector2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    Vector2 operator/(float scalar) const { return {x / scalar, y / scalar}; }
    Vector2& operator+=(const Vector2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    Vector2& operator-=(const Vector2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vector2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }
};

// Scalar * Vector2
inline Vector2 operator*(float scalar, const Vector2& v) { return {v.x * scalar, v.y * scalar}; }