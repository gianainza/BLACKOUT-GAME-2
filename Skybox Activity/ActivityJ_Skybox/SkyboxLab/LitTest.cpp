#include "LitTest.h"
#include <cmath>
#include <algorithm>

float BulbAttenuation(const Bulb& b, float d)
{
    return b.attE * d * d + b.attL * d + b.attC;
}

// slab test of the segment a->b against an axis-aligned box
static bool SegmentHitsBox(const glm::vec3& a, const glm::vec3& b, const glm::vec3& mn, const glm::vec3& mx)
{
    glm::vec3 d = b - a;
    float t0 = 0.0f, t1 = 1.0f;
    for (int k = 0; k < 3; ++k)
    {
        if (std::fabs(d[k]) < 1e-9f)
        {
            if (a[k] < mn[k] || a[k] > mx[k]) return false;
        }
        else
        {
            float inv = 1.0f / d[k];
            float ta = (mn[k] - a[k]) * inv, tb = (mx[k] - a[k]) * inv;
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) return false;
        }
    }
    return true;
}

bool SegmentBlocked(const Scene& sc, const glm::vec3& a, const glm::vec3& b)
{
    // the blockers are EXACTLY the objects that are drawn into the cube maps
    for (const auto& o : sc.objects)
        if (o.castShadow && SegmentHitsBox(a, b, o.minB, o.maxB)) return true;
    return false;
}

float ConeFactor(const Bulb& b, const glm::vec3& p)
{
    if (!b.scanner) return 1.0f;
    glm::vec3 v = p - b.position;
    float d = glm::length(v);
    if (d < 1e-5f) return 1.0f;
    float c = glm::dot(v / d, b.Dir());
    if (c <= b.edgeCos) return 0.0f;
    return 1.0f - (1.0f - c) * (1.0f / (1.0f - b.edgeCos));      // identical to CalcSpotLight in shader.frag
}

LitResult ComputeIllumination(const Scene& sc, const glm::vec3& p, bool lineOfSight)
{
    LitResult r;
    for (size_t i = 0; i < sc.bulbs.size() && i < (size_t)MAX_GAME_LIGHTS; ++i)
    {
        const Bulb& b = sc.bulbs[i];
        float d = glm::length(p - b.position);
        if (d > b.farPlane) continue;
        float I = b.diffuse * b.power;
        if (I <= 0.0f) continue;
        float cone = ConeFactor(b, p);
        if (cone <= 0.0f) continue;
        if (lineOfSight && SegmentBlocked(sc, p, b.position)) continue;
        float c = cone * I / BulbAttenuation(b, d);
        r.visMask |= (1u << i);
        r.per[i] = c;
        r.illum += c;
    }
    return r;
}

bool ExposureEllipse(const Scene& sc, const Bulb& b, glm::vec2& centre, float& yaw, float& semiA, float& semiB)
{
    if (!b.scanner || b.power <= 0.0f) return false;

    glm::vec2 u = b.aim - glm::vec2(b.mount.x, b.mount.z);
    float ul = glm::length(u);
    u = (ul > 1e-4f) ? u / ul : glm::vec2(1.0f, 0.0f);
    const glm::vec2 v(-u.y, u.x);

    // how far from the aim point, along a direction, the contribution stays >= litThreshold
    auto reach = [&](const glm::vec2& dir) -> float
    {
        float last = -1.0f;
        for (float t = 0.0f; t <= 12.0f; t += 0.05f)
        {
            glm::vec2 q = b.aim + dir * t;
            glm::vec3 p(q.x, SAMPLE_HEIGHT, q.y);
            float d = glm::length(p - b.position);
            if (d > b.farPlane) break;
            float c = ConeFactor(b, p) * b.diffuse * b.power / BulbAttenuation(b, d);
            if (c >= sc.litThreshold) last = t;
            else if (t > last + 0.3f) break;
        }
        return last;
    };

    float rFar = reach(u), rNear = reach(-u);
    if (rFar < 0.0f && rNear < 0.0f) return false;            // the aim point itself is not "seen" (too far / too dim)
    rFar = std::max(rFar, 0.0f); rNear = std::max(rNear, 0.0f);
    float rSide = 0.5f * (std::max(reach(v), 0.0f) + std::max(reach(-v), 0.0f));

    semiA = std::max(0.5f * (rFar + rNear), 0.15f);
    semiB = std::max(rSide, 0.15f);
    centre = b.aim + u * (0.5f * (rFar - rNear));
    yaw = std::atan2(-u.y, u.x);                               // rotation about Y that turns local +x into u
    return true;
}
