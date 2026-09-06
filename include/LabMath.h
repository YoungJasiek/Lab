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

        static Vec3 transformPoint(const Mat4& mat, const Vec3& p) {
            float x = mat.m[0] * p.x + mat.m[4] * p.y + mat.m[8] * p.z + mat.m[12];
            float y = mat.m[1] * p.x + mat.m[5] * p.y + mat.m[9] * p.z + mat.m[13];
            float z = mat.m[2] * p.x + mat.m[6] * p.y + mat.m[10] * p.z + mat.m[14];
            float w = mat.m[3] * p.x + mat.m[7] * p.y + mat.m[11] * p.z + mat.m[15];
            if (std::abs(w) > 1e-6f && std::abs(w - 1.0f) > 1e-6f) {
                float invW = 1.0f / w;
                return { x * invW, y * invW, z * invW };
            }
            return { x, y, z };
        }

        static Vec3 transformVector(const Mat4& mat, const Vec3& v) {
            return {
                mat.m[0] * v.x + mat.m[4] * v.y + mat.m[8] * v.z,
                mat.m[1] * v.x + mat.m[5] * v.y + mat.m[9] * v.z,
                mat.m[2] * v.x + mat.m[6] * v.y + mat.m[10] * v.z
            };
        }

        Mat4 inverse() const {
            float inv[16], det;
            const float* src = m;

            inv[0] = src[5] * src[10] * src[15] - src[5] * src[11] * src[14] - src[9] * src[6] * src[15] + src[9] * src[7] * src[14] + src[13] * src[6] * src[11] - src[13] * src[7] * src[10];
            inv[4] = -src[4] * src[10] * src[15] + src[4] * src[11] * src[14] + src[8] * src[6] * src[15] - src[8] * src[7] * src[14] - src[12] * src[6] * src[11] + src[12] * src[7] * src[10];
            inv[8] = src[4] * src[9] * src[15] - src[4] * src[11] * src[13] - src[8] * src[5] * src[15] + src[8] * src[7] * src[13] + src[12] * src[5] * src[11] - src[12] * src[7] * src[9];
            inv[12] = -src[4] * src[9] * src[14] + src[4] * src[10] * src[13] + src[8] * src[5] * src[14] - src[8] * src[6] * src[13] - src[12] * src[5] * src[10] + src[12] * src[6] * src[9];
            inv[1] = -src[1] * src[10] * src[15] + src[1] * src[11] * src[14] + src[9] * src[2] * src[15] - src[9] * src[3] * src[14] - src[13] * src[2] * src[11] + src[13] * src[3] * src[10];
            inv[5] = src[0] * src[10] * src[15] - src[0] * src[11] * src[14] - src[8] * src[2] * src[15] + src[8] * src[3] * src[14] + src[12] * src[2] * src[11] - src[12] * src[3] * src[10];
            inv[9] = -src[0] * src[9] * src[15] + src[0] * src[11] * src[13] + src[8] * src[1] * src[15] - src[8] * src[3] * src[13] - src[12] * src[1] * src[11] + src[12] * src[3] * src[9];
            inv[13] = src[0] * src[9] * src[14] - src[0] * src[10] * src[13] - src[8] * src[1] * src[14] + src[8] * src[2] * src[13] + src[12] * src[1] * src[10] - src[12] * src[2] * src[9];
            inv[2] = src[1] * src[6] * src[15] - src[1] * src[7] * src[14] - src[5] * src[2] * src[15] + src[5] * src[3] * src[14] + src[13] * src[2] * src[7] - src[13] * src[3] * src[6];
            inv[6] = -src[0] * src[6] * src[15] + src[0] * src[7] * src[14] + src[4] * src[2] * src[15] - src[4] * src[3] * src[14] - src[12] * src[2] * src[7] + src[12] * src[3] * src[6];
            inv[10] = src[0] * src[5] * src[15] - src[0] * src[7] * src[13] - src[4] * src[1] * src[15] + src[4] * src[3] * src[13] + src[12] * src[1] * src[7] - src[12] * src[3] * src[5];
            inv[14] = -src[0] * src[5] * src[14] + src[0] * src[6] * src[13] + src[4] * src[1] * src[14] - src[4] * src[2] * src[13] - src[12] * src[1] * src[6] + src[12] * src[2] * src[5];
            inv[3] = -src[1] * src[6] * src[11] + src[1] * src[7] * src[10] + src[5] * src[2] * src[11] - src[5] * src[3] * src[10] - src[9] * src[2] * src[7] + src[9] * src[3] * src[6];
            inv[7] = src[0] * src[6] * src[11] - src[0] * src[7] * src[10] - src[4] * src[2] * src[11] + src[4] * src[3] * src[10] + src[8] * src[2] * src[7] - src[8] * src[3] * src[6];
            inv[11] = -src[0] * src[5] * src[11] + src[0] * src[7] * src[9] + src[4] * src[1] * src[11] - src[4] * src[3] * src[9] - src[8] * src[1] * src[7] + src[8] * src[3] * src[5];
            inv[15] = src[0] * src[5] * src[10] - src[0] * src[6] * src[9] - src[4] * src[1] * src[10] + src[4] * src[2] * src[9] + src[8] * src[1] * src[6] - src[8] * src[2] * src[5];

            det = src[0] * inv[0] + src[1] * inv[4] + src[2] * inv[8] + src[3] * inv[12];
            Mat4 res;
            if (std::abs(det) < 1e-8f) return res; // return identity if singular

            float invDet = 1.0f / det;
            for (int i = 0; i < 16; ++i) {
                res.m[i] = inv[i] * invDet;
            }
            return res;
        }
    };

    struct Quat {
        float x, y, z, w;

        Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
        Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

        static Quat identity() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }

        static Quat fromAxisAngle(const Vec3& axis, float angleRad) {
            Vec3 normAxis = axis.normalized();
            float halfAngle = angleRad * 0.5f;
            float s = std::sin(halfAngle);
            return { normAxis.x * s, normAxis.y * s, normAxis.z * s, std::cos(halfAngle) };
        }

        static Quat fromEuler(float pitchRad, float yawRad, float rollRad) {
            // Yaw (Y), Pitch (X), Roll (Z)
            float cy = std::cos(yawRad * 0.5f);
            float sy = std::sin(yawRad * 0.5f);
            float cp = std::cos(pitchRad * 0.5f);
            float sp = std::sin(pitchRad * 0.5f);
            float cr = std::cos(rollRad * 0.5f);
            float sr = std::sin(rollRad * 0.5f);

            return {
                sr * cp * cy - cr * sp * sy,
                cr * sp * cy + sr * cp * sy,
                cr * cp * sy - sr * sp * cy,
                cr * cp * cy + sr * sp * sy
            };
        }

        static float dot(const Quat& a, const Quat& b) {
            return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        }

        float lengthSq() const { return x * x + y * y + z * z + w * w; }
        float length() const { return std::sqrt(lengthSq()); }

        Quat normalized() const {
            float len = length();
            return (len > 1e-6f) ? Quat(x / len, y / len, z / len, w / len) : identity();
        }

        Quat conjugate() const { return { -x, -y, -z, w }; }
        Quat inverse() const {
            float lenSq = lengthSq();
            return (lenSq > 1e-6f) ? Quat(-x / lenSq, -y / lenSq, -z / lenSq, w / lenSq) : identity();
        }

        Vec3 rotateVector(const Vec3& v) const {
            Vec3 qvec(x, y, z);
            Vec3 uv = Vec3::cross(qvec, v);
            Vec3 uuv = Vec3::cross(qvec, uv);
            return v + ((uv * w) + uuv) * 2.0f;
        }

        Quat operator+(const Quat& q) const { return { x + q.x, y + q.y, z + q.z, w + q.w }; }
        Quat operator-(const Quat& q) const { return { x - q.x, y - q.y, z - q.z, w - q.w }; }
        Quat operator-() const { return { -x, -y, -z, -w }; }
        Quat operator*(float s) const { return { x * s, y * s, z * s, w * s }; }

        Quat operator*(const Quat& q) const {
            return {
                w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w,
                w * q.w - x * q.x - y * q.y - z * q.z
            };
        }

        static Quat slerp(const Quat& a, const Quat& b, float t) {
            Quat q1 = a.normalized();
            Quat q2 = b.normalized();
            float cosTheta = Quat::dot(q1, q2);

            if (cosTheta < 0.0f) {
                q2 = -q2;
                cosTheta = -cosTheta;
            }

            if (cosTheta > 0.9995f) {
                Quat res = q1 + (q2 - q1) * t;
                return res.normalized();
            }

            float angle = std::acos(cosTheta);
            float sinAngle = std::sin(angle);
            float w1 = std::sin((1.0f - t) * angle) / sinAngle;
            float w2 = std::sin(t * angle) / sinAngle;
            return (q1 * w1 + q2 * w2).normalized();
        }

        Mat4 toMat4() const {
            Quat q = normalized();
            Mat4 res;
            float xx = q.x * q.x;
            float yy = q.y * q.y;
            float zz = q.z * q.z;
            float xy = q.x * q.y;
            float xz = q.x * q.z;
            float yz = q.y * q.z;
            float wx = q.w * q.x;
            float wy = q.w * q.y;
            float wz = q.w * q.z;

            res.m[0] = 1.0f - 2.0f * (yy + zz);
            res.m[1] = 2.0f * (xy + wz);
            res.m[2] = 2.0f * (xz - wy);
            res.m[3] = 0.0f;

            res.m[4] = 2.0f * (xy - wz);
            res.m[5] = 1.0f - 2.0f * (xx + zz);
            res.m[6] = 2.0f * (yz + wx);
            res.m[7] = 0.0f;

            res.m[8] = 2.0f * (xz + wy);
            res.m[9] = 2.0f * (yz - wx);
            res.m[10] = 1.0f - 2.0f * (xx + yy);
            res.m[11] = 0.0f;

            res.m[12] = 0.0f;
            res.m[13] = 0.0f;
            res.m[14] = 0.0f;
            res.m[15] = 1.0f;
            return res;
        }
    };

    inline Mat4 makeTransform(const Vec3& translation, const Quat& rotation, const Vec3& scale) {
        Mat4 rotM = rotation.toMat4();
        Mat4 res;
        res.m[0] = rotM.m[0] * scale.x;
        res.m[1] = rotM.m[1] * scale.x;
        res.m[2] = rotM.m[2] * scale.x;
        res.m[3] = 0.0f;

        res.m[4] = rotM.m[4] * scale.y;
        res.m[5] = rotM.m[5] * scale.y;
        res.m[6] = rotM.m[6] * scale.y;
        res.m[7] = 0.0f;

        res.m[8] = rotM.m[8] * scale.z;
        res.m[9] = rotM.m[9] * scale.z;
        res.m[10] = rotM.m[10] * scale.z;
        res.m[11] = 0.0f;

        res.m[12] = translation.x;
        res.m[13] = translation.y;
        res.m[14] = translation.z;
        res.m[15] = 1.0f;
        return res;
    }
}

