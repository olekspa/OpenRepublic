#pragma once

#include "WickedEngine.h"
#include "openrepublic/content/map_placement.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
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
		// Index in MapPlacementRequest::allPlacements; exact for this loaded package.
		std::uint64_t sourcePlacementIndex{};
	};

	struct EconomyInventoryStatus
	{
		std::string resource;
		std::uint64_t amount{};
		std::uint64_t capacity{};
	};

	struct EconomyBuildingStatus
	{
		std::uint64_t id{};
		std::string type;
		bool enabled{};
		std::string stallReason;
		std::string stallExplanation;
		std::vector<EconomyInventoryStatus> inventory;
	};

	// Connects the region view to an OpenRepublicEconomyHost behind the trusted-local loopback
	// bridge. A background thread polls state and serializes bounded commands; the main thread
	// maps snapshots onto rendered instances and can submit human actions through the same API
	// used by external controllers.
	class EconomyOverlay
	{
	public:
		EconomyOverlay(std::string host, int port);
		~EconomyOverlay();
		EconomyOverlay(const EconomyOverlay&) = delete;
		EconomyOverlay& operator=(const EconomyOverlay&) = delete;

		// Applies the latest snapshot to the scene and returns a one-line status for the UI.
		std::string Update(wi::scene::Scene& scene, const std::vector<RegionInstance>& instances,
			const openrepublic::content::MapPlacementRequest& request, double expectedRadiusMeters);
		std::optional<EconomyBuildingStatus> GetBuildingForInstance(wi::ecs::Entity instance) const;
		bool QueueToggleBuilding(std::uint64_t buildingId);
		bool QueueStep(std::uint32_t ticks);
		bool QueueHaulOrder(std::uint64_t sourceId, std::uint64_t destinationId);
		std::string GetActionStatus() const;

	private:
		void PollLoop();
		bool Post(const std::string& body, std::string& response, std::string& error) const;
		bool Enqueue(std::string request, std::string acceptedMessage, bool expectsCommandHandle = false);
		void SetActionStatus(std::string status);
		void ReleaseCommandHandleSlot();
		struct PendingRequest
		{
			std::string body;
			std::string message;
			bool expectsCommandHandle{};
		};

		std::string host_;
		int port_;
		std::atomic<bool> stop_{ false };
		std::thread thread_;
		mutable std::mutex mutex_;
		std::string helloJson_;
		std::string catalogJson_;
		std::string worldJson_;
		std::string pollError_;
		std::string actionStatus_;
		std::deque<PendingRequest> pendingRequests_;
		std::size_t trackedCommandCount_ = 0;
		std::uint64_t appliedSequence_ = 0;
		std::uint64_t sequence_ = 0;
		bool forceApplySnapshot_ = true;
		std::uint64_t worldRevision_ = 0;
		bool haveWorldRevision_ = false;
		bool regionIdentityVerified_ = false;
		bool regionRadiusReduced_ = false;
		bool controllerRole_ = false;
		std::vector<EconomyBuildingStatus> buildings_;
		std::vector<std::pair<wi::ecs::Entity, std::uint64_t>> instanceBuildingIds_;
		std::vector<wi::ecs::Entity> truckEntities_;
	};
}
