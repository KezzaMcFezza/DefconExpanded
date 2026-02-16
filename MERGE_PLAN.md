# Merge Plan: THUNDYv2 → DefconExpanded

**Source**: `c:\Users\miles\DEFCONPROJECT\THUNDYv2`  
**Target**: `C:\DefconExpanded` (all edits here only)

---

## Progress

### Phase 1: Universal Mechanics
| Step | Status | Description |
|------|--------|-------------|
| 1.1a | Done | Add `m_selectedIds`, `ClearSelection()`, `AddToSelection()`, `RemoveFromSelection()`, `IsSelected()` to WorldRenderer |
| 1.1b | Done | Sync `m_selectedIds` with `SetCurrentSelectionId()` |
| 1.1c | Done | Add `GetSelectionCount()`, `GetSelectedId()` accessors |
| 1.1d | Done | Render selection circles for all selected units via highlight batching |
| 1.1e | Done | Add drag-marquee state and `RenderDragSelectionMarquee()` in map renderer |
| 1.1f | Done | Update `HandleSelectObject()` for shift-click add, ctrl-click toggle, drag-select |
| 1.2 | Done | Apply state to selection (same type) |
| 1.3 | Done | Double-click waypoints |

### Phase 2–4
- Not started

---

## Design Decisions (Confirmed)

| Topic | Decision |
|-------|----------|
| **Territories** | 28-territory system from THUNDYv2 (USA, NATO, Russia, China, India, Pakistan, etc.) |
| **Teams** | `MAX_TEAMS = 27` for 28-player system |
| **Radar** | Full multi-tier radar (5 grids: standard, early1, early2, stealth1, stealth2) |
| **Network** | Keep DefconExpanded's network layer; only change when explicitly required for new mechanics |
| **Graphics** | Placeholders for all new units; user will supply final assets |
| **Rendering** | Use DefconExpanded's `Render2D()` / `Render3D()` split (no combined `Render()`) |
| **Burst-fire** | Include as new mechanic (switch target after N shots) |

---

## Execution Order

1. **Phase 1**: Universal mechanics (multi-select, apply-to-selection, double-click waypoints)
2. **Phase 2**: Overarching systems (type system, radar tiers, burst-fire, aircraft spawners)
3. **Phase 3**: Individual new units (silos → ground defense → missiles → subs → ships → aircraft)
4. **Phase 4**: Integration (build, placeholders, localisation, network/recording checks)

---

## Phase 1: Universal Mechanics

### 1.1 Multi-unit selection
- Add `m_selectedIds`, `ClearSelection()`, `AddToSelection()`, `RemoveFromSelection()`, `IsSelected()` to `WorldRenderer`
- Add drag-marquee state and `RenderDragSelectionMarquee()` in map renderer
- Update `HandleSelectObject()` for shift-click add, ctrl-click toggle, drag-select
- Render selection circles for all selected units via highlight batching

### 1.2 Apply state to selection (same type)
- Add `m_stateApplyToSelectionSameType`; port logic from THUNDYv2's `HandleClickStateMenu()`

### 1.3 Double-click waypoints
- Add `m_isDoubleClick` to `ActionOrder`; extend `HandleSetWaypoint()` and input handling

---

## Phase 2: Overarching Systems

### 2.1 Territory & teams
- Replace DefconExpanded's 6 territories with THUNDYv2's 28-territory enum
- Set `MAX_TEAMS = 27`
- Update `World::NumTerritories`, territory assignment, `m_populationCenter`, etc.
- Port territory images / placeholder logic

### 2.2 WorldObject type system
- Add all new type enums (SiloMed, SiloMob, SAM, ABM, RadarEW, LACM, LASM, LANM, SubG/SubC/SubK, BattleShip2/3, AirBase2/3, FighterLight/Stealth, BomberFast/Stealth, AEW, Carrier2)
- Add `Archetype` and `ClassType` enums
- Add `m_archetype`, `m_classType`, `m_maxLife`, `m_stealthType`
- Add LACM fields, `m_maxAEW`, radar tier fields
- Add `IsBuilding()`, `IsShip()`, `IsAircraft()`, `IsSubmarine()`, `IsCarrierClass()`, etc.

### 2.3 WorldObjectState extensions
- Add radar tier ranges to `WorldObjectState` (radarearly1, radarearly2, radarstealth1, radarstealth2)
- Extend `AddState()` to accept these

### 2.4 Multi-tier radar
- Add 4 additional `RadarGrid` instances in `World`: `m_radarearly1Grid`, `m_radarearly2Grid`, `m_radarstealth1Grid`, `m_radarstealth2Grid`
- Extend `RadarGrid` / visibility logic for 5 tiers
- Add `m_previousRadarEarly1Range`, etc. to `WorldObject`
- Update all radar update paths

### 2.5 Burst-fire
- Add burst-fire members: `m_burstFireTargetIds`, `m_burstFireCountdowns`, `m_burstFireShotCount`
- Add `BurstFireTick()`, `BurstFireOnFired()`, `GetBurstFireShots()`, etc.
- Wire into `Update()` and gunfire targeting

### 2.6 Attack odds matrix
- Expand `s_attackOdds[NumObjectTypes][NumObjectTypes]` for all new types

### 2.7 Aircraft spawner extensions
- Add `m_maxAEW`, `CanLaunchFighterLight()`, `CanLaunchStealthFighter()`, `CanLaunchAEW()`, etc.
- Add `GetVisibleLaunchStateCount()`, `ConsumeLaunchSlot()`, `OnBomberLanded()`, `OnFighterLanded()`
- Extend launch methods with aircraft mode parameter

---

## Phase 3: Individual New Units

Order: silos → ground defense → radar → missiles → subs → surface ships → carriers → airbases → aircraft

| Category | Units | Source files |
|----------|-------|--------------|
| Silos | SiloMed, SiloMob | silomed.cpp/h, silomob.cpp/h |
| Ground defense | SAM, ABM, Interceptor | sam.cpp/h, abm.cpp/h, interceptor.cpp/h |
| Radar | RadarEW | radar_ew.cpp/h |
| Missiles | LACM, LASM, LANM | lacm.cpp/h, lasm.cpp/h, lanm.cpp/h |
| Subs | SubG, SubC, SubK, Torpedo | subg.cpp/h, subc.cpp/h, subk.cpp/h, torpedo.cpp/h |
| Battleships | BattleShip2, BattleShip3 | battleship2.cpp/h, battleship3.cpp/h |
| Carriers | Carrier2 | carrier2.cpp/h |
| Airbases | AirBase2, AirBase3 | airbase2.cpp/h, airbase3.cpp/h |
| Aircraft | FighterLight, FighterStealth, BomberFast, BomberStealth, AEW | Extend fighter/bomber or add separate files |

For each unit: add to `CreateObject()`, `GetName()`, `GetTypeName()`, `GetType()`, team/toolbar/side-panel, use placeholder graphics.

---

## Phase 4: Integration

- Add new `.cpp` to build system
- Localisation strings for all new units
- Placeholder graphics paths (user supplies final assets)
- Verify network/recording compatibility; only modify if required for new mechanics

---

## Constraints

- **Never overwrite**: DefconExpanded's systemIV, recording architecture, or render pipeline structure
- **Always use**: `Render2D()` / `Render3D()` split; `GetWorldRenderer()` for selection
- **Build & test** after each phase
