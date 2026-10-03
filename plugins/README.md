# Lab Plugins & Addons Directory

> NOTE: This folder is legacy/docs only. Real drop-in plugins live in
> `assets/plugins/*.lua` — 1 file = 1 plugin, no engine recompile.
> Template: `assets/plugins/dropin_example.lua`.

This directory hosts modular, loadable plugins that extend **Lab Engine** without modifying the core codebase.

## Drop-in model (obowiazuje)

1. **1 plik = 1 plugin:** wrzucasz `moj_plugin.lua` do `assets/plugins/` (lub drag&drop na okno) i działa. Kasujesz plik i plugin znika. Pełna kontrola bez kompilacji silnika.
2. **Izolacja:** każdy plugin dostaje własny stan Lua (`Lab::LuaDropinPlugin`, `include/plugins/LuaDropinPlugin.h`), więc globalne tabele `Plugin` nigdy nie kolidują.
3. **Kontrakt pliku:** tabela `Plugin` (`id`, `displayName`, `version`, `author`, `category`, `description`, `dependencies`) + `onLoad` / `onUpdate(dt)` / `onUnload` + `return Plugin`. Dostępne: stdlib Lua + `Lab.log(msg)`.
4. **Cykl życia:** `PackageManager::scanPluginDirectory` (rescan z edytora), `pollDropinChanges` (auto co ~1s), `importDropinFile` (drag&drop), `deleteDropinPlugin` (usuń z dysku). Stare klasy C++ w `src/plugins/` to legacy — nie są już rejestrowane przy starcie.

## Architectural Principles
1. **Zero Core Bloat:**
   - Any subsystem that is not universal to every game (e.g. 2D-only physics, specific 3D vehicle physics, water buoyancy models) belongs in a plugin.
2. **Standard Interface:**
   - Plugins inherit from `Lab::ILabPlugin` (declared in `include/LabPlugin.h`) and are managed by `Lab::PluginManager`.
3. **Pluggable Systems Planned:**
   - `plugin_physics_box2d` – 2D physics simulation for platformers.
   - `plugin_physics_jolt` – High-performance 3D ragdolls, constraints, and CCD.
   - `plugin_vehicle` – Raycast suspension vehicle physics.
   - `plugin_water` – Buoyancy physics and Gerstner wave simulation.
