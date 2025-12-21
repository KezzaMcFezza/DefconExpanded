#include <cmath>
#include <string.h>
#include "math_utils.h"
#include "Matrix4f.h"

void Matrix4f::Perspective(float fovy, float aspect, float nearZ, float farZ) {
    float rad = fovy * M_PI / 180.0f;
    float f = 1.0f / tanf(rad / 2.0f);
    
    LoadIdentity();
    
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (farZ + nearZ) / (nearZ - farZ);
    m[11] = -1.0f;
    m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    m[15] = 0.0f;
}

void Matrix4f::Ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
    LoadIdentity();
    
    float tx = -(right + left) / (right - left);
    float ty = -(top + bottom) / (top - bottom);
    float tz = -(farZ + nearZ) / (farZ - nearZ);
    
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -2.0f / (farZ - nearZ);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}

void Matrix4f::LookAt(float eyeX, float eyeY, float eyeZ,
                        float centerX, float centerY, float centerZ,
                        float upX, float upY, float upZ) {
    
    //
    // Calculate look direction (from eye to center)
    
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    Normalize(fx, fy, fz);
    
    //
    // Calculate right vector (forward cross up)
    
    float sx, sy, sz;
    Cross(fx, fy, fz, upX, upY, upZ, sx, sy, sz);
    Normalize(sx, sy, sz);
    
    //
    // Calculate up vector (right cross forward)
    
    float ux, uy, uz;
    Cross(sx, sy, sz, fx, fy, fz, ux, uy, uz);
    
    //
    // Build view matrix exactly like gluLookAt (column-major order)
    
    LoadIdentity();
    
    // Column 0
    m[0] = sx;
    m[1] = ux; 
    m[2] = -fx;
    m[3] = 0.0f;
    
    // Column 1
    m[4] = sy;
    m[5] = uy;
    m[6] = -fy;
    m[7] = 0.0f;
    
    // Column 2
    m[8] = sz;
    m[9] = uz;
    m[10] = -fz;
    m[11] = 0.0f;
    
    // Column 3 (translation)
    m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    m[14] = -(-fx * eyeX + -fy * eyeY + -fz * eyeZ);
    m[15] = 1.0f;
}

void Matrix4f::Copy(float* dest) const {
    memcpy(dest, m, sizeof(m));
}

void Matrix4f::Multiply(const float* a, const float* b, float* result) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
}

void Matrix4f::TransformVertex(const float* matrix, float* vertex) {
    float x = vertex[0];
    float y = vertex[1];
    float z = vertex[2];
    
    vertex[0] = matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12];
    vertex[1] = matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13];
    vertex[2] = matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14];
}

void Matrix4f::TransformNormal(const float* matrix, float* normal) {
    float x = normal[0];
    float y = normal[1];
    float z = normal[2];
    
    normal[0] = matrix[0] * x + matrix[4] * y + matrix[8] * z;
    normal[1] = matrix[1] * x + matrix[5] * y + matrix[9] * z;
    normal[2] = matrix[2] * x + matrix[6] * y + matrix[10] * z;
    
    float length = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length > 0.0f) {
        normal[0] /= length;
        normal[1] /= length;
        normal[2] /= length;
    }
}

void Matrix4f::BuildTransformMatrix(const std::vector<double>& translation,
                                      const std::vector<double>& rotation,
                                      const std::vector<double>& scale,
                                      const std::vector<double>& matrix,
                                      float* outMatrix)
{
    //
    // If matrix is provided, use it directly

    if (matrix.size() == 16) {
        for (int i = 0; i < 16; i++) {
            outMatrix[i] = (float)matrix[i];
        }
        return;
    }
    
    //
    // Otherwise build from TRS (Translation, Rotation, Scale)

    float T[3] = {0, 0, 0};
    float R[4] = {0, 0, 0, 1}; // quaternion (x, y, z, w)
    float S[3] = {1, 1, 1};
    
    if (translation.size() == 3) {
        T[0] = (float)translation[0];
        T[1] = (float)translation[1];
        T[2] = (float)translation[2];
    }
    
    if (rotation.size() == 4) {
        R[0] = (float)rotation[0];
        R[1] = (float)rotation[1];
        R[2] = (float)rotation[2];
        R[3] = (float)rotation[3];
    }
    
    if (scale.size() == 3) {
        S[0] = (float)scale[0];
        S[1] = (float)scale[1];
        S[2] = (float)scale[2];
    }
    
    //
    // Convert quaternion to rotation matrix

    float x = R[0], y = R[1], z = R[2], w = R[3];
    float x2 = x * x, y2 = y * y, z2 = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;
    
    //
    // Build combined TRS matrix

    outMatrix[0] = S[0] * (1 - 2 * (y2 + z2));
    outMatrix[1] = S[0] * (2 * (xy + wz));
    outMatrix[2] = S[0] * (2 * (xz - wy));
    outMatrix[3] = 0;
    
    outMatrix[4] = S[1] * (2 * (xy - wz));
    outMatrix[5] = S[1] * (1 - 2 * (x2 + z2));
    outMatrix[6] = S[1] * (2 * (yz + wx));
    outMatrix[7] = 0;
    
    outMatrix[8] = S[2] * (2 * (xz + wy));
    outMatrix[9] = S[2] * (2 * (yz - wx));
    outMatrix[10] = S[2] * (1 - 2 * (x2 + y2));
    outMatrix[11] = 0;
    
    outMatrix[12] = T[0];
    outMatrix[13] = T[1];
    outMatrix[14] = T[2];
    outMatrix[15] = 1;
}

void Matrix4f::Normalize(float& x, float& y, float& z) {
    float length = sqrtf(x * x + y * y + z * z);
    if (length > 0.0f) {
        x /= length;
        y /= length;
        z /= length;
    }
}
