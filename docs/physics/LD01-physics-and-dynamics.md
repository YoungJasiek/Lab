---
docid: LD01
name: Advanced Physics & Dynamics Subsystems
related_docs: [LA01, LB01, LB02]
tags: [physics, vehicles, fluids, ragdolls, sat-ccd]
---

# ⚙️ Advanced Physics & Dynamics Subsystems

The **Lab Engine** incorporates a comprehensive suite of high-performance physics modules built from the ground up in modern C++20. While initial releases provided fundamental rigid body simulation and swept-AABB collision detection, the engine has expanded to include advanced dynamics: raycast suspension vehicle physics, fluid volumes with Archimedes buoyancy and dynamic waves, articulated ragdolls, constraints and joints, Verlet cloth and soft bodies, continuous collision detection (CCD) via Separating Axis Theorem (SAT), and environmental force fields.

---

## 🏎️ 1. Raycast Suspension Vehicle Physics (`LabVehicle.h`)

Rather than relying on unstable cylindrical rigid bodies for vehicle wheels, Lab uses the industry-standard **Raycast Suspension Model**:

- **Raycast Wheel Probing:** Each wheel projects downward rays from chassis anchor points against scene geometry.
- **Spring-Damper Dynamics:** Hooke's Law with damping:
  $$F_{\text{suspension}} = k_s \cdot (L_0 - L) - k_d \cdot v_{\text{rel}}$$
  where $k_s$ is stiffness, $k_d$ is damping, $L_0$ is rest length, and $L$ is current compressed length.
- **Pacejka / Coulomb Lateral Grip:** Calculates longitudinal acceleration/braking forces and lateral friction to prevent unrealistic sliding while enabling controlled drifting.
- **Ackermann Steering Geometry:** Calculates differential turning angles for inner and outer front wheels.
- **Drive Configurations:** Configurable Front-Wheel Drive (FWD), Rear-Wheel Drive (RWD), or All-Wheel Drive (AWD) with differential torque distribution.

---

## 🌊 2. Fluid Volumes, Archimedes Buoyancy & Waves (`LabFluid.h`)

The fluid simulation provides realistic aquatic interactions for water surfaces, toxic waste vats, and zero-gravity fluid tanks:

- **Water Volume Boundaries:** Defined via axis-aligned bounding boxes (AABBs) or arbitrary polygonal boundaries with water plane height $h_{\text{water}}$.
- **Dynamic Wave Functions:** Sum-of-sines / Gerstner wave displacement evaluating continuous height and surface normal:
  $$y(x, z, t) = \sum_{i} A_i \cdot \sin(\vec{k}_i \cdot [x, z]^T - \omega_i t)$$
- **Archimedes Buoyancy Force:** Calculates immersed volume ratio $\phi \in [0, 1]$ of submerged rigid bodies or vehicle chassis:
  $$\vec{F}_{\text{buoyancy}} = \rho_{\text{fluid}} \cdot V_{\text{submerged}} \cdot (-\vec{g})$$
- **Viscous Fluid Drag & Damping:** Applies linear and angular drag:
  $$\vec{F}_{\text{drag}} = -\frac{1}{2} C_d \cdot A \cdot \rho \cdot \|\vec{v}\|\vec{v}$$
  damping tumbling debris, buoyant barrels, and floating vehicles.
- **Splashes & Ripples:** Emits particle spray and sound triggers upon high-velocity impact with the water surface.

---

## 💀 3. Ragdoll Physics & Active Body Animations (`LabRagdoll.h`)

Replaces pre-baked death animations with fully physical skeletal collapses:

- **Bone Collision Capsules:** Automatically maps character bone hierarchies (pelvis, spine, head, limbs) to oriented capsules and collision boxes.
- **Articulated Constraints:** Spherical and cone-twist joints connect bone bodies, enforcing physiological joint angle limits (e.g. elbows bend only along single hinge axes).
- **Active Body Blending:** Smoothly blends physics impulses with kinematic keyframe animations, allowing characters to react to physical impacts before fully collapsing.
- **Impulse Transfer:** High-velocity projectile impacts and nearby volumetric explosions transfer directional kinetic momentum directly to the impacted bone body.

---

## 🔗 4. Constraints & Multi-Body Joints (`LabConstraint.h`)

Provides generalized multi-body mechanical linkages:

- **Joint Types:**
  - **`DistanceConstraint`:** Keeps two anchor points at a fixed or maximum distance (ropes, cables).
  - **`BallSocketConstraint`:** Allows 3-DOF rotation around a shared pivot while preventing relative translation.
  - **`HingeConstraint`:** Restricts motion to a single rotational axis (doors, gates, levers).
  - **`SliderConstraint`:** Restricts translation along a single axis (pistons, elevators, sliding hatches).
  - **`FixedConstraint`:** Rigidly welds two bodies together with breakable force thresholds.
- **Relaxation Iteration:** Uses Gauss-Seidel projected velocity / position impulse solvers for stable constraint chains without numerical explosion.

---

## 🧶 5. Verlet Soft Body & Cloth Simulation (`LabCloth.h`)

Simulates tactical flags, banners, cloth tarps, hair, and soft-body deformable cushions:

- **Verlet Particle Grid:** 2D grid of particles integrated via position Verlet:
  $$\vec{x}_{n+1} = 2\vec{x}_n - \vec{x}_{n-1} + \vec{a} \cdot \Delta t^2$$
- **Triple-Spring Connectivity:**
  - **Structural Springs:** Maintain horizontal and vertical grid spacing.
  - **Shear Springs:** Diagonal cross-connections preventing shearing distortion.
  - **Bending / Flexion Springs:** Every-second-particle links resisting extreme folding.
- **Wind & Force Fields:** Dynamic aerodynamical pressure calculation using surface normal $\vec{n}$:
  $$\vec{F}_{\text{wind}} = (\vec{v}_{\text{wind}} \cdot \vec{n}) \cdot \vec{n}$$
- **Scene Geometry Collision:** Cloth particles project out of solid brush geometry and sphere obstacles.

---

## 🎯 6. Convex Hull 3D SAT & Continuous Collision Detection (`LabCCD.h`)

Prevents high-speed tunneling (e.g. high-velocity projectiles, physical debris, and fast particles clipping through thin walls):

- **Separating Axis Theorem (SAT):** Exact intersection test for arbitrary convex polyhedra testing face normals and edge cross-products.
- **Continuous Collision Detection (CCD):** Casts swept volumes (swept spheres, swept AABBs) over the trajectory $\Delta \vec{x} = \vec{v} \cdot \Delta t$.
- **Time of Impact (TOI):** Solves exact fractional time $t \in [0, 1]$ of earliest contact, clamping body movement to the collision surface before penetration occurs.

---

## 🌀 7. Physics Force Fields & Environmental Triggers (`LabForceField.h`)

Allows level designers to place dynamic volumetric physical forces in `.labmap`:

- **Field Topologies:**
  - **`DirectionalField`:** Constant directional push (wind tunnels, conveyor belts, gravity lifts).
  - **`RadialField`:** Explosive push outwards or gravitational pull inwards with falloff (singularity wells, repulsors).
  - **`VortexField`:** Swirling angular velocity around a central axis (tornadoes, whirlpools).
  - **`BuoyancyField`:** Upward lift counteracting gravity within fluid volumes.
- **Trigger Filters:** Layer masks determining affected body types (RigidBodies, Particles, Vehicles, Ragdolls, Players).
