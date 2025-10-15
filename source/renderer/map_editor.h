#ifndef _included_mapeditor_h
#define _included_mapeditor_h

#include "renderer/map_renderer.h"

//Probably don't want to inherit from MapRenderer after all:
//lots of team-related stuff we don't want to manage or clobber
class MapEditor : public MapRenderer {
  void Test();
  void Init();
};

#endif
