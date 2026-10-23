#include "RoadMesh.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
	int failures = 0;
	void Check(bool value, const std::string& label)
	{
		if (!value) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
	}
	bool Valid(const openrepublic::preview::RoadMeshBuffer& mesh)
	{
		for (const auto& v : mesh.vertices)
			if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
				!std::isfinite(v.u) || !std::isfinite(v.v)) return false;
		for (auto i : mesh.indices) if (i >= mesh.vertices.size()) return false;
		return mesh.indices.size() % 3 == 0;
	}
}

int main()
{
	using namespace openrepublic::content;
	using namespace openrepublic::preview;
	RoadNetwork graph;
	graph.nodes.resize(4);
	graph.nodes[0] = { 0, 0, 0, { 1, 2, 3 }, {}, {} };
	graph.nodes[1] = { -18, 0, 0, { 0 }, {}, {} };
	graph.nodes[2] = { 18, 0, 0, { 0 }, {}, {} };
	graph.nodes[3] = { 0, 0, 20, { 0 }, {}, {} };
	RoadSegment curve;
	curve.nodeA = 0; curve.nodeB = 1;
	curve.polyline = { { 0, 0, 0 }, { -5, 0, 1 }, { -12, 0, 1 }, { -18, 0, 0 } };
	graph.segments.push_back(curve);
	const RoadPathIndex paths(graph);
	RoadMeshOptions options;
	options.maximumSampleSpacing = 5.0;
	options.minimumSampleSpacing = 0.4;
	options.buildSidewalks = true;
	RoadMeshBuffers mesh;
	std::size_t skipped = 0;
	const auto h = [](double x, double z) { return 0.02 * x + 0.01 * z; };
	Check(BuildRoadMesh(graph, paths, h, options, mesh, &skipped), "builder accepts finite synthetic T graph");
	Check(mesh.edgeCount == 3 && mesh.curvedEdgeCount == 1, "all graph edges emitted and decoded curve counted");
	Check(mesh.junctionCount == 1 && !mesh.junctions.indices.empty(), "three-link node receives one junction polygon");
	Check(skipped == 0, "valid edges are not skipped");
	Check(Valid(mesh.pavement) && Valid(mesh.junctions) && Valid(mesh.shoulders) && Valid(mesh.sidewalks),
		"all output buffers have finite vertices and in-range triangle indices");
	Check(mesh.shoulders.vertices.size() > 0 && mesh.sidewalks.vertices.size() > 0,
		"optional shoulders and sidewalks are generated");
	bool continuousUv = true;
	float previous = -1.0f;
	double previousX = 0.0, previousZ = 0.0;
	for (std::size_t i = 0; i + 1 < mesh.pavement.vertices.size(); i += 2)
	{
		const auto& left = mesh.pavement.vertices[i];
		const auto& right = mesh.pavement.vertices[i + 1];
		if (std::fabs(left.v - right.v) > 1e-6f || left.u != 0.0f || right.u != 1.0f) continuousUv = false;
		const double x = (left.x + right.x) * 0.5;
		const double z = (left.z + right.z) * 0.5;
		const double edgeJump = i == 0 ? 0.0 : std::hypot(x - previousX, z - previousZ);
		if (left.v + 1e-6f < previous && edgeJump < 10.0) continuousUv = false;
		previous = left.v;
		previousX = x; previousZ = z;
	}
	Check(continuousUv, "pavement UVs progress continuously and reset only at edge boundaries");
	RoadMeshBuffers preserved;
	preserved.edgeCount = 99;
	options.halfWidth = std::numeric_limits<double>::quiet_NaN();
	Check(!BuildRoadMesh(graph, paths, h, options, preserved), "invalid options fail closed");
	Check(preserved.edgeCount == 99, "failed build leaves caller output unchanged");
	return failures == 0 ? 0 : 1;
}
