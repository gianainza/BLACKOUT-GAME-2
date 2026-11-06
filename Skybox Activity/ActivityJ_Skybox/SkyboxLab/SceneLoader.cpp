#include "SceneLoader.h"
#include "Json.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
    struct Ctx
    {
        Scene& sc;
        std::string file;
        int errors = 0;

        void Say(const char* kind, int line, const std::string& where, const std::string& msg)
        {
            std::string m = file + ":" + std::to_string(line) + ": " + kind + " " + where + (where.empty() ? "" : ": ") + msg;
            sc.messages.push_back(m);
            std::cerr << "[SCENE] " << m << "\n";
            if (std::string(kind) == "ERROR") ++errors;
        }
        void Err(int line, const std::string& where, const std::string& msg) { Say("ERROR", line, where, msg); }
        void Warn(int line, const std::string& where, const std::string& msg) { Say("warning", line, where, msg); }

        // ---- typed getters. All return false (and report) on a wrong type / out of range value.
        bool Num(const JsonValue& o, const char* key, float& out, bool required, const std::string& where,
            float lo = -1e30f, float hi = 1e30f)
        {
            const JsonValue* v = o.Find(key);
            if (!v) { if (required) Err(o.line, where, std::string("missing required number '") + key + "'"); return !required; }
            if (!v->IsNumber() || v->num < lo || v->num > hi)
            {
                Err(v->line, where, std::string("'") + key + "' must be a number" +
                    ((lo > -1e29f || hi < 1e29f) ? " between " + Trim(lo) + " and " + Trim(hi) : std::string()));
                return false;
            }
            out = (float)v->num;
            return true;
        }
        static std::string Trim(float f) { char b[32]; std::snprintf(b, sizeof b, "%g", f); return b; }

        bool Vec(const JsonValue& o, const char* key, float* out, int n, bool required, const std::string& where)
        {
            const JsonValue* v = o.Find(key);
            if (!v) { if (required) Err(o.line, where, std::string("missing required array '") + key + "'"); return !required; }
            if (!v->IsArray() || (int)v->arr.size() != n)
            {
                Err(v->line, where, std::string("'") + key + "' must be an array of exactly " + std::to_string(n) + " numbers"); return false;
            }
            for (int i = 0; i < n; ++i)
            {
                if (!v->arr[i].IsNumber()) { Err(v->arr[i].line, where, std::string("'") + key + "[" + std::to_string(i) + "]' is not a number"); return false; }
                out[i] = (float)v->arr[i].num;
            }
            return true;
        }

        bool Str(const JsonValue& o, const char* key, std::string& out, bool required, const std::string& where)
        {
            const JsonValue* v = o.Find(key);
            if (!v) { if (required) Err(o.line, where, std::string("missing required string '") + key + "'"); return !required; }
            if (!v->IsString()) { Err(v->line, where, std::string("'") + key + "' must be a string"); return false; }
            out = v->str;
            return true;
        }

        bool Bool(const JsonValue& o, const char* key, bool& out, const std::string& where)
        {
            const JsonValue* v = o.Find(key);
            if (!v) return true;
            if (v->type != JsonValue::Bool) { Err(v->line, where, std::string("'") + key + "' must be true or false"); return false; }
            out = v->b;
            return true;
        }

        bool RouteList(const JsonValue& o, const char* key, std::vector<glm::vec2>& out, const std::string& where)
        {
            const JsonValue* v = o.Find(key);
            if (!v) return true;
            if (!v->IsArray()) { Err(v->line, where, std::string("'") + key + "' must be an array of [x, z] points"); return false; }
            for (const auto& p : v->arr)
            {
                if (!p.IsArray() || p.arr.size() != 2 || !p.arr[0].IsNumber() || !p.arr[1].IsNumber())
                {
                    Err(p.line, where, std::string("'") + key + "' entries must be [x, z] pairs of numbers"); return false;
                }
                out.push_back(glm::vec2((float)p.arr[0].num, (float)p.arr[1].num));
            }
            return true;
        }

        const JsonValue* Section(const JsonValue& root, const char* key, JsonValue::Type t, const char* what)
        {
            const JsonValue* v = root.Find(key);
            if (!v) return nullptr;
            if (v->type != t) { Err(v->line, key, std::string("must be ") + what); return nullptr; }
            return v;
        }
    };

    bool FileExists(const std::string& p) { std::ifstream f(p, std::ios::binary); return f.good(); }

    std::string Ident(const JsonValue& o, const char* kind, size_t i)
    {
        const JsonValue* id = o.Find("id");
        std::string s = std::string(kind) + "[" + std::to_string(i) + "]";
        if (id && id->IsString()) s += " '" + id->str + "'";
        return s;
    }

    void LoadMaterials(Ctx& c, const JsonValue& root)
    {
        const JsonValue* arr = c.Section(root, "materials", JsonValue::Array, "an array");
        if (arr)
            for (size_t i = 0; i < arr->arr.size(); ++i)
            {
                const JsonValue& m = arr->arr[i];
                std::string w = Ident(m, "materials", i);
                if (!m.IsObject()) { c.Err(m.line, w, "must be an object"); continue; }
                MaterialDef d;
                bool ok = c.Str(m, "id", d.id, true, w) & c.Str(m, "texture_path", d.texturePath, true, w);
                ok &= c.Num(m, "specular_intensity", d.specular, false, w, 0.0f, 20.0f);
                ok &= c.Num(m, "shininess", d.shininess, false, w, 1.0f, 1024.0f);
                ok &= c.Num(m, "emissive", d.emissive, false, w, 0.0f, 10.0f);
                ok &= c.Num(m, "tile", d.tile, false, w, 0.1f, 100.0f);
                float col[3] = { 1, 1, 1 };
                ok &= c.Vec(m, "colour", col, 3, false, w);
                d.colour = glm::vec3(col[0], col[1], col[2]);
                if (!ok) continue;
                if (c.sc.FindMaterial(d.id) >= 0) { c.Err(m.line, w, "duplicate material id"); continue; }
                if (!FileExists(d.texturePath)) c.Warn(m.line, w, "texture file '" + d.texturePath + "' not found - a magenta checker will be used instead");
                c.sc.materials.push_back(d);
            }
        if (c.sc.materials.empty())
        {
            c.Warn(1, "materials", "no valid materials: adding a plain grey default");
            MaterialDef d; d.id = "default"; d.texturePath = "Textures/plain.png"; c.sc.materials.push_back(d);
        }
    }

    void LoadLights(Ctx& c, const JsonValue& root)
    {
        Scene& sc = c.sc;
        const JsonValue* lights = c.Section(root, "lights", JsonValue::Object, "an object");
        if (!lights) { c.Err(1, "lights", "section is missing: the level has no lights"); return; }

        if (const JsonValue* d = c.Section(*lights, "directional_light", JsonValue::Object, "an object"))
        {
            float dir[3] = { sc.dirDirection.x, sc.dirDirection.y, sc.dirDirection.z }, col[3] = { sc.dirColour.x, sc.dirColour.y, sc.dirColour.z };
            bool ok = c.Vec(*d, "direction", dir, 3, true, "lights.directional_light") & c.Vec(*d, "color", col, 3, true, "lights.directional_light");
            ok &= c.Num(*d, "ambient_intensity", sc.dirAmbient, true, "lights.directional_light", 0.0f, 10.0f);
            ok &= c.Num(*d, "diffuse_intensity", sc.dirDiffuse, true, "lights.directional_light", 0.0f, 10.0f);
            if (ok && glm::length(glm::vec3(dir[0], dir[1], dir[2])) > 1e-4f) { sc.dirDirection = glm::vec3(dir[0], dir[1], dir[2]); sc.dirColour = glm::vec3(col[0], col[1], col[2]); }
            else if (ok) c.Err(d->line, "lights.directional_light", "'direction' cannot be [0,0,0]");
        }

        // ---- drones: patrolling point lights ----
        std::vector<Bulb> drones, scanners;
        if (const JsonValue* pl = c.Section(*lights, "point_lights", JsonValue::Array, "an array"))
            for (size_t i = 0; i < pl->arr.size(); ++i)
            {
                const JsonValue& o = pl->arr[i];
                std::string w = "lights." + Ident(o, "point_lights", i);
                if (!o.IsObject()) { c.Err(o.line, w, "must be an object"); continue; }
                if ((int)drones.size() >= MAX_POINT_LIGHTS) { c.Err(o.line, w, "too many point lights (MAX_POINT_LIGHTS = " + std::to_string(MAX_POINT_LIGHTS) + " in CommonValues.h and shader.frag)"); continue; }
                Bulb b; b.line = o.line;
                float col[3], att[3];
                bool ok = c.Str(o, "id", b.name, true, w) & c.Vec(o, "color", col, 3, true, w) & c.Vec(o, "attenuation", att, 3, true, w);
                ok &= c.Num(o, "ambient_intensity", b.ambient, true, w, 0.0f, 10.0f) & c.Num(o, "diffuse_intensity", b.diffuse, true, w, 0.0f, 50.0f);
                ok &= c.Num(o, "patrol_speed", b.speed, true, w, 0.0f, 50.0f);
                ok &= c.Num(o, "far_plane", b.farPlane, false, w, 5.0f, 500.0f);
                if (const JsonValue* pp = o.Find("patrol_path"))
                {
                    if (!pp->IsArray() || pp->arr.empty()) { c.Err(pp->line, w, "'patrol_path' must be a non-empty array of [x, y, z] points"); ok = false; }
                    else for (const auto& p : pp->arr)
                    {
                        if (!p.IsArray() || p.arr.size() != 3 || !p.arr[0].IsNumber() || !p.arr[1].IsNumber() || !p.arr[2].IsNumber())
                        {
                            c.Err(p.line, w, "'patrol_path' points must be [x, y, z] numbers"); ok = false; break;
                        }
                        b.path.push_back(glm::vec3((float)p.arr[0].num, (float)p.arr[1].num, (float)p.arr[2].num));
                    }
                }
                else { c.Err(o.line, w, "missing required array 'patrol_path'"); ok = false; }
                if (!ok) continue;
                b.baseColour = b.colour = glm::vec3(col[0], col[1], col[2]);
                b.attC = att[0]; b.attL = att[1]; b.attE = att[2];
                if (b.attC <= 0.0f && b.attL <= 0.0f && b.attE <= 0.0f) { c.Err(o.line, w, "attenuation [constant, linear, exponent] cannot be all zero (division by zero)"); continue; }
                b.Finalise();
                drones.push_back(b);
            }

        // ---- scanners: cone (spot) lights with a swept aim point ----
        if (const JsonValue* sl = c.Section(*lights, "spot_lights", JsonValue::Array, "an array"))
            for (size_t i = 0; i < sl->arr.size(); ++i)
            {
                const JsonValue& o = sl->arr[i];
                std::string w = "lights." + Ident(o, "spot_lights", i);
                if (!o.IsObject()) { c.Err(o.line, w, "must be an object"); continue; }
                if ((int)scanners.size() >= MAX_SPOT_LIGHTS - 1) { c.Err(o.line, w, "too many scanners (MAX_SPOT_LIGHTS - 1 = " + std::to_string(MAX_SPOT_LIGHTS - 1) + "; spot 0 is the droid's lamp)"); continue; }
                Bulb b; b.scanner = true; b.line = o.line;
                float pos[3], col[3], att[3] = { 1.0f, 0.09f, 0.032f }, dir[3] = { 0, -1, 0 };
                bool ok = c.Str(o, "id", b.name, false, w) & c.Vec(o, "position", pos, 3, true, w) & c.Vec(o, "color", col, 3, true, w);
                ok &= c.Vec(o, "attenuation", att, 3, false, w) & c.Vec(o, "direction", dir, 3, false, w);
                if (b.name.empty()) b.name = "spot_" + std::to_string(i);
                ok &= c.Num(o, "ambient_intensity", b.ambient, true, w, 0.0f, 10.0f) & c.Num(o, "diffuse_intensity", b.diffuse, true, w, 0.0f, 50.0f);
                ok &= c.Num(o, "edge", b.edgeDeg, true, w, 3.0f, 80.0f);
                ok &= c.Num(o, "far_plane", b.farPlane, false, w, 5.0f, 500.0f);
                float aimSpeed = 3.0f;
                if (const JsonValue* sc2 = o.Find("scanner"))
                {
                    if (!sc2->IsObject()) { c.Err(sc2->line, w, "'scanner' must be an object"); ok = false; }
                    else
                    {
                        ok &= c.Num(*sc2, "aim_speed", aimSpeed, false, w, 0.0f, 50.0f);
                        ok &= c.RouteList(*sc2, "route_a", b.route[0], w) & c.RouteList(*sc2, "route_b", b.route[1], w);
                    }
                }
                if (!ok) continue;
                b.speed = aimSpeed;
                b.baseColour = b.colour = glm::vec3(col[0], col[1], col[2]);
                b.attC = att[0]; b.attL = att[1]; b.attE = att[2];
                if (b.attC <= 0.0f && b.attL <= 0.0f && b.attE <= 0.0f) { c.Err(o.line, w, "attenuation cannot be all zero"); continue; }
                b.mount = glm::vec3(pos[0], pos[1], pos[2]);
                if (b.mount.y < 1.0f) { c.Err(o.line, w, "scanner 'position' y must be at least 1 (it hangs above the floor)"); continue; }
                b.edgeCos = std::cos(glm::radians(b.edgeDeg));
                if (b.route[0].empty())
                {   // no route given: a fixed beam along 'direction' (needs to point down so it reaches the floor)
                    if (dir[1] >= -1e-3f) { c.Err(o.line, w, "no scanner.route_a and 'direction' does not point down, so there is nowhere for the beam to land"); continue; }
                    float t = b.mount.y / -dir[1];
                    b.route[0].push_back(glm::vec2(b.mount.x + dir[0] * t, b.mount.z + dir[2] * t));
                }
                b.Finalise();
                scanners.push_back(b);
            }

        if (const JsonValue* pl = c.Section(*lights, "player_lamp", JsonValue::Object, "an object"))
        {
            float col[3] = { sc.flashColour.x, sc.flashColour.y, sc.flashColour.z }, att[3] = { sc.flashAttC, sc.flashAttL, sc.flashAttE };
            bool ok = c.Bool(*pl, "enabled", sc.flashOn, "lights.player_lamp") & c.Vec(*pl, "color", col, 3, false, "lights.player_lamp");
            ok &= c.Vec(*pl, "attenuation", att, 3, false, "lights.player_lamp");
            ok &= c.Num(*pl, "diffuse_intensity", sc.flashDiffuse, false, "lights.player_lamp", 0.0f, 20.0f) & c.Num(*pl, "edge", sc.flashEdgeDeg, false, "lights.player_lamp", 3.0f, 80.0f);
            if (ok) { sc.flashColour = glm::vec3(col[0], col[1], col[2]); sc.flashAttC = att[0]; sc.flashAttL = att[1]; sc.flashAttE = att[2]; }
        }

        sc.droneCount = (int)drones.size();
        sc.bulbs = drones;
        sc.bulbs.insert(sc.bulbs.end(), scanners.begin(), scanners.end());
    }

    void LoadObjects(Ctx& c, const JsonValue& root)
    {
        Scene& sc = c.sc;
        const JsonValue* arr = c.Section(root, "objects", JsonValue::Array, "an array");
        if (!arr) { c.Err(1, "objects", "section is missing: there is nothing to stand on"); return; }
        for (size_t i = 0; i < arr->arr.size(); ++i)
        {
            const JsonValue& o = arr->arr[i];
            std::string w = Ident(o, "objects", i);
            if (!o.IsObject()) { c.Err(o.line, w, "must be an object"); continue; }
            SceneObject ob; ob.line = o.line;
            std::string mat;
            float p[3], r[3] = { 0, 0, 0 }, s[3];
            bool ok = c.Str(o, "id", ob.id, true, w) & c.Str(o, "mesh_path", ob.mesh, true, w) & c.Str(o, "material_id", mat, true, w);
            ok &= c.Vec(o, "position", p, 3, true, w) & c.Vec(o, "scale", s, 3, true, w) & c.Vec(o, "rotation", r, 3, false, w);
            ok &= c.Bool(o, "solid", ob.solid, w) & c.Bool(o, "cast_shadow", ob.castShadow, w);
            if (!ok) continue;
            if (ob.mesh != "primitive:box" && ob.mesh != "primitive:cylinder" && ob.mesh != "primitive:sphere")
            {
                c.Err(o.line, w, "mesh_path '" + ob.mesh + "' is not available (model loading is switched off, USE_MODELS 0). Use primitive:box, primitive:cylinder or primitive:sphere"); continue;
            }
            if (s[0] <= 0 || s[1] <= 0 || s[2] <= 0) { c.Err(o.line, w, "'scale' values must all be > 0"); continue; }
            ob.material = sc.FindMaterial(mat);
            if (ob.material < 0) { c.Warn(o.line, w, "material_id '" + mat + "' does not exist - using material 0 instead"); ob.material = 0; }
            ob.pos = glm::vec3(p[0], p[1], p[2]); ob.scale = glm::vec3(s[0], s[1], s[2]); ob.rot = glm::vec3(r[0], r[1], r[2]);
            if ((ob.solid || ob.castShadow) && (r[0] != 0 || r[1] != 0 || r[2] != 0))
            {   // the collision test and the CPU line-of-sight test use axis-aligned boxes, so a rotated blocker would disagree with its own shadow
                c.Warn(o.line, w, "rotation is ignored for solid / shadow-casting objects (collision and the lit test use axis-aligned boxes)");
                ob.rot = glm::vec3(0.0f);
            }
            ob.minB = ob.pos - ob.scale * 0.5f; ob.maxB = ob.pos + ob.scale * 0.5f;
            sc.objects.push_back(ob);
        }
        if (sc.objects.empty()) c.Err(1, "objects", "no valid objects");
    }

    void LoadConveyors(Ctx& c, const JsonValue& root)
    {
        const JsonValue* arr = c.Section(root, "conveyors", JsonValue::Array, "an array");
        if (!arr) return;
        for (size_t i = 0; i < arr->arr.size(); ++i)
        {
            const JsonValue& o = arr->arr[i];
            std::string w = Ident(o, "conveyors", i);
            if (!o.IsObject()) { c.Err(o.line, w, "must be an object"); continue; }
            Conveyor cv; std::string mat = "mat_conveyor";
            float p[3], s[2], d[2], sp = 2.0f;
            bool ok = c.Str(o, "id", cv.id, true, w) & c.Vec(o, "position", p, 3, true, w) & c.Vec(o, "size", s, 2, true, w) & c.Vec(o, "direction", d, 2, true, w);
            ok &= c.Num(o, "speed", sp, true, w, -20.0f, 20.0f) & c.Str(o, "material_id", mat, false, w);
            if (!ok) continue;
            if (s[0] <= 0 || s[1] <= 0) { c.Err(o.line, w, "'size' [x, z] must be > 0"); continue; }
            glm::vec2 dir(d[0], d[1]);
            if (glm::length(dir) < 1e-4f) { c.Err(o.line, w, "'direction' [x, z] cannot be [0, 0]"); continue; }
            cv.centre = glm::vec2(p[0], p[2]); cv.size = glm::vec2(s[0], s[1]); cv.dir = glm::normalize(dir); cv.speed = sp;
            cv.material = c.sc.FindMaterial(mat);
            if (cv.material < 0) { c.Warn(o.line, w, "material_id '" + mat + "' does not exist - using material 0"); cv.material = 0; }
            c.sc.conveyors.push_back(cv);
        }
    }

    void LoadTerminals(Ctx& c, const JsonValue& root)
    {
        Scene& sc = c.sc;
        const JsonValue* arr = c.Section(root, "terminals", JsonValue::Array, "an array");
        if (!arr) return;
        for (size_t i = 0; i < arr->arr.size(); ++i)
        {
            const JsonValue& o = arr->arr[i];
            std::string w = Ident(o, "terminals", i);
            if (!o.IsObject()) { c.Err(o.line, w, "must be an object"); continue; }
            Terminal t; t.line = o.line; std::string mat = "mat_terminal";
            float p[3];
            bool ok = c.Str(o, "id", t.id, true, w) & c.Vec(o, "position", p, 3, true, w) & c.Str(o, "material_id", mat, false, w);
            const JsonValue* ln = o.Find("controls");
            if (!ln || !ln->IsArray() || ln->arr.empty()) { c.Err(o.line, w, "'controls' must be a non-empty array of scanner ids"); ok = false; }
            else for (const auto& n : ln->arr)
            {
                if (!n.IsString()) { c.Err(n.line, w, "'controls' entries must be strings"); ok = false; break; }
                t.linkNames.push_back(n.str);
            }
            if (!ok) continue;
            t.pos = glm::vec2(p[0], p[2]);
            for (const auto& nm : t.linkNames)
            {
                int found = -1;
                for (size_t k = 0; k < sc.bulbs.size(); ++k) if (sc.bulbs[k].scanner && sc.bulbs[k].name == nm) { found = (int)k; break; }
                if (found < 0) c.Err(o.line, w, "'controls' names '" + nm + "' but there is no scanner (spot light) with that id");
                else t.links.push_back(found);
            }
            if (t.links.empty()) { c.Warn(o.line, w, "terminal controls nothing and is skipped"); continue; }
            sc.terminals.push_back(t);

            // the cabinet itself: solid and shadow-casting like any other box
            SceneObject ob; ob.id = "terminal_" + t.id; ob.line = o.line;
            ob.material = sc.FindMaterial(mat);
            if (ob.material < 0) { c.Warn(o.line, w, "material_id '" + mat + "' does not exist - using material 0"); ob.material = 0; }
            ob.scale = glm::vec3(0.9f, 1.2f, 0.9f);
            ob.pos = glm::vec3(t.pos.x, 0.6f, t.pos.y);
            ob.minB = ob.pos - ob.scale * 0.5f; ob.maxB = ob.pos + ob.scale * 0.5f;
            sc.objects.push_back(ob);
        }
    }
}

bool SceneLoader::Load(const std::string& filepath, Scene& sc)
{
    sc.file = filepath;
    sc.playable = false;
    Ctx c{ sc, filepath };

    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open())
    {
        c.Err(0, "", "cannot open the level file (is the working directory the project folder?)");
        return false;
    }
    std::stringstream ss; ss << in.rdbuf();

    JsonValue root; std::string perr;
    if (!ParseJson(ss.str(), root, perr))
    {
        int ln = 0; std::sscanf(perr.c_str(), "line %d:", &ln);
        size_t colon = perr.find(": ");
        c.Err(ln, "", "not valid JSON: " + (colon != std::string::npos ? perr.substr(colon + 2) : perr));
        return false;
    }
    if (!root.IsObject()) { c.Err(1, "", "the file must contain one JSON object"); return false; }

    if (const JsonValue* w = c.Section(root, "window", JsonValue::Object, "an object"))
    {
        float ww = (float)sc.windowW, hh = (float)sc.windowH;
        if (c.Num(*w, "width", ww, false, "window", 320, 7680) & c.Num(*w, "height", hh, false, "window", 240, 4320)) { sc.windowW = (int)ww; sc.windowH = (int)hh; }
        c.Str(*w, "title", sc.title, false, "window");
    }
    if (const JsonValue* r = c.Section(root, "rendering", JsonValue::Object, "an object"))
    {
        float pr = (float)sc.omniRes, dr = (float)sc.dirRes;
        if (c.Num(*r, "point_shadow_resolution", pr, false, "rendering", 64, 4096) & c.Num(*r, "directional_shadow_resolution", dr, false, "rendering", 64, 8192)) { sc.omniRes = (int)pr; sc.dirRes = (int)dr; }
        c.Num(*r, "omni_bias", sc.omniBias, false, "rendering", 0.0f, 2.0f);
    }

    LoadMaterials(c, root);
    LoadLights(c, root);
    LoadObjects(c, root);
    LoadConveyors(c, root);
    LoadTerminals(c, root);

    if (const JsonValue* sky = c.Section(root, "skybox", JsonValue::Object, "an object"))
    {
        const JsonValue* f = sky->Find("faces");
        if (!f || !f->IsArray() || f->arr.size() != 6) c.Err(sky->line, "skybox", "'faces' must be an array of exactly 6 file paths (right, left, up, down, back, front)");
        else for (int i = 0; i < 6; ++i)
        {
            if (!f->arr[i].IsString()) { c.Err(f->arr[i].line, "skybox", "faces entries must be strings"); break; }
            sc.skyFaces[i] = f->arr[i].str;
            if (!FileExists(sc.skyFaces[i])) c.Warn(f->arr[i].line, "skybox", "face '" + sc.skyFaces[i] + "' not found - the sky will be left black");
        }
    }
    else c.Warn(1, "skybox", "no skybox section: the sky will be black");

    if (const JsonValue* g = c.Section(root, "game_logic", JsonValue::Object, "an object"))
    {
        float ps[3], ez[3], bd[4] = { sc.bounds.x, sc.bounds.y, sc.bounds.z, sc.bounds.w };
        if (c.Vec(*g, "player_start", ps, 3, true, "game_logic")) sc.playerStart = glm::vec2(ps[0], ps[2]);
        if (const JsonValue* e = c.Section(*g, "exit_zone", JsonValue::Object, "an object"))
        {
            if (c.Vec(*e, "position", ez, 3, true, "game_logic.exit_zone")) sc.exitPos = glm::vec2(ez[0], ez[2]);
            c.Num(*e, "radius", sc.exitRadius, true, "game_logic.exit_zone", 0.3f, 20.0f);

            // optional glowing pathway to a doorway in the wall
            if (const JsonValue* pw = c.Section(*e, "pathway", JsonValue::Object, "an object"))
            {
                const std::string w = "game_logic.exit_zone.pathway";
                ExitPath& ep = sc.exitPath;
                float from[2], to[2];
                std::string mat = "mat_exit", glow = "mat_exit_glow";
                bool ok = c.Vec(*pw, "from", from, 2, true, w) & c.Vec(*pw, "to", to, 2, true, w);
                ok &= c.Num(*pw, "width", ep.width, false, w, 0.8f, 10.0f) & c.Num(*pw, "door_height", ep.doorHeight, false, w, 1.0f, 3.0f);
                ok &= c.Str(*pw, "material_id", mat, false, w) & c.Str(*pw, "glow_material_id", glow, false, w);
                if (ok && glm::length(glm::vec2(to[0] - from[0], to[1] - from[1])) < 0.5f) { c.Err(pw->line, w, "'from' and 'to' must be at least 0.5 m apart"); ok = false; }
                if (ok)
                {
                    ep.from = glm::vec2(from[0], from[1]); ep.to = glm::vec2(to[0], to[1]);
                    ep.material = sc.FindMaterial(mat);
                    if (ep.material < 0) { c.Warn(pw->line, w, "material_id '" + mat + "' does not exist - using material 0"); ep.material = 0; }
                    ep.glowMaterial = sc.FindMaterial(glow);
                    if (ep.glowMaterial < 0) { c.Warn(pw->line, w, "glow_material_id '" + glow + "' does not exist - using material 0"); ep.glowMaterial = 0; }
                    ep.enabled = true;
                    if (glm::length(sc.exitPos - ep.to) > sc.exitRadius + 2.0f)
                        c.Warn(pw->line, w, "the pathway ends more than " + std::to_string(sc.exitRadius + 2.0f) + " m from the exit zone: the green path would not lead to the exit");
                }
            }
        }
        else c.Err(g->line, "game_logic", "missing 'exit_zone'");
        if (c.Vec(*g, "bounds", bd, 4, false, "game_logic")) sc.bounds = glm::vec4(bd[0], bd[1], bd[2], bd[3]);
    }
    else c.Err(1, "game_logic", "section is missing: no player start or exit");

    if (const JsonValue* g = c.Section(root, "gameplay", JsonValue::Object, "an object"))
    {
        c.Num(*g, "lit_threshold", sc.litThreshold, false, "gameplay", 0.001f, 10.0f);
        c.Num(*g, "alarm_fill", sc.alarmFill, false, "gameplay", 0.01f, 20.0f);
        c.Num(*g, "alarm_drain", sc.alarmDrain, false, "gameplay", 0.0f, 20.0f);
        c.Num(*g, "speed_step", sc.speedStep, false, "gameplay", 1.0f, 3.0f);
        if (const JsonValue* s = c.Section(*g, "strike", JsonValue::Object, "an object"))
        {
            c.Num(*s, "delay", sc.strikeDelay, false, "gameplay.strike", 0.3f, 30.0f);
            c.Num(*s, "radius", sc.strikeRadius, false, "gameplay.strike", 0.3f, 20.0f);
            c.Num(*s, "cooldown", sc.strikeCooldown, false, "gameplay.strike", 0.0f, 60.0f);
        }
        if (const JsonValue* h = c.Section(*g, "hack", JsonValue::Object, "an object"))
        {
            c.Num(*h, "time", sc.hackTime, false, "gameplay.hack", 0.1f, 30.0f);
            c.Num(*h, "radius", sc.hackRadius, false, "gameplay.hack", 0.3f, 20.0f);
            c.Num(*h, "cooldown", sc.hackCooldown, false, "gameplay.hack", 0.0f, 120.0f);
        }
    }

    // ---- is the level playable at all? ----
    bool playable = true;
    if (sc.bulbs.empty()) { c.Err(1, "lights", "no usable lights: the game would have nothing to hide from"); playable = false; }
    if (sc.objects.empty()) playable = false;
    if (PointInsideSolid(sc, sc.playerStart.x, sc.playerStart.y, PLAYER_RADIUS)) { c.Err(1, "game_logic", "player_start is inside a solid object"); playable = false; }
    if (PointInsideSolid(sc, sc.exitPos.x, sc.exitPos.y, 0.0f)) c.Warn(1, "game_logic", "the exit zone centre is inside a solid object");
    int moving = 0; for (const auto& b : sc.bulbs) if (b.scanner || !b.IsStatic()) ++moving;
    if (moving < 2) c.Warn(1, "lights", "the spec asks for at least two moving lights (patrolling drones or sweeping scanners), this level has " + std::to_string(moving));
    int casters = 0; for (const auto& o : sc.objects) if (o.castShadow && o.solid) ++casters;
    if (casters < 4) c.Warn(1, "objects", "the spec asks for at least four hiding-shadow casters, this level has " + std::to_string(casters));

    sc.playable = playable && c.errors >= 0;
    std::cout << "[SCENE] " << filepath << ": " << sc.bulbs.size() << " lights (" << sc.droneCount << " drones, " << (sc.bulbs.size() - sc.droneCount)
        << " scanners), " << sc.objects.size() << " objects, " << sc.conveyors.size() << " conveyors, " << sc.terminals.size()
        << " terminals, " << c.errors << " error(s)\n";
    return sc.playable;
}
