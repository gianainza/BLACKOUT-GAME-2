#include "Scene.h"
#include <cmath>
#include <algorithm>

// ------------------------------------------------------------------ Bulb
void Bulb::Finalise()
{
    if (scanner) { Reset(); return; }
    pathLen = 0.0f;
    for (size_t i = 0; i < path.size(); ++i)
        pathLen += glm::length(path[(i + 1) % path.size()] - path[i]);
    Place();
}

void Bulb::Reset()
{
    s = 0.0f;
    state = ScanState::Scanning;
    detect = 0.0f; stateTime = 0.0f; power = 1.0f;
    colour = baseColour;
    if (scanner)
    {
        routeId = 0;
        wp = (route[0].size() > 1) ? 1 : 0;
        aim = route[0].empty() ? glm::vec2(mount.x, mount.z) : route[0][0];
    }
    Place();
}

glm::vec3 Bulb::Dir() const
{
    glm::vec3 d = glm::vec3(aim.x, 0.0f, aim.y) - position;
    float l = glm::length(d);
    return (l > 1e-5f) ? d / l : glm::vec3(0.0f, -1.0f, 0.0f);
}

void Bulb::SweepAim(float dt, float aimSpeed)
{
    const std::vector<glm::vec2>& r = route[routeId];
    if (r.empty()) return;
    glm::vec2 target = r[wp % r.size()];
    glm::vec2 d = target - aim;
    float len = glm::length(d), step = aimSpeed * dt;
    if (len <= step) { aim = target; wp = (wp + 1) % r.size(); }
    else             aim += d / len * step;
}

void Bulb::SwitchRoute()
{
    int other = 1 - routeId;
    if (route[other].empty()) return;          // nothing to switch to
    routeId = other;
    float best = 1e30f; size_t bi = 0;         // resume from the waypoint nearest to where the beam is now
    for (size_t i = 0; i < route[routeId].size(); ++i)
    {
        float d = glm::length(route[routeId][i] - aim);
        if (d < best) { best = d; bi = i; }
    }
    wp = bi;
}

void Bulb::Place()
{
    if (scanner) { position = mount; return; }
    if (path.empty()) return;
    if (path.size() == 1 || pathLen <= 1e-6f) { position = path[0]; return; }
    float d = std::fmod(s, pathLen);
    if (d < 0.0f) d += pathLen;
    for (size_t i = 0; i < path.size(); ++i)
    {
        const glm::vec3& a = path[i];
        const glm::vec3& b = path[(i + 1) % path.size()];
        float seg = glm::length(b - a);
        if (d <= seg || i + 1 == path.size())
        {
            float t = (seg > 1e-6f) ? glm::clamp(d / seg, 0.0f, 1.0f) : 0.0f;
            position = a + (b - a) * t;
            return;
        }
        d -= seg;
    }
}

// ------------------------------------------------------------------ helpers
bool PointInsideSolid(const Scene& sc, float x, float z, float margin)
{
    for (const auto& o : sc.objects)
    {
        if (!o.solid) continue;
        if (x > o.minB.x - margin && x < o.maxB.x + margin && z > o.minB.z - margin && z < o.maxB.z + margin) return true;
    }
    return false;
}
