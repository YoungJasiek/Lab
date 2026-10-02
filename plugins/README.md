# Lab Plugins & Addons Directory

This directory hosts modular, loadable plugins that extend **Lab Engine** without modifying the core codebase.

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
