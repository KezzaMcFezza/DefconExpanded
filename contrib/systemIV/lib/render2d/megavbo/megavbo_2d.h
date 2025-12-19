#ifndef _included_renderervbo_h
#define _included_renderervbo_h

#include "lib/render2d/renderer_2d.h"
#include "lib/render/colour.h"
#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"
#include "lib/tosser/darray.h"

class Renderer2DVBO {

protected:

  int m_maxMegaVertices;
  int m_maxMegaIndices;

  bool m_megaVBOActive;
  char *m_currentMegaVBOKey;
  Colour m_megaVBOColor;
  float m_megaVBOWidth;
  Vertex2D *m_megaVertices;
  int m_megaVertexCount;
  unsigned int *m_megaIndices;
  int m_megaIndexCount;

  bool m_megaVBOTexturedActive;
  char *m_currentMegaVBOTexturedKey;
  unsigned int m_currentMegaVBOTextureID;
  int m_maxMegaTexturedVertices;
  int m_maxMegaTexturedIndices;
  Vertex2D *m_megaTexturedVertices;
  int m_megaTexturedVertexCount;
  unsigned int *m_megaTexturedIndices;
  int m_megaTexturedIndexCount;

  bool m_megaVBOTrianglesActive;
  char *m_currentMegaVBOTrianglesKey;
  Colour m_megaVBOTrianglesColor;
  int m_maxMegaTriangleVertices;
  int m_maxMegaTriangleIndices;
  Vertex2D *m_megaTriangleVertices;
  int m_megaTriangleVertexCount;
  unsigned int *m_megaTriangleIndices;
  int m_megaTriangleIndexCount;

  //
  // VBO caching system for coastlines and borders, replaced display lists

  enum VBOVertexType {
    VBO_VERTEX_2D,              // Vertex2D
    VBO_VERTEX_3D,              // Vertex3D
    VBO_VERTEX_3D_TEXTURED      // Vertex3DTextured
  };

  struct CachedVBO {
    unsigned int VBO;
    unsigned int VAO;
    unsigned int IBO;           // Index buffer for indexed drawing
    int vertexCount;            // Number of actual vertices (no duplication)
    int indexCount;             // Number of indices
    Colour color;
    float lineWidth;
    VBOVertexType vertexType;
    bool isValid;
  };

  BTree<CachedVBO*> m_cachedVBOs;
  DArray<char*> m_protectedVBOKeys;

public:
  Renderer2DVBO();
  virtual ~Renderer2DVBO();

  void InvalidateCachedVBO      (const char *cacheKey);

  void BeginMegaVBO             (const char *megaVBOKey, Colour const &col);
  void AddLineStripToMegaVBO    (float *vertices, int vertexCount);
  void EndMegaVBO               ();
  void RenderMegaVBO            (const char *megaVBOKey);
  bool IsMegaVBOValid           (const char *megaVBOKey);
  void SetMegaVBOBufferSizes    (int vertexCount, int indexCount, const char *cacheKey);

  void BeginTexturedMegaVBO          (const char *megaVBOKey, unsigned int textureID);
  void AddTexturedQuadsToMegaVBO     (const Vertex2D *vertices, int vertexCount, int quadCount);
  void EndTexturedMegaVBO            ();
  void RenderTexturedMegaVBO         (const char *megaVBOKey);
  bool IsTexturedMegaVBOValid        (const char *megaVBOKey);
  void SetTexturedMegaVBOBufferSizes (int vertexCount, int indexCount, const char *cacheKey);

  void BeginTriangleMegaVBO          (const char *megaVBOKey, Colour const &col);
  void AddTrianglesToMegaVBO         (const float *vertices, int vertexCount);
  void EndTriangleMegaVBO            ();
  void RenderTriangleMegaVBO         (const char *megaVBOKey);
  bool IsTriangleMegaVBOValid        (const char *megaVBOKey);
  void SetTriangleMegaVBOBufferSizes (int vertexCount, int indexCount, const char *cacheKey);
  
  void InvalidateAllVBOs        ();

  void ProtectVBO               (const char* key);
  void UnprotectVBO             (const char* key);
  void ClearVBOProtection       ();
  bool IsVBOProtected           (const char* key);
  
  int GetCachedVBOCount         ();
  int GetCachedVBOVertexCount   (const char *cacheKey);
  int GetCachedVBOIndexCount    (const char *cacheKey);
  size_t GetCachedVBOVertexSize (const char *cacheKey);
  
  DArray<char*> *GetAllCachedVBOKeys();

  int GetMegaBufferVertexCount   () const { return m_maxMegaVertices; }
  int GetMegaBufferIndexCount    () const { return m_maxMegaIndices; }

  size_t GetTotalAllocatedBufferMemory() const {
    size_t total = 0;
    
    total += m_maxMegaVertices * sizeof(Vertex2D);
    total += m_maxMegaIndices * sizeof(unsigned int);
    total += m_maxMegaTexturedVertices * sizeof(Vertex2D);
    total += m_maxMegaTexturedIndices * sizeof(unsigned int);
    total += m_maxMegaTriangleVertices * sizeof(Vertex2D);
    total += m_maxMegaTriangleIndices * sizeof(unsigned int);
    
    return total;
  }
};

class Renderer2DVBO;

extern Renderer2DVBO *g_renderer2dvbo;

#endif
