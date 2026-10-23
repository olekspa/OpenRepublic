#include "RoadMesh.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace openrepublic::preview
{
	namespace
	{
		struct Point
		{
			double x = 0.0;
			double z = 0.0;
		double distance = 0.0;
		};

		struct EdgeEnd
		{
			std::uint32_t node = 0;
			Point left;
			Point right;
		};

		double Distance(const Point& a, const Point& b)
		{
			return std::hypot(b.x - a.x, b.z - a.z);
		}

		Point Interpolate(const Point& a, const Point& b, double t)
		{
			return { a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t,
				a.distance + (b.distance - a.distance) * t };
		}

		std::vector<Point> AdaptivePath(const std::vector<std::pair<double, double>>& raw,
			double maxSpacing, double minSpacing)
		{
			std::vector<Point> result;
			if (raw.size() < 2) return result;
			std::vector<double> cumulative(raw.size(), 0.0);
			for (std::size_t i = 1; i < raw.size(); ++i)
			{
				const double length = std::hypot(raw[i].first - raw[i - 1].first, raw[i].second - raw[i - 1].second);
				if (!std::isfinite(length)) return {};
				cumulative[i] = cumulative[i - 1] + length;
			}
			if (cumulative.back() < 0.01) return {};
			result.push_back({ raw.front().first, raw.front().second, 0.0 });
			for (std::size_t i = 1; i < raw.size(); ++i)
			{
				const double segmentLength = cumulative[i] - cumulative[i - 1];
				if (segmentLength < 1e-6) continue;
				double turn = 0.0;
				if (i + 1 < raw.size() && i > 0)
				{
					const double ax = raw[i].first - raw[i - 1].first;
					const double az = raw[i].second - raw[i - 1].second;
					const double bx = raw[i + 1].first - raw[i].first;
					const double bz = raw[i + 1].second - raw[i].second;
					const double denom = std::hypot(ax, az) * std::hypot(bx, bz);
					if (denom > 1e-9)
						turn = std::acos(std::clamp((ax * bx + az * bz) / denom, -1.0, 1.0));
				}
				const double spacing = std::clamp(maxSpacing / (1.0 + turn * 2.0), minSpacing, maxSpacing);
				const std::size_t divisions = std::max<std::size_t>(1,
					static_cast<std::size_t>(std::ceil(segmentLength / spacing)));
				for (std::size_t j = 1; j <= divisions; ++j)
				{
					const double t = static_cast<double>(j) / static_cast<double>(divisions);
					result.push_back({ raw[i - 1].first + (raw[i].first - raw[i - 1].first) * t,
						raw[i - 1].second + (raw[i].second - raw[i - 1].second) * t,
						cumulative[i - 1] + segmentLength * t });
				}
			}
			return result;
		}

		Point AtDistance(const std::vector<Point>& path, double distance)
		{
			if (distance <= path.front().distance) return path.front();
			if (distance >= path.back().distance) return path.back();
			auto it = std::upper_bound(path.begin(), path.end(), distance,
				[](double d, const Point& p) { return d < p.distance; });
			const auto& b = *it;
			const auto& a = *(it - 1);
			return Interpolate(a, b, (distance - a.distance) / std::max(1e-9, b.distance - a.distance));
		}

		std::vector<Point> TrimPath(const std::vector<Point>& path, double trimStart, double trimEnd)
		{
			if (path.size() < 2 || path.back().distance <= trimStart + trimEnd + 0.05) return {};
			std::vector<Point> result;
			result.push_back(AtDistance(path, trimStart));
			for (const auto& p : path)
				if (p.distance > trimStart + 1e-5 && p.distance < path.back().distance - trimEnd - 1e-5)
					result.push_back(p);
			result.push_back(AtDistance(path, path.back().distance - trimEnd));
			return result;
		}

		std::vector<Point> ConvexHull(std::vector<Point> points)
		{
			std::sort(points.begin(), points.end(), [](const Point& a, const Point& b) {
				return a.x == b.x ? a.z < b.z : a.x < b.x;
			});
			points.erase(std::unique(points.begin(), points.end(), [](const Point& a, const Point& b) {
				return std::hypot(a.x - b.x, a.z - b.z) < 1e-5;
			}), points.end());
			if (points.size() < 3) return {};
			const auto cross = [](const Point& o, const Point& a, const Point& b) {
				return (a.x - o.x) * (b.z - o.z) - (a.z - o.z) * (b.x - o.x);
			};
			std::vector<Point> hull;
			for (const auto& p : points)
			{
				while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull.back(), p) <= 1e-8) hull.pop_back();
				hull.push_back(p);
			}
			const std::size_t lowerSize = hull.size();
			for (auto it = points.rbegin() + 1; it != points.rend(); ++it)
			{
				while (hull.size() > lowerSize && cross(hull[hull.size() - 2], hull.back(), *it) <= 1e-8) hull.pop_back();
				hull.push_back(*it);
			}
			if (!hull.empty()) hull.pop_back();
			return hull;
		}

		RoadMeshVertex Vertex(double x, double y, double z, float u, float v,
			float r = 1.0f, float g = 1.0f, float b = 1.0f)
		{
			return { x, y, z, u, v, r, g, b, 1.0f };
		}

		bool Valid(const RoadMeshBuffer& buffer)
		{
			for (const auto& v : buffer.vertices)
				if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
					!std::isfinite(v.u) || !std::isfinite(v.v) || !std::isfinite(v.r) ||
					!std::isfinite(v.g) || !std::isfinite(v.b) || !std::isfinite(v.a)) return false;
			for (auto i : buffer.indices) if (i >= buffer.vertices.size()) return false;
			return buffer.indices.size() % 3 == 0;
		}
	}

	bool BuildRoadMesh(const openrepublic::content::RoadNetwork& network,
		const RoadPathIndex& paths, const RoadTerrainHeight& terrainHeight,
		const RoadMeshOptions& options, RoadMeshBuffers& output, std::size_t* skippedEdges)
	{
		if (!std::isfinite(options.halfWidth) || options.halfWidth <= 0.1 ||
			!std::isfinite(options.shoulderWidth) || options.shoulderWidth < 0.0 ||
			!std::isfinite(options.sidewalkWidth) || options.sidewalkWidth < 0.0 ||
			!std::isfinite(options.sidewalkRise) || options.sidewalkRise < 0.0 ||
			!std::isfinite(options.surfaceLift) || options.surfaceLift < 0.0 ||
			!std::isfinite(options.textureRepeatLength) || options.textureRepeatLength <= 0.0 ||
			!std::isfinite(options.maximumSampleSpacing) || options.maximumSampleSpacing <= 0.0 ||
			!std::isfinite(options.minimumSampleSpacing) || options.minimumSampleSpacing <= 0.0 ||
			options.minimumSampleSpacing > options.maximumSampleSpacing ||
			!std::isfinite(options.maximumMiterScale) || options.maximumMiterScale < 1.0 ||
			!std::isfinite(options.maximumDistance) || options.maximumDistance < 0.0 ||
			!std::isfinite(options.centerX) || !std::isfinite(options.centerZ) || options.maximumVertices < 4)
			return false;

		RoadMeshBuffers built;
		std::vector<std::vector<Point>> junctionPorts(network.nodes.size());
		std::size_t skipped = 0;
		const auto height = [&](double x, double z) {
			if (!terrainHeight) return 0.0;
			const double y = terrainHeight(x, z);
			return std::isfinite(y) ? y : 0.0;
		};
		const auto appendStrip = [&](RoadMeshBuffer& buffer, const std::vector<Point>& path,
			double sideA, double sideB, double yOffsetA, double yOffsetB, double repeat, bool useMiter,
			float shadeA, float shadeB, std::uint32_t& baseOut) -> bool
		{
			if (path.size() < 2 || buffer.vertices.size() + path.size() * 2 > options.maximumVertices) return false;
			baseOut = static_cast<std::uint32_t>(buffer.vertices.size());
			for (std::size_t i = 0; i < path.size(); ++i)
			{
				const auto& p = path[i];
				const auto& before = path[i == 0 ? i : i - 1];
				const auto& after = path[i + 1 < path.size() ? i + 1 : i];
				double dx = after.x - before.x;
				double dz = after.z - before.z;
				const double length = std::hypot(dx, dz);
				if (length < 1e-8) return false;
				dx /= length; dz /= length;
				double nx = dz, nz = -dx;
				double scale = 1.0;
				if (useMiter && i > 0 && i + 1 < path.size())
				{
					const double len0 = std::hypot(p.x - before.x, p.z - before.z);
					const double len1 = std::hypot(after.x - p.x, after.z - p.z);
					if (len0 > 1e-8 && len1 > 1e-8)
					{
						const double n0x = (p.z - before.z) / len0, n0z = -(p.x - before.x) / len0;
						const double n1x = (after.z - p.z) / len1, n1z = -(after.x - p.x) / len1;
						nx = n0x + n1x; nz = n0z + n1z;
						const double nl = std::hypot(nx, nz);
						if (nl > 1e-8)
						{
							nx /= nl; nz /= nl;
							const double denom = std::max(0.25, std::fabs(nx * n1x + nz * n1z));
							scale = std::min(options.maximumMiterScale, 1.0 / denom);
						}
					}
				}
				const double yA = std::max(height(p.x + nx * sideA * scale, p.z + nz * sideA * scale), height(p.x, p.z)) + yOffsetA;
				const double yB = std::max(height(p.x + nx * sideB * scale, p.z + nz * sideB * scale), height(p.x, p.z)) + yOffsetB;
				buffer.vertices.push_back(Vertex(p.x + nx * sideA * scale, yA, p.z + nz * sideA * scale,
					0.0f, static_cast<float>(p.distance / repeat), shadeA, shadeA, shadeA));
				buffer.vertices.push_back(Vertex(p.x + nx * sideB * scale, yB, p.z + nz * sideB * scale,
					1.0f, static_cast<float>(p.distance / repeat), shadeB, shadeB, shadeB));
			}
			for (std::size_t i = 0; i + 1 < path.size(); ++i)
			{
				const std::uint32_t a = baseOut + static_cast<std::uint32_t>(i * 2);
				buffer.indices.insert(buffer.indices.end(), { a, a + 1, a + 2, a + 1, a + 3, a + 2 });
			}
			return true;
		};

		for (std::size_t i = 0; i < network.nodes.size(); ++i)
		{
			const auto& node = network.nodes[i];
			for (const std::uint32_t j : node.neighbors)
			{
				if (j <= i) continue;
				if (j >= network.nodes.size()) { ++skipped; continue; }
				const auto& other = network.nodes[j];
				if (options.maximumDistance > 0.0 &&
					std::min(std::hypot(node.x - options.centerX, node.z - options.centerZ),
						std::hypot(other.x - options.centerX, other.z - options.centerZ)) > options.maximumDistance)
				{ ++skipped; continue; }
				auto path = AdaptivePath(paths.Path(static_cast<std::uint32_t>(i), j),
					options.maximumSampleSpacing, options.minimumSampleSpacing);
				if (path.size() < 2) { ++skipped; continue; }
				const bool startJunction = node.neighbors.size() >= 3;
				const bool endJunction = other.neighbors.size() >= 3;
				path = TrimPath(path, startJunction ? options.halfWidth * 1.15 : 0.0,
					endJunction ? options.halfWidth * 1.15 : 0.0);
				if (path.size() < 2) { ++skipped; continue; }
				std::uint32_t pavementBase = 0;
				const std::size_t pavementCount = built.pavement.vertices.size();
				if (!appendStrip(built.pavement, path, -options.halfWidth, options.halfWidth,
					options.surfaceLift, options.surfaceLift, options.textureRepeatLength, true, 1.0f, 1.0f, pavementBase))
				{ built.pavement.vertices.resize(pavementCount); ++skipped; continue; }
				if (startJunction)
				{
					const auto& a = built.pavement.vertices[pavementBase];
					const auto& b = built.pavement.vertices[pavementBase + 1];
					junctionPorts[i].push_back({ a.x, a.z, a.y }); junctionPorts[i].push_back({ b.x, b.z, b.y });
				}
				if (endJunction)
				{
					const auto& a = built.pavement.vertices[pavementBase + (path.size() - 1) * 2];
					const auto& b = built.pavement.vertices[pavementBase + (path.size() - 1) * 2 + 1];
					junctionPorts[j].push_back({ a.x, a.z, a.y }); junctionPorts[j].push_back({ b.x, b.z, b.y });
				}
				if (options.buildShoulders && options.shoulderWidth > 0.0)
				{
					std::uint32_t ignored = 0;
					const auto count = built.shoulders.vertices.size();
					const auto indexCount = built.shoulders.indices.size();
					if (!appendStrip(built.shoulders, path, -options.halfWidth - options.shoulderWidth, -options.halfWidth,
						0.0, options.surfaceLift, options.textureRepeatLength, true, 0.82f, 0.52f, ignored) ||
						!appendStrip(built.shoulders, path, options.halfWidth, options.halfWidth + options.shoulderWidth,
						options.surfaceLift, 0.0, options.textureRepeatLength, true, 0.52f, 0.82f, ignored))
					{ built.shoulders.vertices.resize(count); built.shoulders.indices.resize(indexCount); }
				}
				if (options.buildSidewalks && options.sidewalkWidth > 0.0)
				{
					std::uint32_t ignored = 0;
					const auto count = built.sidewalks.vertices.size();
					const auto indexCount = built.sidewalks.indices.size();
					const double sidewalkInner = options.halfWidth + (options.buildShoulders ? options.shoulderWidth : 0.0);
					if (!appendStrip(built.sidewalks, path, -sidewalkInner - options.sidewalkWidth, -sidewalkInner,
						options.surfaceLift + options.sidewalkRise, options.surfaceLift + options.sidewalkRise, options.textureRepeatLength, true, 1.0f, 1.0f, ignored) ||
						!appendStrip(built.sidewalks, path, sidewalkInner, sidewalkInner + options.sidewalkWidth,
						options.surfaceLift + options.sidewalkRise, options.surfaceLift + options.sidewalkRise, options.textureRepeatLength, true, 1.0f, 1.0f, ignored))
					{ built.sidewalks.vertices.resize(count); built.sidewalks.indices.resize(indexCount); }
				}
				++built.edgeCount;
				if (paths.HasCurve(static_cast<std::uint32_t>(i), j)) ++built.curvedEdgeCount;
			}
		}

		for (std::size_t n = 0; n < junctionPorts.size(); ++n)
		{
			if (network.nodes[n].neighbors.size() < 3 || junctionPorts[n].size() < 6) continue;
			const auto hull = ConvexHull(std::move(junctionPorts[n]));
			if (hull.size() < 3 || built.junctions.vertices.size() + hull.size() > options.maximumVertices) continue;
			const auto base = static_cast<std::uint32_t>(built.junctions.vertices.size());
			for (const auto& p : hull)
				built.junctions.vertices.push_back(Vertex(p.x, p.distance + 0.015,
					p.z, static_cast<float>((p.x - network.nodes[n].x) / (options.halfWidth * 2.0) + 0.5),
					static_cast<float>((p.z - network.nodes[n].z) / (options.halfWidth * 2.0) + 0.5)));
			for (std::size_t k = 1; k + 1 < hull.size(); ++k)
				built.junctions.indices.insert(built.junctions.indices.end(), { base, base + static_cast<std::uint32_t>(k), base + static_cast<std::uint32_t>(k + 1) });
			++built.junctionCount;
		}
		if (!Valid(built.pavement) || !Valid(built.junctions) || !Valid(built.shoulders) || !Valid(built.sidewalks)) return false;
		output = std::move(built);
		if (skippedEdges) *skippedEdges = skipped;
		return true;
	}
}
