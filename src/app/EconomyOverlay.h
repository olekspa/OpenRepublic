#pragma once

#include "WickedEngine.h"
#include "openrepublic/content/map_placement.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace openrepublic::preview
{
	// One placed building instance in the region view (map coordinates in metres).
	struct RegionInstance
	{
		wi::ecs::Entity object = wi::ecs::INVALID_ENTITY;
		std::string baseType;
		double x = 0.0;
		double z = 0.0;
	};

	// Shows a running economy world (OpenRepublicEconomyHost behind tools/economy_http_bridge.py)
	// on the region view: buildings are tinted by their stall state and trucks appear as markers.
	// A background thread polls `hello` once and `world` every second over loopback HTTP; the main
	// thread applies the newest snapshot. The overlay only reads: it never changes the world.
	class EconomyOverlay
	{
	public:
		EconomyOverlay(std::string host, int port);
		~EconomyOverlay();
		EconomyOverlay(const EconomyOverlay&) = delete;
		EconomyOverlay& operator=(const EconomyOverlay&) = delete;

		// Applies the latest snapshot to the scene and returns a one-line status for the UI.
		std::string Update(wi::scene::Scene& scene, const std::vector<RegionInstance>& instances,
			const openrepublic::content::MapPlacementRequest& request);

	private:
		void PollLoop();
		bool Post(const std::string& body, std::string& response, std::string& error) const;

		std::string host_;
		int port_;
		std::atomic<bool> stop_{ false };
		std::thread thread_;
		std::mutex mutex_;
		std::string helloJson_;
		std::string worldJson_;
		std::string pollError_;
		std::uint64_t appliedSequence_ = 0;
		std::uint64_t sequence_ = 0;
		std::vector<wi::ecs::Entity> truckEntities_;
	};
}
