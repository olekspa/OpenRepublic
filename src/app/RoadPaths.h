#pragma once

#include "openrepublic/content/road_network.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace openrepublic::preview
{
	struct RoadSample
	{
		double x = 0.0;
		double z = 0.0;
		double dx = 0.0; // unit direction along the path
		double dz = 1.0;
		double distance = 0.0; // arc length from the path start
	};

	// Centre-line of the road between two junction nodes: the decoded segment polyline when the
	// package's segment geometry covers it, otherwise the straight chord between the junctions.
	class RoadPathIndex
	{
	public:
		explicit RoadPathIndex(const openrepublic::content::RoadNetwork& network) : network_(network)
		{
			for (std::size_t s = 0; s < network.segments.size(); ++s)
			{
				const auto& segment = network.segments[s];
				if (segment.polyline.size() >= 2)
				{
					byNodes_[Key(segment.nodeA, segment.nodeB)] = s;
				}
			}
		}

		// Points from node `a` to node `b` as (x, z) pairs.
		std::vector<std::pair<double, double>> Path(std::uint32_t a, std::uint32_t b) const
		{
			std::vector<std::pair<double, double>> points;
			const auto found = byNodes_.find(Key(a, b));
			if (found != byNodes_.end())
			{
				const auto& segment = network_.segments[found->second];
				for (const auto& p : segment.polyline)
				{
					points.emplace_back(p.x, p.z);
				}
				// The polyline is stored in the segment's node order; orient it from a to b.
				const auto& na = network_.nodes[a];
				const auto& nb = network_.nodes[b];
				const double forward = std::hypot(points.front().first - na.x, points.front().second - na.z) +
					std::hypot(points.back().first - nb.x, points.back().second - nb.z);
				const double reverse = std::hypot(points.front().first - nb.x, points.front().second - nb.z) +
					std::hypot(points.back().first - na.x, points.back().second - na.z);
				if (reverse < forward)
				{
					std::reverse(points.begin(), points.end());
				}
				// Close the small gap between the junction and the first/last decoded point.
				points.insert(points.begin(), { na.x, na.z });
				points.emplace_back(nb.x, nb.z);
				return points;
			}
			points.emplace_back(network_.nodes[a].x, network_.nodes[a].z);
			points.emplace_back(network_.nodes[b].x, network_.nodes[b].z);
			return points;
		}

		bool HasCurve(std::uint32_t a, std::uint32_t b) const
		{
			return byNodes_.find(Key(a, b)) != byNodes_.end();
		}

		// Resamples a path every ~`step` metres with smoothed directions.
		static std::vector<RoadSample> Resample(const std::vector<std::pair<double, double>>& points, double step)
		{
			std::vector<RoadSample> samples;
			if (points.size() < 2)
			{
				return samples;
			}
			std::vector<double> cumulative(points.size(), 0.0);
			for (std::size_t i = 1; i < points.size(); ++i)
			{
				cumulative[i] = cumulative[i - 1] + std::hypot(points[i].first - points[i - 1].first, points[i].second - points[i - 1].second);
			}
			const double total = cumulative.back();
			if (total < 0.01)
			{
				return samples;
			}
			const int count = std::max(1, static_cast<int>(std::ceil(total / step)));
			std::size_t segment = 0;
			const auto at = [&](double d, double& x, double& z)
			{
				while (segment + 2 < points.size() && cumulative[segment + 1] < d)
				{
					++segment;
				}
				const double span = std::max(1e-6, cumulative[segment + 1] - cumulative[segment]);
				const double t = std::clamp((d - cumulative[segment]) / span, 0.0, 1.0);
				x = points[segment].first + (points[segment + 1].first - points[segment].first) * t;
				z = points[segment].second + (points[segment + 1].second - points[segment].second) * t;
			};
			for (int s = 0; s <= count; ++s)
			{
				const double d = total * s / count;
				RoadSample sample;
				sample.distance = d;
				segment = 0;
				at(d, sample.x, sample.z);
				double ax, az, bx, bz;
				segment = 0;
				at(std::max(0.0, d - step), ax, az);
				segment = 0;
				at(std::min(total, d + step), bx, bz);
				const double length = std::hypot(bx - ax, bz - az);
				if (length > 1e-6)
				{
					sample.dx = (bx - ax) / length;
					sample.dz = (bz - az) / length;
				}
				samples.push_back(sample);
			}
			return samples;
		}

	private:
		static std::uint64_t Key(std::uint32_t a, std::uint32_t b)
		{
			if (a > b)
			{
				std::swap(a, b);
			}
			return (static_cast<std::uint64_t>(a) << 32) | b;
		}

		const openrepublic::content::RoadNetwork& network_;
		std::unordered_map<std::uint64_t, std::size_t> byNodes_;
	};
}
