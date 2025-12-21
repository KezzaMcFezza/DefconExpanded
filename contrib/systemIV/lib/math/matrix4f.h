#ifndef _included_matrix4f_h
#define _included_matrix4f_h

#include <vector>

class Matrix4f {
public:
    float m[16];
    
    constexpr Matrix4f() : m{1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1} {}
    constexpr void LoadIdentity();
    void Perspective        (float fovy, float aspect, float nearZ, float farZ);
    void Ortho              (float left, float right, float bottom, float top, float nearZ, float farZ);
    void LookAt             (float eyeX, float eyeY, float eyeZ,
                             float centerX, float centerY, float centerZ,
                             float upX, float upY, float upZ);
    constexpr void Multiply (const Matrix4f& other);
    void Copy               (float* dest) const;
    
    static void Multiply            (const float* a, const float* b, float* result);
    static void TransformVertex     (const float* matrix, float* vertex);
    static void TransformNormal     (const float* matrix, float* normal);
    static void BuildTransformMatrix(const std::vector<double>& translation,
                                     const std::vector<double>& rotation,
                                     const std::vector<double>& scale,
                                     const std::vector<double>& matrix,
                                     float* outMatrix);
    
private:
    void Normalize          (float& x, float& y, float& z);
    constexpr void Cross    (float ax, float ay, float az, float bx, float by, float bz, 
                             float& cx, float& cy, float& cz);
};

constexpr void Matrix4f::LoadIdentity() {
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
    m[4] = 0.0f; m[5] = 1.0f; m[6] = 0.0f; m[7] = 0.0f;
    m[8] = 0.0f; m[9] = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

constexpr void Matrix4f::Multiply(const Matrix4f& other) {
    Matrix4f result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += m[i * 4 + k] * other.m[k * 4 + j];
            }
        }
    }
    *this = result;
}

constexpr void Matrix4f::Cross(float ax, float ay, float az, float bx, float by, float bz, float& cx, float& cy, float& cz) {
    cx = ay * bz - az * by;
    cy = az * bx - ax * bz;
    cz = ax * by - ay * bx;
}

#endif
