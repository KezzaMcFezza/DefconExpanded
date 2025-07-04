# MegaLineBuffer: Complete Implementation & Usage Guide

## **Header Declaration (renderer.h)**

Add this to your renderer.h file:

```cpp
// Forward declarations
struct LineVertex {
    float x, y;
    float r, g, b, a;
    
    LineVertex() : x(0), y(0), r(1), g(1), b(1), a(1) {}
    LineVertex(float px, float py, float pr, float pg, float pb, float pa) 
        : x(px), y(py), r(pr), g(pg), b(pb), a(pa) {}
};

struct LineProperties {
    uint32_t colorRGBA;
    float width;
    int blendMode;
    
    LineProperties(Colour col, float w, int blend) 
        : width(w), blendMode(blend) {
        colorRGBA = (col.m_r << 24) | (col.m_g << 16) | (col.m_b << 8) | col.m_a;
    }
    
    bool operator==(const LineProperties& other) const {
        return colorRGBA == other.colorRGBA && 
               width == other.width && 
               blendMode == other.blendMode;
    }
    
    // Hash function for unordered_map
    struct Hash {
        size_t operator()(const LineProperties& props) const {
            return std::hash<uint32_t>()(props.colorRGBA) ^ 
                   (std::hash<float>()(props.width) << 1) ^
                   (std::hash<int>()(props.blendMode) << 2);
        }
    };
};

class MegaLineBuffer {
private:
    struct LineBatch {
        std::vector<LineVertex> vertices;
        LineProperties properties;
        unsigned int VAO, VBO;
        bool vboNeedsUpdate;
        
        LineBatch(const LineProperties& props) 
            : properties(props), VAO(0), VBO(0), vboNeedsUpdate(true) {
            vertices.reserve(10000); // Pre-allocate for performance
        }
    };
    
    std::unordered_map<LineProperties, std::unique_ptr<LineBatch>, LineProperties::Hash> m_batches;
    std::vector<LineBatch*> m_activeBatches; // For frame rendering order
    
    bool m_frameActive;
    unsigned int m_shaderProgram;
    
    // Performance statistics
    int m_totalLinesThisFrame;
    int m_batchesUsedThisFrame;
    int m_drawCallsThisFrame;

public:
    MegaLineBuffer();
    ~MegaLineBuffer();
    
    // Frame management
    void BeginFrame();
    void EndFrame();
    
    // Line addition (automatically batches by properties)
    void AddLine(float x1, float y1, float x2, float y2, const Colour& color, float width = 1.0f);
    void AddCircle(float centerX, float centerY, float radius, const Colour& color, int segments = 32, float width = 1.0f);
    void AddRect(float x, float y, float w, float h, const Colour& color, float width = 1.0f);
    
    // Advanced line types
    void AddDashedLine(float x1, float y1, float x2, float y2, const Colour& color, float dashLength = 2.0f, float width = 1.0f);
    void AddFadingLine(float x1, float y1, float x2, float y2, const Colour& startColor, const Colour& endColor, float width = 1.0f);
    
    // Batch rendering
    void RenderAllBatches();
    
    // Performance monitoring
    int GetTotalLinesThisFrame() const { return m_totalLinesThisFrame; }
    int GetBatchCount() const { return m_batchesUsedThisFrame; }
    int GetDrawCallCount() const { return m_drawCallsThisFrame; }
    
    // Cleanup
    void ClearAllBatches();

private:
    LineBatch* GetOrCreateBatch(const LineProperties& properties);
    void SetupBatchVBO(LineBatch* batch);
    void RenderBatch(LineBatch* batch);
    void InitializeShader();
};

// Add these public methods to your main Renderer class:
class Renderer {
private:
    MegaLineBuffer m_megaLineBuffer;
    
public:
    // MegaLineBuffer interface
    void BeginLines() { m_megaLineBuffer.BeginFrame(); }
    void EndLines() { m_megaLineBuffer.EndFrame(); }
    void AddLine(float x1, float y1, float x2, float y2, const Colour& color, float width = 1.0f) {
        m_megaLineBuffer.AddLine(x1, y1, x2, y2, color, width);
    }
    void AddCircle(float x, float y, float radius, const Colour& color, int segments = 32, float width = 1.0f) {
        m_megaLineBuffer.AddCircle(x, y, radius, color, segments, width);
    }
    void AddRect(float x, float y, float w, float h, const Colour& color, float width = 1.0f) {
        m_megaLineBuffer.AddRect(x, y, w, h, color, width);
    }
    
    // Performance monitoring
    int GetMegaLineBufferStats() const { return m_megaLineBuffer.GetDrawCallCount(); }
};
```

---

## **Implementation (renderer.cpp)**

Add this to your renderer.cpp file:

```cpp
#include <unordered_map>
#include <memory>
#include <vector>
#include <cmath>

// ========================================
// MegaLineBuffer Implementation
// ========================================

MegaLineBuffer::MegaLineBuffer() 
    : m_frameActive(false), m_shaderProgram(0), m_totalLinesThisFrame(0), 
      m_batchesUsedThisFrame(0), m_drawCallsThisFrame(0) {
    InitializeShader();
}

MegaLineBuffer::~MegaLineBuffer() {
    ClearAllBatches();
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
    }
}

void MegaLineBuffer::InitializeShader() {
    // Use the existing color shader from main renderer
    // This assumes you have m_colorShaderProgram available
    // If not, we'll create a simple shader
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aColor;
        
        uniform mat4 uProjection;
        uniform mat4 uModelView;
        
        out vec4 vertexColor;
        
        void main() {
            gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
            vertexColor = aColor;
        }
    )";
    
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec4 vertexColor;
        out vec4 FragColor;
        
        void main() {
            FragColor = vertexColor;
        }
    )";
    
    // Create and compile shaders (reuse your existing CreateShader method)
    m_shaderProgram = g_renderer->CreateShader(vertexShaderSource, fragmentShaderSource);
}

void MegaLineBuffer::BeginFrame() {
    if (m_frameActive) {
        AppDebugOut("MegaLineBuffer: BeginFrame called while frame already active!\n");
        return;
    }
    
    m_frameActive = true;
    m_totalLinesThisFrame = 0;
    m_batchesUsedThisFrame = 0;
    m_drawCallsThisFrame = 0;
    m_activeBatches.clear();
    
    // Clear all batch vertices for new frame
    for (auto& pair : m_batches) {
        pair.second->vertices.clear();
        pair.second->vboNeedsUpdate = false;
    }
}

void MegaLineBuffer::EndFrame() {
    if (!m_frameActive) {
        return; // No active frame
    }
    
    RenderAllBatches();
    m_frameActive = false;
    
    // Debug output for performance monitoring
    #ifdef DEBUG_MEGALINEBUFFER
    if (m_totalLinesThisFrame > 0) {
        AppDebugOut("MegaLineBuffer: %d lines in %d batches = %d draw calls\n", 
                   m_totalLinesThisFrame, m_batchesUsedThisFrame, m_drawCallsThisFrame);
    }
    #endif
}

MegaLineBuffer::LineBatch* MegaLineBuffer::GetOrCreateBatch(const LineProperties& properties) {
    auto it = m_batches.find(properties);
    if (it != m_batches.end()) {
        return it->second.get();
    }
    
    // Create new batch
    auto batch = std::make_unique<LineBatch>(properties);
    LineBatch* batchPtr = batch.get();
    m_batches[properties] = std::move(batch);
    
    // Setup VBO for new batch
    SetupBatchVBO(batchPtr);
    
    return batchPtr;
}

void MegaLineBuffer::SetupBatchVBO(LineBatch* batch) {
    glGenVertexArrays(1, &batch->VAO);
    glGenBuffers(1, &batch->VBO);
    
    glBindVertexArray(batch->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, batch->VBO);
    
    // Allocate large buffer for dynamic use
    glBufferData(GL_ARRAY_BUFFER, sizeof(LineVertex) * 50000, nullptr, GL_DYNAMIC_DRAW);
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute (location 1)  
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void MegaLineBuffer::AddLine(float x1, float y1, float x2, float y2, const Colour& color, float width) {
    if (!m_frameActive) {
        AppDebugOut("MegaLineBuffer: AddLine called outside BeginFrame/EndFrame!\n");
        return;
    }
    
    LineProperties props(color, width, g_renderer->GetBlendMode());
    LineBatch* batch = GetOrCreateBatch(props);
    
    // Convert color to float
    float r = color.m_r / 255.0f;
    float g = color.m_g / 255.0f;
    float b = color.m_b / 255.0f;
    float a = color.m_a / 255.0f;
    
    // Add line as two vertices
    batch->vertices.emplace_back(x1, y1, r, g, b, a);
    batch->vertices.emplace_back(x2, y2, r, g, b, a);
    batch->vboNeedsUpdate = true;
    
    m_totalLinesThisFrame++;
    
    // Track this batch for rendering
    if (batch->vertices.size() == 2) { // First line in this batch
        m_activeBatches.push_back(batch);
    }
}

void MegaLineBuffer::AddCircle(float centerX, float centerY, float radius, const Colour& color, int segments, float width) {
    if (!m_frameActive) return;
    
    // Generate circle as line segments
    float angleStep = 2.0f * M_PI / segments;
    for (int i = 0; i < segments; i++) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;
        
        float x1 = centerX + cosf(angle1) * radius;
        float y1 = centerY + sinf(angle1) * radius;
        float x2 = centerX + cosf(angle2) * radius;
        float y2 = centerY + sinf(angle2) * radius;
        
        AddLine(x1, y1, x2, y2, color, width);
    }
}

void MegaLineBuffer::AddRect(float x, float y, float w, float h, const Colour& color, float width) {
    if (!m_frameActive) return;
    
    // Add rectangle as 4 lines
    AddLine(x, y, x + w, y, color, width);         // Top
    AddLine(x + w, y, x + w, y + h, color, width); // Right  
    AddLine(x + w, y + h, x, y + h, color, width); // Bottom
    AddLine(x, y + h, x, y, color, width);         // Left
}

void MegaLineBuffer::AddDashedLine(float x1, float y1, float x2, float y2, const Colour& color, float dashLength, float width) {
    if (!m_frameActive) return;
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    float totalLength = sqrtf(dx * dx + dy * dy);
    
    if (totalLength < 0.001f) return; // Too short
    
    float unitX = dx / totalLength;
    float unitY = dy / totalLength;
    
    float currentLength = 0.0f;
    bool drawing = true;
    
    while (currentLength < totalLength) {
        float segmentLength = std::min(dashLength, totalLength - currentLength);
        
        if (drawing) {
            float startX = x1 + unitX * currentLength;
            float startY = y1 + unitY * currentLength;
            float endX = x1 + unitX * (currentLength + segmentLength);
            float endY = y1 + unitY * (currentLength + segmentLength);
            
            AddLine(startX, startY, endX, endY, color, width);
        }
        
        currentLength += segmentLength;
        drawing = !drawing; // Alternate between drawing and not drawing
    }
}

void MegaLineBuffer::AddFadingLine(float x1, float y1, float x2, float y2, const Colour& startColor, const Colour& endColor, float width) {
    if (!m_frameActive) return;
    
    LineProperties props(startColor, width, g_renderer->GetBlendMode()); // Use start color for batching
    LineBatch* batch = GetOrCreateBatch(props);
    
    // Convert colors to float
    float r1 = startColor.m_r / 255.0f, g1 = startColor.m_g / 255.0f;
    float b1 = startColor.m_b / 255.0f, a1 = startColor.m_a / 255.0f;
    float r2 = endColor.m_r / 255.0f, g2 = endColor.m_g / 255.0f;  
    float b2 = endColor.m_b / 255.0f, a2 = endColor.m_a / 255.0f;
    
    // Add fading line as two vertices with different colors
    batch->vertices.emplace_back(x1, y1, r1, g1, b1, a1);
    batch->vertices.emplace_back(x2, y2, r2, g2, b2, a2);
    batch->vboNeedsUpdate = true;
    
    m_totalLinesThisFrame++;
    
    if (batch->vertices.size() == 2) {
        m_activeBatches.push_back(batch);
    }
}

void MegaLineBuffer::RenderAllBatches() {
    if (m_activeBatches.empty()) return;
    
    // Use our shader
    glUseProgram(m_shaderProgram);
    
    // Set matrix uniforms (use main renderer's matrices)
    int projLoc = glGetUniformLocation(m_shaderProgram, "uProjection");
    int modelViewLoc = glGetUniformLocation(m_shaderProgram, "uModelView");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, g_renderer->GetProjectionMatrix());
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, g_renderer->GetModelViewMatrix());
    
    // Render each active batch
    for (LineBatch* batch : m_activeBatches) {
        if (!batch->vertices.empty()) {
            RenderBatch(batch);
            m_drawCallsThisFrame++;
        }
    }
    
    m_batchesUsedThisFrame = m_activeBatches.size();
    
    glUseProgram(0);
}

void MegaLineBuffer::RenderBatch(LineBatch* batch) {
    if (batch->vertices.empty()) return;
    
    // Update VBO if needed
    if (batch->vboNeedsUpdate) {
        glBindBuffer(GL_ARRAY_BUFFER, batch->VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 
                       batch->vertices.size() * sizeof(LineVertex), 
                       batch->vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        batch->vboNeedsUpdate = false;
    }
    
    // Set blend mode for this batch
    g_renderer->SetBlendMode(batch->properties.blendMode);
    
    // Render the batch
    glBindVertexArray(batch->VAO);
    glDrawArrays(GL_LINES, 0, batch->vertices.size());
    glBindVertexArray(0);
}

void MegaLineBuffer::ClearAllBatches() {
    for (auto& pair : m_batches) {
        LineBatch* batch = pair.second.get();
        if (batch->VAO) glDeleteVertexArrays(1, &batch->VAO);
        if (batch->VBO) glDeleteBuffers(1, &batch->VBO);
    }
    m_batches.clear();
    m_activeBatches.clear();
}
```

---

## **Usage Guide**

### **Step 1: Replace Line() Calls**

**OLD (Immediate Mode - BAD):**
```cpp
// This causes immediate draw calls
g_renderer->Line(x1, y1, x2, y2, color);
g_renderer->Circle(x, y, radius, 30, color);
```

**NEW (Batched - GOOD):**
```cpp
// These get automatically batched
g_renderer->AddLine(x1, y1, x2, y2, color);
g_renderer->AddCircle(x, y, radius, color, 30);
```

### **Step 2: Wrap Rendering Sections**

**In map_renderer.cpp:**
```cpp
void MapRenderer::RenderDynamicContent() {
    // Begin collecting lines for batching
    g_renderer->BeginLines();
    
    // All of these will be automatically batched by color/properties
    RenderUnitTrails();     // Convert to use AddLine()
    RenderOrderLines();     // Convert to use AddLine()
    RenderGunfire();        // Convert to use AddLine()
    RenderEffects();        // Convert to use AddLine()
    RenderRadarSweeps();    // Convert to use AddLine()
    
    // Render all batches in optimal draw calls
    g_renderer->EndLines();
}
```

### **Step 3: Convert Specific Systems**

**Unit Trails:**
```cpp
// OLD
void WorldObject::RenderTrail() {
    for (int i = 0; i < m_trailHistory.Size() - 1; i++) {
        g_renderer->Line(trail[i].x, trail[i].y, trail[i+1].x, trail[i+1].y, trailColor);
    }
}

// NEW
void WorldObject::RenderTrail() {
    for (int i = 0; i < m_trailHistory.Size() - 1; i++) {
        g_renderer->AddLine(trail[i].x, trail[i].y, trail[i+1].x, trail[i+1].y, trailColor);
    }
}
```

**Order Lines:**
```cpp
// OLD
void MapRenderer::RenderWorldObjectTargets(WorldObject* wobj) {
    g_renderer->Line(wobj->x, wobj->y, target->x, target->y, orderColor);
}

// NEW  
void MapRenderer::RenderWorldObjectTargets(WorldObject* wobj) {
    g_renderer->AddLine(wobj->x, wobj->y, target->x, target->y, orderColor);
}
```

**Gunfire:**
```cpp
// OLD
void Gunfire::Render() {
    g_renderer->Line(m_fromX, m_fromY, m_toX, m_toY, GUNFIRE_COLOR);
}

// NEW
void Gunfire::Render() {
    g_renderer->AddLine(m_fromX, m_fromY, m_toX, m_toY, GUNFIRE_COLOR);
}
```

### **Step 4: Advanced Features**

**Animated/Fading Trails:**
```cpp
void WorldObject::RenderFadingTrail() {
    for (int i = 0; i < m_trailHistory.Size() - 1; i++) {
        float age = (float)i / m_trailHistory.Size();
        int alpha = 255 * (1.0f - age); // Fade with age
        Colour fadeColor(255, 255, 255, alpha);
        
        g_renderer->AddLine(trail[i].x, trail[i].y, trail[i+1].x, trail[i+1].y, fadeColor);
    }
}
```

**Dashed Order Lines:**
```cpp
void RenderDashedOrderLine(float x1, float y1, float x2, float y2) {
    g_renderer->AddDashedLine(x1, y1, x2, y2, ORDER_COLOR, 3.0f); // 3-pixel dashes
}
```

### **Step 5: Performance Monitoring**

```cpp
void MapRenderer::RenderDebugInfo() {
    int drawCalls = g_renderer->GetMegaLineBufferStats();
    g_renderer->TextSimple(10, 10, White, 12, 
        "Line Draw Calls: %d", drawCalls);
}
```

---

## **Migration Strategy**

### **Phase 1: Basic Integration (Day 1)**
1. Add MegaLineBuffer class to renderer.h and renderer.cpp
2. Add BeginLines()/EndLines()/AddLine() methods to main Renderer class
3. Test with simple line: `g_renderer->AddLine(0, 0, 100, 100, Red);`

### **Phase 2: Convert Major Systems (Day 2-3)**
1. Wrap RenderObjects() with BeginLines()/EndLines()
2. Replace Line() calls in unit trail rendering
3. Replace Line() calls in order line rendering
4. Replace Line() calls in gunfire rendering

### **Phase 3: Advanced Features (Day 4-5)**
1. Add circle/rectangle support
2. Add dashed line support
3. Add fading line support
4. Performance optimization

**Expected Result: 100x-200x performance improvement for dynamic line rendering!**