#include "LabPhysics.h"
#include "LabRenderer.h"
#include "LabAudio.h"
#include <cmath>
#include <cstdlib>

namespace Lab {

    void RigidBody::setBox(const Vec3& halfExt, float m) {
        halfExtents = halfExt;
        mass = m;
        if (mass > 0.0001f && !isStatic) {
            invMass = 1.0f / mass;
            // Moment of inertia for solid cuboid: I_x = m/12 * (w_y^2 + w_z^2) = m/3 * (h_y^2 + h_z^2)
            float ix = (mass / 3.0f) * (halfExtents.y * halfExtents.y + halfExtents.z * halfExtents.z);
            float iy = (mass / 3.0f) * (halfExtents.x * halfExtents.x + halfExtents.z * halfExtents.z);
            float iz = (mass / 3.0f) * (halfExtents.x * halfExtents.x + halfExtents.y * halfExtents.y);
            inertiaLocal = Vec3(ix, iy, iz);
            invInertiaLocal = Vec3(1.0f / ix, 1.0f / iy, 1.0f / iz);
        } else {
            invMass = 0.0f;
            inertiaLocal = Vec3(1e9f, 1e9f, 1e9f);
            invInertiaLocal = Vec3(0.0f, 0.0f, 0.0f);
            isStatic = true;
        }
    }

    void RigidBody::applyCentralImpulse(const Vec3& impulse) {
        if (isStatic || invMass == 0.0f) return;
        linearVelocity += impulse * invMass;
        isSleeping = false;
        sleepTimer = 0.0f;
    }

    void RigidBody::applyTorqueImpulse(const Vec3& torqueImpulse) {
        if (isStatic || invMass == 0.0f) return;
        Vec3 localTorque = orientation.inverse().rotateVector(torqueImpulse);
        Vec3 localDeltaOmega(
            localTorque.x * invInertiaLocal.x,
            localTorque.y * invInertiaLocal.y,
            localTorque.z * invInertiaLocal.z
        );
        angularVelocity += orientation.rotateVector(localDeltaOmega);
        isSleeping = false;
        sleepTimer = 0.0f;
    }

    void RigidBody::applyImpulse(const Vec3& impulse, const Vec3& contactPoint) {
        if (isStatic || invMass == 0.0f) return;
        linearVelocity += impulse * invMass;
        Vec3 r = contactPoint - position;
        Vec3 torqueImpulse = Vec3::cross(r, impulse);
        applyTorqueImpulse(torqueImpulse);
        isSleeping = false;
        sleepTimer = 0.0f;
    }

    Mat4 RigidBody::getTransformMatrix() const {
        return makeTransform(position, orientation, halfExtents * 2.0f);
    }

    std::vector<Vec3> RigidBody::getCorners() const {
        std::vector<Vec3> corners;
        corners.reserve(8);
        for (float sx : { -1.0f, 1.0f }) {
            for (float sy : { -1.0f, 1.0f }) {
                for (float sz : { -1.0f, 1.0f }) {
                    Vec3 localCorner(halfExtents.x * sx, halfExtents.y * sy, halfExtents.z * sz);
                    corners.push_back(position + orientation.rotateVector(localCorner));
                }
            }
        }
        return corners;
    }

    void RigidBody::getAABB(Vec3& outMin, Vec3& outMax) const {
        auto corners = getCorners();
        outMin = corners[0];
        outMax = corners[0];
        for (size_t i = 1; i < corners.size(); ++i) {
            outMin.x = std::min(outMin.x, corners[i].x);
            outMin.y = std::min(outMin.y, corners[i].y);
            outMin.z = std::min(outMin.z, corners[i].z);

            outMax.x = std::max(outMax.x, corners[i].x);
            outMax.y = std::max(outMax.y, corners[i].y);
            outMax.z = std::max(outMax.z, corners[i].z);
        }
    }

    bool RigidBody::raycast(const Vec3& rayOrigin, const Vec3& rayDir, float& outDist, Vec3& outNormal) const {
        // Transform ray to local object space
        Quat invQuat = orientation.inverse();
        Vec3 localOrigin = invQuat.rotateVector(rayOrigin - position);
        Vec3 localDir = invQuat.rotateVector(rayDir);

        Vec3 localHitNormal;
        if (Raycast::rayIntersectAABB(localOrigin, localDir, -halfExtents, halfExtents, outDist, &localHitNormal)) {
            outNormal = orientation.rotateVector(localHitNormal).normalized();
            return true;
        }
        return false;
    }

    // =========================================================================
    // PhysicsWorld Implementation
    // =========================================================================

    void PhysicsWorld::init() {
        if (!crateTexture) {
            crateTexture = std::make_unique<Texture>("crate_wood.png");
        }
        if (!barrelTexture) {
            barrelTexture = std::make_unique<Texture>("barrel_hazard.png");
        }
        if (!debrisTexture) {
            debrisTexture = std::make_unique<Texture>("debris_wood.png");
        }
    }

    void PhysicsWorld::clear() {
        bodies.clear();
        pendingExplosions.clear();
        _nextBodyId = 1;
    }

    RigidBody* PhysicsWorld::spawnCrate(const Vec3& pos, const Vec3& halfExt, float mass) {
        init();
        auto body = std::make_unique<RigidBody>();
        body->id = _nextBodyId++;
        body->type = PropType::Crate;
        body->position = pos;
        body->orientation = Quat::identity();
        body->health = 45.0f;
        body->maxHealth = 45.0f;
        body->color = Vec3(1.0f, 1.0f, 1.0f);
        body->setBox(halfExt, mass);
        body->material.restitution = 0.22f;
        body->material.friction = 0.65f;

        RigidBody* ptr = body.get();
        bodies.push_back(std::move(body));
        return ptr;
    }

    RigidBody* PhysicsWorld::spawnExplosiveBarrel(const Vec3& pos, const Vec3& halfExt, float mass) {
        init();
        auto body = std::make_unique<RigidBody>();
        body->id = _nextBodyId++;
        body->type = PropType::ExplosiveBarrel;
        body->position = pos;
        body->orientation = Quat::identity();
        body->health = 30.0f;
        body->maxHealth = 30.0f;
        body->color = Vec3(1.0f, 1.0f, 1.0f);
        body->setBox(halfExt, mass);
        body->material.restitution = 0.35f;
        body->material.friction = 0.50f;

        RigidBody* ptr = body.get();
        bodies.push_back(std::move(body));
        return ptr;
    }

    RigidBody* PhysicsWorld::spawnDebris(const Vec3& pos, const Vec3& halfExt, const Vec3& vel, const Vec3& angVel, PropType parentType, float lifetime) {
        init();
        auto body = std::make_unique<RigidBody>();
        body->id = _nextBodyId++;
        body->type = PropType::DebrisChunk;
        body->position = pos;
        body->orientation = Quat::fromEuler(
            ((rand() % 100) / 100.0f) * 6.28f,
            ((rand() % 100) / 100.0f) * 6.28f,
            ((rand() % 100) / 100.0f) * 6.28f
        );
        body->linearVelocity = vel;
        body->angularVelocity = angVel;
        body->health = 10.0f;
        body->maxHealth = 10.0f;
        body->lifetime = 0.0f;
        body->maxLifetime = lifetime;
        body->color = (parentType == PropType::ExplosiveBarrel) ? Vec3(0.35f, 0.32f, 0.30f) : Vec3(0.85f, 0.70f, 0.50f);
        body->setBox(halfExt, 2.5f);
        body->material.restitution = 0.28f;
        body->material.friction = 0.60f;

        RigidBody* ptr = body.get();
        bodies.push_back(std::move(body));
        return ptr;
    }

    void PhysicsWorld::spawnCrateDebris(const Vec3& posIn, const Vec3& halfExtIn, const Vec3& impactDirIn) {
        Vec3 pos = posIn;
        Vec3 halfExt = halfExtIn;
        Vec3 impactDir = impactDirIn;
        AudioEngine::playSound3D(SoundID::CrateBreak, pos, 1.0f, 1.0f);
        Vec3 base = (impactDir.lengthSq() > 0.01f) ? impactDir.normalized() : Vec3(0, 1, 0);

        for (int i = 0; i < 7; ++i) {
            float rx = ((rand() % 100) - 50) / 50.0f;
            float ry = ((rand() % 100) / 100.0f) * 0.8f + 0.3f;
            float rz = ((rand() % 100) - 50) / 50.0f;
            Vec3 dir = (base * 1.8f + Vec3(rx, ry, rz)).normalized();
            float speed = 3.5f + (rand() % 40) / 10.0f;

            Vec3 angVel(
                ((rand() % 100) - 50) * 0.2f,
                ((rand() % 100) - 50) * 0.2f,
                ((rand() % 100) - 50) * 0.2f
            );

            Vec3 chunkExt(
                halfExt.x * 0.45f,
                halfExt.y * 0.25f,
                halfExt.z * 0.50f
            );

            Vec3 spawnPos = pos + Vec3(rx * 0.25f, ry * 0.25f, rz * 0.25f);
            spawnDebris(spawnPos, chunkExt, dir * speed, angVel, PropType::Crate, 5.0f + (rand() % 30) / 10.0f);
        }
    }

    void PhysicsWorld::spawnBarrelDebris(const Vec3& posIn, const Vec3& halfExtIn, const Vec3& impactDirIn) {
        Vec3 pos = posIn;
        Vec3 halfExt = halfExtIn;
        Vec3 impactDir = impactDirIn;
        AudioEngine::playSound3D(SoundID::BarrelImpact, pos, 1.0f, 1.0f);
        Vec3 base = (impactDir.lengthSq() > 0.01f) ? impactDir.normalized() : Vec3(0, 1, 0);

        for (int i = 0; i < 6; ++i) {
            float rx = ((rand() % 100) - 50) / 50.0f;
            float ry = ((rand() % 100) / 100.0f) * 1.0f + 0.5f;
            float rz = ((rand() % 100) - 50) / 50.0f;
            Vec3 dir = (base * 2.0f + Vec3(rx, ry, rz)).normalized();
            float speed = 4.5f + (rand() % 50) / 10.0f;

            Vec3 angVel(
                ((rand() % 100) - 50) * 0.3f,
                ((rand() % 100) - 50) * 0.3f,
                ((rand() % 100) - 50) * 0.3f
            );

            Vec3 chunkExt(
                halfExt.x * 0.40f,
                halfExt.y * 0.30f,
                halfExt.z * 0.40f
            );

            Vec3 spawnPos = pos + Vec3(rx * 0.2f, ry * 0.2f, rz * 0.2f);
            spawnDebris(spawnPos, chunkExt, dir * speed, angVel, PropType::ExplosiveBarrel, 4.5f + (rand() % 30) / 10.0f);
        }
    }

    bool PhysicsWorld::takeDamage(int propId, float damage, const Vec3& hitPoint, const Vec3& hitDir) {
        Vec3 destroyPos;
        Vec3 destroyHalfExt;
        PropType destroyType = PropType::Static;
        bool shouldDestroy = false;
        Vec3 pushDir = (hitDir.lengthSq() > 0.01f) ? hitDir.normalized() : Vec3(0, 0, 1);
        int destroyedId = -1;

        for (auto& body : bodies) {
            if (body->id == propId && !body->isDestroyed) {
                body->health -= damage;
                body->applyImpulse(pushDir * (damage * 3.5f) + Vec3(0.0f, damage * 0.8f, 0.0f), hitPoint);

                if (body->health <= 0.0f) {
                    body->isDestroyed = true;
                    shouldDestroy = true;
                    destroyPos = body->position;
                    destroyHalfExt = body->halfExtents;
                    destroyType = body->type;
                    destroyedId = body->id;
                }
                break;
            }
        }

        if (shouldDestroy) {
            if (destroyType == PropType::Crate) {
                spawnCrateDebris(destroyPos, destroyHalfExt, pushDir);
            } else if (destroyType == PropType::ExplosiveBarrel) {
                applyExplosionImpulse(destroyPos, 6.5f, 450.0f, 130.0f, destroyedId);
                spawnBarrelDebris(destroyPos, destroyHalfExt, pushDir);
            }
            return true;
        }
        return false;
    }

    void PhysicsWorld::applyExplosionImpulse(const Vec3& epicenter, float radius, float maxImpulse, float damage, int sourcePropId) {
        // Buffer event for main game loop (spawning explosion VFX, dealing damage to player/bots)
        pendingExplosions.push_back({ epicenter, radius, damage, maxImpulse, sourcePropId });

        struct SecondaryDestruction {
            Vec3 pos;
            Vec3 halfExt;
            Vec3 dir;
            PropType type;
            int id;
        };
        std::vector<SecondaryDestruction> secondaryDestructions;

        // Affect rigid bodies in physics simulation
        float rSq = radius * radius;
        for (size_t i = 0; i < bodies.size(); ++i) {
            auto& b = bodies[i];
            if (b->isDestroyed || b->isStatic) continue;

            Vec3 diff = b->position - epicenter;
            float dSq = diff.lengthSq();
            if (dSq <= rSq && dSq > 0.001f) {
                float dist = std::sqrt(dSq);
                float factor = 1.0f - (dist / radius);
                Vec3 dir = diff / dist;

                Vec3 impulse = dir * (maxImpulse * factor) + Vec3(0.0f, maxImpulse * 0.45f * factor, 0.0f);
                b->applyImpulse(impulse, b->position - dir * (b->halfExtents.x * 0.5f));
                b->isSleeping = false;

                // Chain reaction damage
                if (b->id != sourcePropId && (b->type == PropType::Crate || b->type == PropType::ExplosiveBarrel)) {
                    b->health -= (damage * factor);
                    if (b->health <= 0.0f) {
                        b->isDestroyed = true;
                        secondaryDestructions.push_back({ b->position, b->halfExtents, dir, b->type, b->id });
                    }
                }
            }
        }

        // Process secondary chain reaction destruction AFTER loop over bodies is finished
        for (const auto& sec : secondaryDestructions) {
            if (sec.type == PropType::Crate) {
                spawnCrateDebris(sec.pos, sec.halfExt, sec.dir);
            } else if (sec.type == PropType::ExplosiveBarrel) {
                applyExplosionImpulse(sec.pos, radius * 0.9f, maxImpulse * 0.9f, damage * 0.9f, sec.id);
                spawnBarrelDebris(sec.pos, sec.halfExt, sec.dir);
            }
        }
    }

    void PhysicsWorld::resolveGroundCollision(RigidBody& body, float /*dt*/) {
        auto corners = body.getCorners();
        float deepestPen = 0.0f;
        Vec3 deepestCorner = body.position;

        for (const auto& c : corners) {
            if (c.y < 0.0f) {
                float pen = -c.y;
                if (pen > deepestPen) {
                    deepestPen = pen;
                    deepestCorner = c;
                }
            }
        }

        if (deepestPen > 0.0001f) {
            body.position.y += deepestPen;

            Vec3 n(0.0f, 1.0f, 0.0f);
            Vec3 r = deepestCorner - body.position;
            Vec3 vContact = body.linearVelocity + Vec3::cross(body.angularVelocity, r);
            float vn = Vec3::dot(vContact, n);

            if (vn < 0.0f) {
                float e = (std::abs(vn) < 0.25f) ? 0.0f : body.material.restitution;
                float jn = -(1.0f + e) * vn * body.mass * 0.65f;
                body.linearVelocity += n * (jn * body.invMass);

                // Angular response to torque at contact point
                Vec3 torqueImpulse = Vec3::cross(r, n * jn);
                body.applyTorqueImpulse(torqueImpulse * 0.45f);

                // Ground friction
                Vec3 vt = vContact - n * vn;
                float vtLen = vt.length();
                if (vtLen > 0.01f) {
                    Vec3 frictionDir = -vt / vtLen;
                    float frictionImpulse = std::min(vtLen * body.mass, jn * body.material.friction);
                    body.linearVelocity += frictionDir * (frictionImpulse * body.invMass);
                    body.angularVelocity = body.angularVelocity * 0.88f;
                }
            }
        }
    }

    void PhysicsWorld::resolveObstacleCollision(RigidBody& body, const CollisionBox& obstacle, float /*dt*/) {
        Vec3 bMin, bMax;
        body.getAABB(bMin, bMax);

        if (!obstacle.intersects(bMin, bMax)) return;

        // Determine shallowest penetration axis
        float dx1 = obstacle.max.x - bMin.x;
        float dx2 = bMax.x - obstacle.min.x;
        float overlapX = std::min(dx1, dx2);

        float dy1 = obstacle.max.y - bMin.y;
        float dy2 = bMax.y - obstacle.min.y;
        float overlapY = std::min(dy1, dy2);

        float dz1 = obstacle.max.z - bMin.z;
        float dz2 = bMax.z - obstacle.min.z;
        float overlapZ = std::min(dz1, dz2);

        Vec3 resolveNormal(0, 0, 0);
        float minOverlap = overlapX;
        resolveNormal = (dx1 < dx2) ? Vec3(1, 0, 0) : Vec3(-1, 0, 0);

        if (overlapY < minOverlap) {
            minOverlap = overlapY;
            resolveNormal = (dy1 < dy2) ? Vec3(0, 1, 0) : Vec3(0, -1, 0);
        }
        if (overlapZ < minOverlap) {
            minOverlap = overlapZ;
            resolveNormal = (dz1 < dz2) ? Vec3(0, 0, 1) : Vec3(0, 0, -1);
        }

        body.position += resolveNormal * (minOverlap + 0.002f);

        float vn = Vec3::dot(body.linearVelocity, resolveNormal);
        if (vn < 0.0f) {
            float e = (std::abs(vn) < 0.2f) ? 0.0f : body.material.restitution;
            body.linearVelocity -= resolveNormal * (vn * (1.0f + e));
            body.angularVelocity = body.angularVelocity * 0.82f;
        }
    }

    void PhysicsWorld::resolveBodyVsBodyCollision(RigidBody& a, RigidBody& b, float /*dt*/) {
        Vec3 delta = b.position - a.position;
        float dSq = delta.lengthSq();
        float ra = std::max({ a.halfExtents.x, a.halfExtents.y, a.halfExtents.z });
        float rb = std::max({ b.halfExtents.x, b.halfExtents.y, b.halfExtents.z });
        float minDist = ra + rb;

        if (dSq < (minDist * minDist) && dSq > 0.0001f) {
            float d = std::sqrt(dSq);
            Vec3 n = delta / d;
            float pen = minDist - d;

            float totalMass = a.mass + b.mass;
            if (totalMass > 0.001f) {
                a.position -= n * (pen * (b.mass / totalMass));
                b.position += n * (pen * (a.mass / totalMass));

                Vec3 relVel = b.linearVelocity - a.linearVelocity;
                float vn = Vec3::dot(relVel, n);
                if (vn < 0.0f) {
                    float e = std::min(a.material.restitution, b.material.restitution);
                    float j = -(1.0f + e) * vn / (a.invMass + b.invMass);
                    Vec3 imp = n * j;
                    a.linearVelocity -= imp * a.invMass;
                    b.linearVelocity += imp * b.invMass;
                    a.isSleeping = false;
                    b.isSleeping = false;
                }
            }
        }
    }

    void PhysicsWorld::update(float dt, const std::vector<CollisionBox>& obstacles) {
        if (dt <= 0.0f) return;
        float clampedDt = std::min(dt, 0.05f);
        const int subSteps = 2;
        float subDt = clampedDt / (float)subSteps;

        for (int step = 0; step < subSteps; ++step) {
            for (size_t i = 0; i < bodies.size(); ++i) {
                auto& body = bodies[i];
                if (body->isDestroyed || body->isStatic || body->isSleeping) continue;

                // 1. Apply gravity & damping
                body->linearVelocity += gravity * subDt;
                body->linearVelocity *= std::pow(body->linearDamping, subDt * 60.0f);
                body->angularVelocity *= std::pow(body->angularDamping, subDt * 60.0f);

                // 2. Position integration
                body->position += body->linearVelocity * subDt;

                // 3. Orientation integration
                float angSpeed = body->angularVelocity.length();
                if (angSpeed > 1e-5f) {
                    Vec3 axis = body->angularVelocity / angSpeed;
                    Quat dQ = Quat::fromAxisAngle(axis, angSpeed * subDt);
                    body->orientation = (dQ * body->orientation).normalized();
                }

                // 4. Ground Collision
                resolveGroundCollision(*body, subDt);

                // 5. Solid Obstacle Collision
                for (const auto& obs : obstacles) {
                    resolveObstacleCollision(*body, obs, subDt);
                }

                // 6. Sleeping Check
                if (body->linearVelocity.lengthSq() < 0.015f && body->angularVelocity.lengthSq() < 0.015f) {
                    body->sleepTimer += subDt;
                    if (body->sleepTimer > 0.45f) {
                        body->isSleeping = true;
                        body->linearVelocity = Vec3(0, 0, 0);
                        body->angularVelocity = Vec3(0, 0, 0);
                    }
                } else {
                    body->sleepTimer = 0.0f;
                }

                // 7. Debris Lifetime
                if (body->type == PropType::DebrisChunk) {
                    body->lifetime += subDt;
                    if (body->lifetime >= body->maxLifetime) {
                        body->isDestroyed = true;
                    }
                }
            }

            // Body vs Body collision resolution
            for (size_t i = 0; i < bodies.size(); ++i) {
                if (bodies[i]->isDestroyed || bodies[i]->isStatic) continue;
                for (size_t j = i + 1; j < bodies.size(); ++j) {
                    if (bodies[j]->isDestroyed || bodies[j]->isStatic) continue;
                    resolveBodyVsBodyCollision(*bodies[i], *bodies[j], subDt);
                }
            }
        }

        // Clean up expired destroyed bodies
        bodies.erase(std::remove_if(bodies.begin(), bodies.end(),
            [](const std::unique_ptr<RigidBody>& b) {
                return b->isDestroyed;
            }), bodies.end());
    }

    bool PhysicsWorld::raycast(const Vec3& rayOrigin, const Vec3& rayDir, RaycastHit& outHit, int excludeId) {
        bool hitAny = false;
        for (const auto& b : bodies) {
            if (b->isDestroyed || b->id == excludeId) continue;

            float dist = 0.0f;
            Vec3 norm;
            if (b->raycast(rayOrigin, rayDir, dist, norm)) {
                if (dist < outHit.distance) {
                    outHit.hit = true;
                    outHit.distance = dist;
                    outHit.point = rayOrigin + rayDir * dist;
                    outHit.normal = norm;
                    outHit.tag = EntityTag::RigidProp;
                    outHit.entityIndex = b->id;
                    outHit.isHeadshot = false;
                    hitAny = true;
                }
            }
        }
        return hitAny;
    }

    void PhysicsWorld::render() const {
        for (const auto& b : bodies) {
            if (b->isDestroyed) continue;

            const Texture* tex = nullptr;
            if (b->type == PropType::Crate) {
                tex = crateTexture.get();
            } else if (b->type == PropType::ExplosiveBarrel) {
                tex = barrelTexture.get();
            } else if (b->type == PropType::DebrisChunk) {
                tex = debrisTexture.get();
            }

            Mat4 model = b->getTransformMatrix();
            Renderer::drawCube(model, b->color, tex, true, { 1.0f, 1.0f }, 1);
        }
    }

    void PhysicsWorld::renderShadow() const {
        for (const auto& b : bodies) {
            if (b->isDestroyed) continue;
            Renderer::drawShadowCube(b->getTransformMatrix());
        }
    }

} // namespace Lab
