#pragma once

#include "RoadPaths.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace openrepublic::preview
{
	struct RoadMeshVertex
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		float u = 0.0f;
		float v = 0.0f;
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;
	};

	struct RoadMeshBuffer
	{
		std::vector<RoadMeshVertex> vertices;
		std::vector<std::uint32_t> indices;
	};

	struct RoadMeshBuffers
	{
		RoadMeshBuffer pavement;
		RoadMeshBuffer junctions;
		RoadMeshBuffer shoulders;
		RoadMeshBuffer sidewalks;
		std::size_t edgeCount = 0;
		std::size_t curvedEdgeCount = 0;
		std::size_t junctionCount = 0;
	};

	struct RoadMeshOptions
	{
		double halfWidth = 2.5;
		double shoulderWidth = 1.25;
		double sidewalkWidth = 1.5;
		double sidewalkRise = 0.12;
		double surfaceLift = 0.06;
		double textureRepeatLength = 16.0;
		double maximumSampleSpacing = 8.0;
		double minimumSampleSpacing = 0.5;
		double maximumMiterScale = 2.0;
		double maximumDistance = 0.0; // 0 disables region filtering
		double centerX = 0.0;
		double centerZ = 0.0;
		bool buildShoulders = true;
		bool buildSidewalks = false;
		std::size_t maximumVertices = 240000;
	};

	using RoadTerrainHeight = std::function<double(double, double)>;

	// Builds OpenRepublic preview geometry from the safely decoded graph and the
	// bounded path adapter. Road width and sidewalk use are presentation options;
	// they do not assert recovered WRSR width, lane, or town-road semantics.
	bool BuildRoadMesh(const openrepublic::content::RoadNetwork& network,
		const RoadPathIndex& paths, const RoadTerrainHeight& terrainHeight,
		const RoadMeshOptions& options, RoadMeshBuffers& output,
		std::size_t* skippedEdges = nullptr);
}
