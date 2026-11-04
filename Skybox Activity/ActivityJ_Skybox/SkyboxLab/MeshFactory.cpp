#include "MeshFactory.h"
#include "CommonValues.h"

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

static void PushVertex(std::vector<float>& v, const glm::vec3& p, const glm::vec2& uv, const glm::vec3& n)
{
	v.push_back(p.x);  v.push_back(p.y);  v.push_back(p.z);
	v.push_back(uv.x); v.push_back(uv.y);
	v.push_back(n.x);  v.push_back(n.y);  v.push_back(n.z);
}

static Mesh* Build(std::vector<float>& verts, std::vector<unsigned int>& idx)
{
	Mesh* m = new Mesh();
	m->CreateMesh(verts.data(), idx.data(), (unsigned int)verts.size(), (unsigned int)idx.size());
	return m;
}

Mesh* MakeBoxMesh(float sx, float sy, float sz, float tileSize)
{
	const glm::vec3 h(sx * 0.5f, sy * 0.5f, sz * 0.5f);
	struct Face { glm::vec3 n, u, v; };
	// u x v == n for every face, so (-u-v, +u-v, +u+v, -u+v) is counter-clockwise seen from outside
	const Face faces[6] = {
		{ { 1, 0, 0}, { 0, 0,-1}, { 0, 1, 0} },
		{ {-1, 0, 0}, { 0, 0, 1}, { 0, 1, 0} },
		{ { 0, 1, 0}, { 1, 0, 0}, { 0, 0,-1} },
		{ { 0,-1, 0}, { 1, 0, 0}, { 0, 0, 1} },
		{ { 0, 0, 1}, { 1, 0, 0}, { 0, 1, 0} },
		{ { 0, 0,-1}, {-1, 0, 0}, { 0, 1, 0} },
	};
	std::vector<float> verts;
	std::vector<unsigned int> idx;
	for (int f = 0; f < 6; ++f)
	{
		const Face& F = faces[f];
		float hn = std::fabs(F.n.x) * h.x + std::fabs(F.n.y) * h.y + std::fabs(F.n.z) * h.z;
		float hu = std::fabs(F.u.x) * h.x + std::fabs(F.u.y) * h.y + std::fabs(F.u.z) * h.z;
		float hv = std::fabs(F.v.x) * h.x + std::fabs(F.v.y) * h.y + std::fabs(F.v.z) * h.z;
		float su = 2.0f * hu / tileSize, sv = 2.0f * hv / tileSize;      // texture repeats every tileSize world units
		glm::vec3 c = F.n * hn;
		unsigned int base = (unsigned int)(verts.size() / 8);
		PushVertex(verts, c - F.u * hu - F.v * hv, { 0,  0 }, F.n);
		PushVertex(verts, c + F.u * hu - F.v * hv, { su, 0 }, F.n);
		PushVertex(verts, c + F.u * hu + F.v * hv, { su, sv }, F.n);
		PushVertex(verts, c - F.u * hu + F.v * hv, { 0,  sv }, F.n);
		idx.insert(idx.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
	}
	return Build(verts, idx);
}

Mesh* MakeSphereMesh(float radius, int slices, int stacks)
{
	std::vector<float> verts;
	std::vector<unsigned int> idx;
	for (int i = 0; i <= stacks; ++i)
	{
		float phi = PI_F * (float)i / (float)stacks;
		for (int j = 0; j <= slices; ++j)
		{
			float theta = 2.0f * PI_F * (float)j / (float)slices;
			glm::vec3 n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
			PushVertex(verts, n * radius, { (float)j / slices, (float)i / stacks }, n);
		}
	}
	for (int i = 0; i < stacks; ++i)
		for (int j = 0; j < slices; ++j)
		{
			unsigned int a = i * (slices + 1) + j;
			unsigned int b = a + slices + 1;
			idx.insert(idx.end(), { a, b, a + 1, a + 1, b, b + 1 });
		}
	return Build(verts, idx);
}

Mesh* MakeCylinderMesh(float radius, float height, int slices)
{
	std::vector<float> verts;
	std::vector<unsigned int> idx;

	// side (smooth normals)
	for (int j = 0; j <= slices; ++j)
	{
		float t = 2.0f * PI_F * (float)j / (float)slices;
		glm::vec3 n(std::cos(t), 0.0f, std::sin(t));
		PushVertex(verts, { radius * n.x, 0.0f,   radius * n.z }, { (float)j / slices, 0.0f }, n);
		PushVertex(verts, { radius * n.x, height, radius * n.z }, { (float)j / slices, 1.0f }, n);
	}
	for (int j = 0; j < slices; ++j)
	{
		unsigned int a = 2 * j;
		idx.insert(idx.end(), { a, a + 2, a + 1, a + 1, a + 2, a + 3 });
	}
	// caps (flat normals)
	for (int cap = 0; cap < 2; ++cap)
	{
		float y = cap ? height : 0.0f;
		glm::vec3 n(0.0f, cap ? 1.0f : -1.0f, 0.0f);
		unsigned int centre = (unsigned int)(verts.size() / 8);
		PushVertex(verts, { 0.0f, y, 0.0f }, { 0.5f, 0.5f }, n);
		for (int j = 0; j <= slices; ++j)
		{
			float t = 2.0f * PI_F * (float)j / (float)slices;
			PushVertex(verts, { radius * std::cos(t), y, radius * std::sin(t) }, { 0.5f + 0.5f * std::cos(t), 0.5f + 0.5f * std::sin(t) }, n);
		}
		for (int j = 0; j < slices; ++j)
		{
			if (cap) idx.insert(idx.end(), { centre, centre + 2 + (unsigned)j, centre + 1 + (unsigned)j });
			else     idx.insert(idx.end(), { centre, centre + 1 + (unsigned)j, centre + 2 + (unsigned)j });
		}
	}
	return Build(verts, idx);
}

Mesh* MakeConeMesh(float baseRadius, float height, int slices)
{
	// apex at the origin, circular base at y = -height. Used for the volumetric scanner beam
	// (drawn without depth writes, so the winding does not matter).
	std::vector<float> verts;
	std::vector<unsigned int> idx;
	for (int j = 0; j <= slices; ++j)
	{
		float t = 2.0f * PI_F * (float)j / (float)slices;
		glm::vec3 n = glm::normalize(glm::vec3(std::cos(t), 0.25f, std::sin(t)));
		PushVertex(verts, { 0, 0, 0 }, { (float)j / slices, 0 }, n);
		PushVertex(verts, { baseRadius * std::cos(t), -height, baseRadius * std::sin(t) }, { (float)j / slices, 1 }, n);
	}
	for (int j = 0; j < slices; ++j)
	{
		unsigned int a = 2 * j;
		idx.insert(idx.end(), { a, a + 1, a + 3 });
	}
	return Build(verts, idx);
}
