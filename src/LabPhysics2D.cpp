#include "LabPhysics2D.h"
#include <limits>

namespace Lab {

    PhysicsWorld2D::PhysicsWorld2D(const Vec2& gravity)
        : _gravity(gravity) {
        _bodies.reserve(1024);
    }

    size_t PhysicsWorld2D::createBoxBody(BodyType2D type, const Vec2& position, const Vec2& size, float mass) {
        RigidBody2D body;
        body.id = _nextId++;
        body.type = type;
        body.colliderType = ColliderType2D::Box;
        body.position = position;
        body.boxSize = size;
        body.setMass(mass);

        _bodies.push_back(body);
        return body.id;
    }

    size_t PhysicsWorld2D::createCircleBody(BodyType2D type, const Vec2& position, float radius, float mass) {
        RigidBody2D body;
        body.id = _nextId++;
        body.type = type;
        body.colliderType = ColliderType2D::Circle;
        body.position = position;
        body.circleRadius = radius;
        body.setMass(mass);

        _bodies.push_back(body);
        return body.id;
    }

    RigidBody2D* PhysicsWorld2D::getBody(size_t id) {
        for (auto& b : _bodies) {
            if (b.id == id) return &b;
        }
        return nullptr;
    }

    const RigidBody2D* PhysicsWorld2D::getBody(size_t id) const {
        for (const auto& b : _bodies) {
            if (b.id == id) return &b;
        }
        return nullptr;
    }

    void PhysicsWorld2D::clear() {
        _bodies.clear();
        _nextId = 1;
    }

    void PhysicsWorld2D::step(float dt, int velocityIterations, int positionIterations) {
        if (dt <= 0.0f) return;

        // 1. Integrate forces and gravity to velocities
        for (auto& body : _bodies) {
            if (body.type != BodyType2D::Dynamic) continue;

            body.velocity += (_gravity + body.force * body.invMass) * dt;
            body.angularVelocity += (body.torque * body.invInertia) * dt;

            // Simple linear and angular damping
            body.velocity *= 0.998f;
            body.angularVelocity *= 0.995f;

            body.force = Vec2(0.0f, 0.0f);
            body.torque = 0.0f;
        }

        // 2. Collision detection
        std::vector<Contact2D> contacts;
        detectCollisions(contacts);

        // 3. Iterative velocity resolution
        for (int iter = 0; iter < velocityIterations; ++iter) {
            resolveContacts(contacts);
        }

        // 4. Integrate positions
        for (auto& body : _bodies) {
            if (body.type == BodyType2D::Static) continue;

            body.position += body.velocity * dt;
            body.rotation += body.angularVelocity * dt;
        }

        // 5. Positional correction (penetration slop resolution)
        for (int iter = 0; iter < positionIterations; ++iter) {
            positionalCorrection(contacts);
        }
    }

    void PhysicsWorld2D::detectCollisions(std::vector<Contact2D>& contacts) {
        contacts.clear();
        size_t n = _bodies.size();

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                const auto& a = _bodies[i];
                const auto& b = _bodies[j];

                // Static bodies do not collide with each other
                if (a.type == BodyType2D::Static && b.type == BodyType2D::Static) continue;

                // Broadphase: AABB overlap check
                if (!a.getAABB().overlaps(b.getAABB())) continue;

                // Narrowphase
                Contact2D contact;
                contact.bodyA = i;
                contact.bodyB = j;
                if (checkCollision(a, b, contact)) {
                    contacts.push_back(contact);
                }
            }
        }
    }

    bool PhysicsWorld2D::checkCollision(const RigidBody2D& a, const RigidBody2D& b, Contact2D& contact) {
        // Case 1: Circle vs Circle
        if (a.colliderType == ColliderType2D::Circle && b.colliderType == ColliderType2D::Circle) {
            Vec2 delta = b.position - a.position;
            float distSq = delta.lengthSquared();
            float rSum = a.circleRadius + b.circleRadius;

            if (distSq > rSum * rSum) return false;

            float dist = std::sqrt(distSq);
            if (dist > 0.0001f) {
                contact.normal = delta / dist;
                contact.penetration = rSum - dist;
            } else {
                contact.normal = Vec2(0.0f, 1.0f);
                contact.penetration = a.circleRadius;
            }
            return true;
        }

        // Case 2: Box vs Box (AABB)
        if (a.colliderType == ColliderType2D::Box && b.colliderType == ColliderType2D::Box) {
            Vec2 delta = b.position - a.position;
            Vec2 aHalf = a.boxSize * 0.5f;
            Vec2 bHalf = b.boxSize * 0.5f;

            float overlapX = (aHalf.x + bHalf.x) - std::abs(delta.x);
            if (overlapX <= 0.0f) return false;

            float overlapY = (aHalf.y + bHalf.y) - std::abs(delta.y);
            if (overlapY <= 0.0f) return false;

            // Smallest penetration axis determines normal
            if (overlapX < overlapY) {
                contact.normal = (delta.x > 0.0f) ? Vec2(1.0f, 0.0f) : Vec2(-1.0f, 0.0f);
                contact.penetration = overlapX;
            } else {
                contact.normal = (delta.y > 0.0f) ? Vec2(0.0f, 1.0f) : Vec2(0.0f, -1.0f);
                contact.penetration = overlapY;
            }
            return true;
        }

        // Case 3: Circle vs Box
        const RigidBody2D* circleBody = (a.colliderType == ColliderType2D::Circle) ? &a : &b;
        const RigidBody2D* boxBody = (a.colliderType == ColliderType2D::Box) ? &a : &b;
        bool reversed = (a.colliderType == ColliderType2D::Box);

        Vec2 boxHalf = boxBody->boxSize * 0.5f;
        Vec2 relPos = circleBody->position - boxBody->position;

        // Find closest point on box to circle center
        Vec2 closest(
            std::clamp(relPos.x, -boxHalf.x, boxHalf.x),
            std::clamp(relPos.y, -boxHalf.y, boxHalf.y)
        );

        Vec2 diff = relPos - closest;
        float distSq = diff.lengthSquared();

        if (distSq > circleBody->circleRadius * circleBody->circleRadius) return false;

        float dist = std::sqrt(distSq);
        Vec2 norm;
        float pen;

        if (dist > 0.0001f) {
            norm = diff / dist;
            pen = circleBody->circleRadius - dist;
        } else {
            // Circle center is inside box - push along nearest face
            float dx = boxHalf.x - std::abs(relPos.x);
            float dy = boxHalf.y - std::abs(relPos.y);
            if (dx < dy) {
                norm = (relPos.x > 0.0f) ? Vec2(1.0f, 0.0f) : Vec2(-1.0f, 0.0f);
                pen = dx + circleBody->circleRadius;
            } else {
                norm = (relPos.y > 0.0f) ? Vec2(0.0f, 1.0f) : Vec2(0.0f, -1.0f);
                pen = dy + circleBody->circleRadius;
            }
        }

        if (reversed) {
            contact.normal = norm; // From Box (A) to Circle (B)
        } else {
            contact.normal = norm * -1.0f; // From Circle (A) to Box (B)
        }
        contact.penetration = pen;
        return true;
    }

    void PhysicsWorld2D::resolveContacts(const std::vector<Contact2D>& contacts) {
        for (const auto& c : contacts) {
            auto& a = _bodies[c.bodyA];
            auto& b = _bodies[c.bodyB];

            Vec2 rv = b.velocity - a.velocity;
            float velAlongNormal = Vec2::dot(rv, c.normal);

            // Separating velocity check
            if (velAlongNormal > 0.0f) continue;

            float restitution = std::min(a.restitution, b.restitution);
            float totalInvMass = a.invMass + b.invMass;
            if (totalInvMass <= 0.0f) continue;

            // Normal impulse magnitude
            float j = -(1.0f + restitution) * velAlongNormal / totalInvMass;
            Vec2 impulse = c.normal * j;

            if (a.type == BodyType2D::Dynamic) a.velocity -= impulse * a.invMass;
            if (b.type == BodyType2D::Dynamic) b.velocity += impulse * b.invMass;

            // Friction impulse along tangent
            Vec2 tangent = rv - c.normal * velAlongNormal;
            float tangentLen = tangent.length();
            if (tangentLen > 0.0001f) {
                tangent = tangent / tangentLen;
                float friction = std::sqrt(a.friction * b.friction);
                float jt = -Vec2::dot(rv, tangent) / totalInvMass;
                jt = std::clamp(jt, -j * friction, j * friction);

                Vec2 frictionImpulse = tangent * jt;
                if (a.type == BodyType2D::Dynamic) a.velocity -= frictionImpulse * a.invMass;
                if (b.type == BodyType2D::Dynamic) b.velocity += frictionImpulse * b.invMass;
            }
        }
    }

    void PhysicsWorld2D::positionalCorrection(const std::vector<Contact2D>& contacts) {
        const float percent = 0.4f; // Penetration percentage to correct
        const float slop = 0.01f;    // Penetration allowance

        for (const auto& c : contacts) {
            auto& a = _bodies[c.bodyA];
            auto& b = _bodies[c.bodyB];

            float totalInvMass = a.invMass + b.invMass;
            if (totalInvMass <= 0.0f) continue;

            float penetration = std::max(c.penetration - slop, 0.0f);
            Vec2 correction = c.normal * (penetration / totalInvMass * percent);

            if (a.type == BodyType2D::Dynamic) a.position -= correction * a.invMass;
            if (b.type == BodyType2D::Dynamic) b.position += correction * b.invMass;
        }
    }

    bool PhysicsWorld2D::raycast(const Vec2& start, const Vec2& direction, float maxDist, RaycastHit2D& outHit) const {
        if (maxDist <= 0.0f) return false;
        Vec2 dir = direction.normalized();

        bool hitFound = false;
        float closestDist = maxDist;

        for (const auto& body : _bodies) {
            if (body.colliderType == ColliderType2D::Circle) {
                Vec2 m = start - body.position;
                float b = Vec2::dot(m, dir);
                float c = Vec2::dot(m, m) - body.circleRadius * body.circleRadius;

                if (c > 0.0f && b > 0.0f) continue;

                float discr = b * b - c;
                if (discr < 0.0f) continue;

                float t = -b - std::sqrt(discr);
                if (t < 0.0f) t = 0.0f;

                if (t <= closestDist) {
                    closestDist = t;
                    hitFound = true;
                    outHit.hit = true;
                    outHit.fraction = t / maxDist;
                    outHit.point = start + dir * t;
                    outHit.normal = (outHit.point - body.position).normalized();
                    outHit.bodyId = body.id;
                }
            } else if (body.colliderType == ColliderType2D::Box) {
                // Ray vs AABB Slab Method
                Vec2 half = body.boxSize * 0.5f;
                Vec2 boxMin = body.position - half;
                Vec2 boxMax = body.position + half;

                float tMin = 0.0f;
                float tMax = closestDist;
                Vec2 hitNormal(0.0f, 0.0f);

                bool intersect = true;
                // Check X slab
                if (std::abs(dir.x) < 1e-6f) {
                    if (start.x < boxMin.x || start.x > boxMax.x) intersect = false;
                } else {
                    float ood = 1.0f / dir.x;
                    float t1 = (boxMin.x - start.x) * ood;
                    float t2 = (boxMax.x - start.x) * ood;
                    Vec2 n1(-1.0f, 0.0f);
                    Vec2 n2(1.0f, 0.0f);
                    if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
                    if (t1 > tMin) { tMin = t1; hitNormal = n1; }
                    if (t2 < tMax) tMax = t2;
                    if (tMin > tMax) intersect = false;
                }

                // Check Y slab
                if (intersect) {
                    if (std::abs(dir.y) < 1e-6f) {
                        if (start.y < boxMin.y || start.y > boxMax.y) intersect = false;
                    } else {
                        float ood = 1.0f / dir.y;
                        float t1 = (boxMin.y - start.y) * ood;
                        float t2 = (boxMax.y - start.y) * ood;
                        Vec2 n1(0.0f, -1.0f);
                        Vec2 n2(0.0f, 1.0f);
                        if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
                        if (t1 > tMin) { tMin = t1; hitNormal = n1; }
                        if (t2 < tMax) tMax = t2;
                        if (tMin > tMax) intersect = false;
                    }
                }

                if (intersect && tMin <= closestDist) {
                    closestDist = tMin;
                    hitFound = true;
                    outHit.hit = true;
                    outHit.fraction = tMin / maxDist;
                    outHit.point = start + dir * tMin;
                    outHit.normal = hitNormal;
                    outHit.bodyId = body.id;
                }
            }
        }

        return hitFound;
    }

} // namespace Lab
