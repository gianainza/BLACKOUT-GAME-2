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
