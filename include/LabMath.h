#pragma once
#include <cmath>
#include <iostream>

namespace Lab {

    struct Vec2 {
        float x, y;
        Vec2(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
    };

    struct Vec4 {
        float x, y, z, w;
        Vec4(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 0.0f) : x(x), y(y), z(z), w(w) {}
    };

    struct Vec3 {
        float x, y, z;

        Vec3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}

        Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
        Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
        Vec3 operator-() const { return { -x, -y, -z }; }
        Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
        Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }

        Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
        Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
        Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

        float lengthSq() const { return x * x + y * y + z * z; }
        float length() const { return std::sqrt(lengthSq()); }

        Vec3 normalized() const {
            float len = length();
            return (len > 0) ? *this / len : Vec3(0, 0, 0);
        }

        static float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        static Vec3 cross(const Vec3& a, const Vec3& b) {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        static Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
    };

    struct Mat4 {
        float m[16];

        Mat4() {
            for (int i = 0; i < 16; ++i) m[i] = 0.0f;
            m[0] = m[5] = m[10] = m[15] = 1.0f;
        }

        static Mat4 identity() { return Mat4(); }

        static Mat4 perspective(float fov, float aspect, float near, float far) {
            Mat4 res;
            float tanHalfFov = std::tan(fov / 2.0f);
            res.m[0] = 1.0f / (aspect * tanHalfFov);
            res.m[5] = 1.0f / tanHalfFov;
            res.m[10] = -(far + near) / (far - near);
            res.m[11] = -1.0f;
            res.m[14] = -(2.0f * far * near) / (far - near);
            res.m[15] = 0.0f;
            return res;
        }

        static Mat4 ortho(float left, float right, float bottom, float top, float near, float far) {
            Mat4 res;
            res.m[0] = 2.0f / (right - left);
            res.m[5] = 2.0f / (top - bottom);
            res.m[10] = -2.0f / (far - near);
            res.m[12] = -(right + left) / (right - left);
            res.m[13] = -(top + bottom) / (top - bottom);
            res.m[14] = -(far + near) / (far - near);
            res.m[15] = 1.0f;
            return res;
        }

        static Mat4 translate(const Vec3& v) {
            Mat4 res;
            res.m[12] = v.x;
            res.m[13] = v.y;
            res.m[14] = v.z;
            return res;
        }

        static Mat4 rotate(float angle, const Vec3& axis) {
            Mat4 res;
            float c = std::cos(angle);
            float s = std::sin(angle);
            float t = 1.0f - c;
            Vec3 a = axis.normalized();

            res.m[0] = t * a.x * a.x + c;
            res.m[1] = t * a.x * a.y + s * a.z;
            res.m[2] = t * a.x * a.z - s * a.y;
            res.m[4] = t * a.x * a.y - s * a.z;
            res.m[5] = t * a.y * a.y + c;
            res.m[6] = t * a.y * a.z + s * a.x;
            res.m[8] = t * a.x * a.z + s * a.y;
            res.m[9] = t * a.y * a.z - s * a.x;
            res.m[10] = t * a.z * a.z + c;
            return res;
        }

        static Mat4 scale(const Vec3& s) {
            Mat4 res;
            res.m[0] = s.x;
            res.m[5] = s.y;
            res.m[10] = s.z;
            return res;
        }
        
        static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
            Vec3 f = (center - eye).normalized();
            Vec3 s = Vec3::cross(f, up).normalized();
            Vec3 u = Vec3::cross(s, f);

            Mat4 res;
            res.m[0] = s.x; res.m[4] = s.y; res.m[8] = s.z;
            res.m[1] = u.x; res.m[5] = u.y; res.m[9] = u.z;
            res.m[2] = -f.x; res.m[6] = -f.y; res.m[10] = -f.z;
            res.m[12] = -Vec3::dot(s, eye);
            res.m[13] = -Vec3::dot(u, eye);
            res.m[14] = Vec3::dot(f, eye);
            return res;
        }

        Mat4 operator*(const Mat4& right) const {
            Mat4 res;
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    float sum = 0.0f;
                    for (int i = 0; i < 4; ++i) {
                        sum += m[i * 4 + r] * right.m[c * 4 + i];
                    }
                    res.m[c * 4 + r] = sum;
                }
            }
            return res;
        }
    };
}
