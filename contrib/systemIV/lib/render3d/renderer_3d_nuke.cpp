#include "lib/universal_include.h"

#include <math.h>

#include "renderer_3d.h"
#include "lib/resource/image.h"
#include "lib/render/colour.h"

//
// creates proper 3D missile geometry instead of flat sprites

void Renderer3D::CreateNukeModel3D(const Vector3<float>& position, const Vector3<float>& direction, 
                                   float length, float radius, Colour const &col, Vertex3D* vertices, int& vertexCount) {
    vertexCount = 0;
    
    Vector3<float> forward = direction;
    forward.Normalise();
    
    Vector3<float> up = Vector3<float>(0.0f, 1.0f, 0.0f);
    if (fabsf(forward.y) > 0.9f) {
        up = Vector3<float>(1.0f, 0.0f, 0.0f);
    }
    
    Vector3<float> right = up ^ forward;
    right.Normalise();
    up = forward ^ right;
    up.Normalise();
    
    // convert color to float
    float r = col.m_r / 255.0f;
    float g = col.m_g / 255.0f;
    float b = col.m_b / 255.0f;
    float a = col.m_a / 255.0f;
    
    // model proportions
    float noseLength = length * 0.3f;      // 30% nose cone
    float bodyLength = length * 0.6f;      // 60% cylindrical body  
    float tailLength = length * 0.1f;      // 10% tapered tail
    float bodyRadius = radius;
    float tailRadius = radius * 0.7f;      // Slightly tapered tail
    
    // Create model sections
    int segments = 8;  // 8-sided 
    
    // nose cone
    Vector3<float> noseTip = position + forward * (length * 0.5f);
    Vector3<float> noseBase = position + forward * (length * 0.5f - noseLength);
    
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * 2.0f * M_PI;
        float angle2 = (float)(i + 1) / segments * 2.0f * M_PI;
        
        Vector3<float> p1 = noseBase + right * (bodyRadius * cosf(angle1)) + up * (bodyRadius * sinf(angle1));
        Vector3<float> p2 = noseBase + right * (bodyRadius * cosf(angle2)) + up * (bodyRadius * sinf(angle2));
        
        // triangle from tip to base edge
        vertices[vertexCount++] = Vertex3D(noseTip.x, noseTip.y, noseTip.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(p1.x, p1.y, p1.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(p2.x, p2.y, p2.z, r, g, b, a);
    }
    
    // cylindrical body
    Vector3<float> tailBase = position + forward * (length * 0.5f - noseLength - bodyLength);
    
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * 2.0f * M_PI;
        float angle2 = (float)(i + 1) / segments * 2.0f * M_PI;
        
        Vector3<float> front1 = noseBase + right * (bodyRadius * cosf(angle1)) + up * (bodyRadius * sinf(angle1));
        Vector3<float> front2 = noseBase + right * (bodyRadius * cosf(angle2)) + up * (bodyRadius * sinf(angle2));
        Vector3<float> back1 = tailBase + right * (bodyRadius * cosf(angle1)) + up * (bodyRadius * sinf(angle1));
        Vector3<float> back2 = tailBase + right * (bodyRadius * cosf(angle2)) + up * (bodyRadius * sinf(angle2));
        
        // two triangles per segment to form rectangle
        vertices[vertexCount++] = Vertex3D(front1.x, front1.y, front1.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(back1.x, back1.y, back1.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(front2.x, front2.y, front2.z, r, g, b, a);
        
        vertices[vertexCount++] = Vertex3D(front2.x, front2.y, front2.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(back1.x, back1.y, back1.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(back2.x, back2.y, back2.z, r, g, b, a);
    }
    
    // tapered tail
    Vector3<float> tailTip = position - forward * (length * 0.5f);
    
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / segments * 2.0f * M_PI;
        float angle2 = (float)(i + 1) / segments * 2.0f * M_PI;
        
        Vector3<float> base1 = tailBase + right * (bodyRadius * cosf(angle1)) + up * (bodyRadius * sinf(angle1));
        Vector3<float> base2 = tailBase + right * (bodyRadius * cosf(angle2)) + up * (bodyRadius * sinf(angle2));
        Vector3<float> tip1 = tailTip + right * (tailRadius * cosf(angle1)) + up * (tailRadius * sinf(angle1));
        Vector3<float> tip2 = tailTip + right * (tailRadius * cosf(angle2)) + up * (tailRadius * sinf(angle2));
        
        // two triangles per segment
        vertices[vertexCount++] = Vertex3D(base1.x, base1.y, base1.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(tip1.x, tip1.y, tip1.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(base2.x, base2.y, base2.z, r, g, b, a);
        
        vertices[vertexCount++] = Vertex3D(base2.x, base2.y, base2.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(tip1.x, tip1.y, tip1.z, r, g, b, a);
        vertices[vertexCount++] = Vertex3D(tip2.x, tip2.y, tip2.z, r, g, b, a);
    }
}

void Renderer3D::Nuke3DModel(const Vector3<float>& position, const Vector3<float>& direction, 
                             float length, float radius, Colour const &col) {
    const int VERTICES_PER_NUKE = 144;
    
    FlushNuke3DModels3DIfFull(VERTICES_PER_NUKE);
    
    Vertex3D nukeVertices[VERTICES_PER_NUKE];
    int nukeVertexCount = 0;
    
    CreateNukeModel3D(position, direction, length, radius, col, nukeVertices, nukeVertexCount);
    
    // add to buffer
    for (int i = 0; i < nukeVertexCount && m_nuke3DModelVertexCount3D < MAX_NUKE_3D_MODEL_VERTICES_3D; i++) {
        m_nuke3DModelVertices3D[m_nuke3DModelVertexCount3D++] = nukeVertices[i];
    }
}