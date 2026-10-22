#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace openrepublic::procgeo
{
	// Engine-independent triangle mesh used by OpenRepublic's procedural presentation assets
	// (vehicles, props, vegetation). Units are metres, +Y up, +Z forward, +X right, the origin at
	// ground level under the object's centre. Triangles are counter-clockwise when seen from
	// outside (the front face), like Wicked Engine's convention in the app. Colours are linear
	// RGB 0..1 per vertex. These meshes are OpenRepublic-authored, not WRSR content.
	struct Vec3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct Color3
	{
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
	};

	struct Mesh
	{
		std::vector<Vec3> positions;
		std::vector<Color3> colors;          // same length as positions
		std::vector<std::uint32_t> indices;  // multiple of 3

		// Appends another mesh translated by `offset` (indices are rebased).
		void Append(const Mesh& other, Vec3 offset = {});
		// Axis-aligned bounds; returns false for an empty mesh.
		bool Bounds(Vec3& minimum, Vec3& maximum) const;
		// True when sizes agree and every index is in range.
		bool Valid() const;
	};

	// Adds a box with the given centre, half extents and colour (24 vertices, 12 triangles).
	void AddBox(Mesh& mesh, Vec3 center, Vec3 halfExtents, Color3 color);
	// Adds a vertical cylinder (closed) from y0 to y1 around (cx, cz).
	void AddCylinder(Mesh& mesh, float cx, float cz, float y0, float y1, float radius, int segments, Color3 color);
}
