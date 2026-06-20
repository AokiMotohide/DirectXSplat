#pragma once

#include <array>
#include <cstdint>

namespace dxsplat {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct Vec4 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 0.0f;
};

struct Quat {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;
};

struct Mat3 {
  std::array<float, 9> m{};
};

struct Mat4 {
  std::array<float, 16> m{};
};

Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& a, float s);
Vec3 operator/(const Vec3& a, float s);
Vec3 Min(const Vec3& a, const Vec3& b);
Vec3 Max(const Vec3& a, const Vec3& b);
float Dot(const Vec3& a, const Vec3& b);
Vec3 Cross(const Vec3& a, const Vec3& b);
float Length(const Vec3& v);
Vec3 Normalize(const Vec3& v);

Quat Normalize(const Quat& q);
Mat3 QuatToMat3(const Quat& q);
Mat3 Diagonal3(const Vec3& d);
Mat3 Mat3Mul(const Mat3& a, const Mat3& b);
Mat3 Mat3Transpose(const Mat3& a);

Mat4 Identity4();
Mat4 Mul(const Mat4& a, const Mat4& b);
Vec4 Mul(const Mat4& m, const Vec4& v);
Mat4 Perspective(float fovYRadians, float aspect, float zNear, float zFar);
Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up);
Mat4 Inverse(const Mat4& m);

float Clamp(float v, float lo, float hi);

Mat3 BuildCovariance(const Vec3& scale, const Quat& rotation);
Mat3 ProjectCovarianceToScreen(const Mat3& covariance,
                               const Vec3& positionView,
                               float focalX,
                               float focalY);

}  // namespace dxsplat
