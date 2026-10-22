#include "openrepublic/procgeo/mesh.h"

#include <algorithm>
#include <cmath>

namespace openrepublic::procgeo
{
	void Mesh::Append(const Mesh& other, Vec3 offset)
	{
		const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
		for (const Vec3& p : other.positions)
		{
			positions.push_back({ p.x + offset.x, p.y + offset.y, p.z + offset.z });
		}
		colors.insert(colors.end(), other.colors.begin(), other.colors.end());
		for (const std::uint32_t i : other.indices)
		{
			indices.push_back(base + i);
		}
	}

	bool Mesh::Bounds(Vec3& minimum, Vec3& maximum) const
	{
		if (positions.empty())
		{
			return false;
		}
		minimum = maximum = positions.front();
		for (const Vec3& p : positions)
		{
			minimum = { std::min(minimum.x, p.x), std::min(minimum.y, p.y), std::min(minimum.z, p.z) };
			maximum = { std::max(maximum.x, p.x), std::max(maximum.y, p.y), std::max(maximum.z, p.z) };
		}
		return true;
	}

	bool Mesh::Valid() const
	{
		if (colors.size() != positions.size() || indices.size() % 3 != 0)
		{
			return false;
		}
		return std::all_of(indices.begin(), indices.end(),
			[this](std::uint32_t i) { return i < positions.size(); });
	}

	void AddBox(Mesh& mesh, Vec3 c, Vec3 h, Color3 color)
	{
		struct Face { Vec3 n; Vec3 u; Vec3 v; };
		// Each face: outward normal n, with u x v = n so the winding is counter-clockwise from outside.
		const Face faces[6] = {
			{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } },
			{ { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
			{ { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } },
			{ { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
			{ { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },
			{ { 0, 0, -1 }, { 0, 1, 0 }, { 1, 0, 0 } },
		};
		for (const Face& f : faces)
		{
			const std::uint32_t base = static_cast<std::uint32_t>(mesh.positions.size());
			const float su[2] = { -1.0f, 1.0f };
			for (int j = 0; j < 2; ++j)
			{
				for (int i = 0; i < 2; ++i)
				{
					mesh.positions.push_back({
						c.x + h.x * (f.n.x + su[i] * f.u.x + su[j] * f.v.x),
						c.y + h.y * (f.n.y + su[i] * f.u.y + su[j] * f.v.y),
						c.z + h.z * (f.n.z + su[i] * f.u.z + su[j] * f.v.z) });
					mesh.colors.push_back(color);
				}
			}
			mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 3, base, base + 3, base + 2 });
		}
	}

	void AddCylinder(Mesh& mesh, float cx, float cz, float y0, float y1, float radius, int segments, Color3 color)
	{
		segments = std::max(3, segments);
		const std::uint32_t base = static_cast<std::uint32_t>(mesh.positions.size());
		for (int i = 0; i < segments; ++i)
		{
			const float a = 6.28318530718f * static_cast<float>(i) / segments;
			mesh.positions.push_back({ cx + std::cos(a) * radius, y0, cz + std::sin(a) * radius });
			mesh.positions.push_back({ cx + std::cos(a) * radius, y1, cz + std::sin(a) * radius });
			mesh.colors.push_back(color);
			mesh.colors.push_back(color);
		}
		for (int i = 0; i < segments; ++i)
		{
			const std::uint32_t a0 = base + 2 * i;
			const std::uint32_t a1 = base + 2 * ((i + 1) % segments);
			// (a0 bottom, a0+1 top); sin increases clockwise seen from +Y, so wind for outward faces.
			mesh.indices.insert(mesh.indices.end(), { a0, a0 + 1, a1, a1, a0 + 1, a1 + 1 });
		}
		const std::uint32_t topCenter = static_cast<std::uint32_t>(mesh.positions.size());
		mesh.positions.push_back({ cx, y1, cz });
		mesh.colors.push_back(color);
		const std::uint32_t bottomCenter = static_cast<std::uint32_t>(mesh.positions.size());
		mesh.positions.push_back({ cx, y0, cz });
		mesh.colors.push_back(color);
		for (int i = 0; i < segments; ++i)
		{
			const std::uint32_t a = base + 2 * i;
			const std::uint32_t b = base + 2 * ((i + 1) % segments);
			mesh.indices.insert(mesh.indices.end(), { a + 1, topCenter, b + 1 });
			mesh.indices.insert(mesh.indices.end(), { a, b, bottomCenter });
		}
	}
}
