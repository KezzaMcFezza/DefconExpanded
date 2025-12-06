#ifndef _included_renderer3dvbo_h
#define _included_renderer3dvbo_h

#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"
#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"
#include "lib/tosser/darray.h"

class Renderer3DVBO {

protected:

  int m_maxMegaVertices3D;
  int m_maxMegaIndices3D;

  bool m_megaVBO3DActive;
  char *m_currentMegaVBO3DKey;
  Colour m_megaVBO3DColor;
  Vertex3D *m_megaVertices3D;
  int m_megaVertex3DCount;
  unsigned int *m_megaIndices3D;
  int m_megaIndex3DCount;

  bool m_megaVBO3DTexturedActive;
  char *m_currentMegaVBO3DTexturedKey;
  unsigned int m_currentMegaVBO3DTextureID;
  int m_maxMegaTexturedVertices3D;
  int m_maxMegaTexturedIndices3D;
  Vertex3DTextured *m_megaTexturedVertices3D;
  int m_megaTexturedVertex3DCount;
  unsigned int *m_megaTexturedIndices3D;
  int m_megaTexturedIndex3DCount;

  bool m_megaVBO3DTrianglesActive;
  char *m_currentMegaVBO3DTrianglesKey;
  Colour m_megaVBO3DTrianglesColor;
  int m_maxMegaTriangleVertices3D;
  int m_maxMegaTriangleIndices3D;
  Vertex3D *m_megaTriangleVertices3D;
  int m_megaTriangleVertex3DCount;
  unsigned int *m_megaTriangleIndices3D;
  int m_megaTriangleIndex3DCount;

  //
  // VBO caching system for 3D geometry, replaced display lists

  struct Cached3DVBO {
    unsigned int VBO;
    unsigned int VAO;
    unsigned int IBO;           // Index buffer for indexed drawing
    int vertexCount;            // Number of actual vertices (no duplication)
    int indexCount;             // Number of indices
    Colour color;
    bool isValid;
  };

  BTree<Cached3DVBO*> m_cached3DVBOs;
  DArray<char*> m_protected3DVBOKeys;

public:
  Renderer3DVBO();
  virtual ~Renderer3DVBO();

  void InvalidateCached3DVBO      (const char *cacheKey);

  void BeginMegaVBO3D             (const char *megaVBOKey, Colour const &col);
  void AddLineStripToMegaVBO3D    (const Vector3<float> *vertices, int vertexCount);
  void EndMegaVBO3D               ();
  void RenderMegaVBO3D            (const char *megaVBOKey);
  bool IsMegaVBO3DValid           (const char *megaVBOKey);
  void SetMegaVBO3DBufferSizes    (int vertexCount, int indexCount);

  void BeginTexturedMegaVBO3D          (const char *megaVBOKey, unsigned int textureID);
  void AddTexturedQuadsToMegaVBO3D     (const Vertex3DTextured *vertices, int vertexCount, int quadCount);
  void EndTexturedMegaVBO3D            ();
  void RenderTexturedMegaVBO3D         (const char *megaVBOKey);
  bool IsTexturedMegaVBO3DValid        (const char *megaVBOKey);
  void SetTexturedMegaVBO3DBufferSizes (int vertexCount, int indexCount);

  void BeginTriangleMegaVBO3D          (const char *megaVBOKey, Colour const &col);
  void AddTrianglesToMegaVBO3D         (const Vector3<float> *vertices, int vertexCount);
  void EndTriangleMegaVBO3D            ();
  void RenderTriangleMegaVBO3D         (const char *megaVBOKey);
  bool IsTriangleMegaVBO3DValid         (const char *megaVBOKey);
  void SetTriangleMegaVBO3DBufferSizes (int vertexCount, int indexCount);
  
  void InvalidateAll3DVBOs        ();

  void Protect3DVBO               (const char* key);
  void Unprotect3DVBO             (const char* key);
  void Clear3DVBOProtection       ();
  bool Is3DVBOProtected           (const char* key);
  
  int GetCached3DVBOCount         ();
  int GetCached3DVBOVertexCount   (const char *cacheKey);
  int GetCached3DVBOIndexCount    (const char *cacheKey);

  int GetMegaBufferVertexCount3D   () const { return m_maxMegaVertices3D; }
  int GetMegaBufferIndexCount3D    () const { return m_maxMegaIndices3D; }

  size_t GetTotalAllocatedBufferMemory() const {
    size_t total = 0;
    
    total += m_maxMegaVertices3D * sizeof(Vertex3D);
    total += m_maxMegaIndices3D * sizeof(unsigned int);
    total += m_maxMegaTexturedVertices3D * sizeof(Vertex3DTextured);
    total += m_maxMegaTexturedIndices3D * sizeof(unsigned int);
    total += m_maxMegaTriangleVertices3D * sizeof(Vertex3D);
    total += m_maxMegaTriangleIndices3D * sizeof(unsigned int);
    
    return total;
  }
};

extern Renderer3DVBO *g_renderer3dvbo;

#endif
