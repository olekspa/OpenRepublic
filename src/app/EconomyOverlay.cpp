#include "EconomyOverlay.h"

#include "json.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <tuple>

namespace openrepublic::preview
{
	using Json = nlohmann::json;
	constexpr std::size_t kMaxPendingRequests = 32;
	constexpr std::uint32_t kMaxQueuedStepTicks = 100000;
	constexpr double kMaxBuildingMatchDistanceMeters = 12.0;

	bool ParseDecimalU64(const Json& value, std::uint64_t& out)
	{
		if (!value.is_string()) return false;
		const std::string text = value.get<std::string>();
		if (text.empty()) return false;
		std::uint64_t parsed = 0;
		for (const char ch : text)
		{
			if (ch < '0' || ch > '9') return false;
			const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
			if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return false;
			parsed = parsed * 10 + digit;
		}
		out = parsed;
		return true;
	}

	std::string ProtocolFailure(const Json& response)
	{
		if (response.contains("error") && response["error"].is_object())
		{
			const Json& error = response["error"];
			const std::string code = error.value("code", "request_failed");
			const std::string message = error.value("message", "economy host rejected the request");
			return code + ": " + message;
		}
		if (response.contains("error") && response["error"].is_string())
		{
			return "economy bridge error: " + response["error"].get<std::string>();
		}
		return "economy host returned an invalid response";
	}

	bool HasMatchingSession(const Json& response, const std::string& sessionId)
	{
		return response.is_object() && response.contains("sessionId") &&
			response["sessionId"].is_string() && response["sessionId"].get<std::string>() == sessionId;
	}

	bool HasDifferentSession(const Json& response, const std::string& sessionId)
	{
		return response.is_object() && response.contains("sessionId") &&
			(!response["sessionId"].is_string() || response["sessionId"].get<std::string>() != sessionId);
	}

	EconomyOverlay::EconomyOverlay(std::string host, int port) : host_(std::move(host)), port_(port)
	{
		thread_ = std::thread([this]() { PollLoop(); });
	}

	EconomyOverlay::~EconomyOverlay()
	{
		stop_ = true;
		if (thread_.joinable())
		{
			thread_.join();
		}
	}

	bool EconomyOverlay::Post(const std::string& body, std::string& response, std::string& error) const
	{
		bool ok = false;
		const std::wstring wideHost(host_.begin(), host_.end());
		HINTERNET session = WinHttpOpen(L"OpenRepublic economy overlay", WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		HINTERNET connection = session ? WinHttpConnect(session, wideHost.c_str(), static_cast<INTERNET_PORT>(port_), 0) : nullptr;
		HINTERNET request = connection ? WinHttpOpenRequest(connection, L"POST", L"/rpc", nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0) : nullptr;
		if (request != nullptr)
		{
			WinHttpSetTimeouts(request, 1500, 1500, 3000, 5000);
			const wchar_t* headers = L"Content-Type: application/json";
			if (WinHttpSendRequest(request, headers, static_cast<DWORD>(-1), const_cast<char*>(body.data()),
				static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
				WinHttpReceiveResponse(request, nullptr))
			{
				response.clear();
				DWORD available = 0;
				while (WinHttpQueryDataAvailable(request, &available) && available > 0)
				{
					std::string chunk(available, '\0');
					DWORD read = 0;
					if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0)
					{
						break;
					}
					response.append(chunk.data(), read);
					if (response.size() > 8u * 1024u * 1024u)
					{
						break;
					}
				}
				ok = !response.empty();
			}
		}
		if (!ok)
		{
			error = "economy bridge not reachable at " + host_ + ":" + std::to_string(port_);
		}
		if (request) WinHttpCloseHandle(request);
		if (connection) WinHttpCloseHandle(connection);
		if (session) WinHttpCloseHandle(session);
		return ok;
	}

	void EconomyOverlay::PollLoop()
	{
		bool haveHello = false;
		bool haveCatalog = false;
		std::string currentSessionId;
		std::vector<std::pair<std::string, std::string>> pendingCommandHandles;
		const auto invalidateSession = [&](const std::string& reason)
		{
			haveHello = false;
			haveCatalog = false;
			pendingCommandHandles.clear();
			std::lock_guard<std::mutex> lock(mutex_);
			helloJson_.clear();
			catalogJson_.clear();
			worldJson_.clear();
			controllerRole_ = false;
			regionIdentityVerified_ = false;
			pendingRequests_.clear();
			trackedCommandCount_ = 0;
			pollError_ = reason;
			actionStatus_ = "Economy bridge connection lost; pending command outcomes are unknown.";
			++sequence_;
		};
		const auto waitForNextPoll = [&]()
		{
			for (int i = 0; i < 10 && !stop_; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		};
		while (!stop_)
		{
			std::string response;
			std::string error;
			if (!haveHello && Post("{\"method\":\"hello\"}", response, error))
			{
				const Json hello = Json::parse(response, nullptr, false);
				if (!hello.is_discarded() && hello.value("ok", false) &&
					hello.contains("result") && hello["result"].is_object() &&
					hello["result"].contains("sessionId") && hello["result"]["sessionId"].is_string() &&
					!hello["result"]["sessionId"].get<std::string>().empty())
				{
					const std::string newSessionId = hello["result"]["sessionId"].get<std::string>();
					if (!currentSessionId.empty() && newSessionId != currentSessionId)
					{
						invalidateSession("economy host session changed; cleared stale snapshot and command state");
					}
					currentSessionId = newSessionId;
					std::lock_guard<std::mutex> lock(mutex_);
					helloJson_ = response;
					controllerRole_ = hello["result"].value("role", "") == "controller";
					pollError_.clear();
					error.clear();
					haveHello = true;
				}
				else
				{
					error = ProtocolFailure(hello);
					invalidateSession(error);
				}
			}
			else if (!haveHello)
			{
				invalidateSession(error.empty() ? "economy bridge hello request failed" : error);
			}
			if (haveHello)
			{
				if (!haveCatalog && Post(Json{{"method", "catalog"}, {"sessionId", currentSessionId}}.dump(), response, error))
				{
					const Json catalog = Json::parse(response, nullptr, false);
					if (!catalog.is_discarded() && catalog.value("ok", false) &&
						catalog.contains("result") && catalog["result"].is_object() &&
						HasMatchingSession(catalog, currentSessionId))
					{
						std::lock_guard<std::mutex> lock(mutex_);
						catalogJson_ = response;
						error.clear();
						haveCatalog = true;
					}
					else
					{
						error = HasDifferentSession(catalog, currentSessionId)
							? "economy host session changed while loading catalog"
							: ProtocolFailure(catalog);
						invalidateSession(error);
					}
				}
				else if (!haveCatalog && !error.empty())
				{
					invalidateSession(error);
				}
				if (!haveHello)
				{
					waitForNextPoll();
					continue;
				}

				PendingRequest pending;
				bool havePending = false;
				{
					std::lock_guard<std::mutex> lock(mutex_);
					if (!pendingRequests_.empty())
					{
						pending = std::move(pendingRequests_.front());
						pendingRequests_.pop_front();
						havePending = true;
					}
				}
				if (havePending)
				{
					std::string actionError;
					Json actionRequest = Json::parse(pending.body, nullptr, false);
					if (actionRequest.is_discarded() || !actionRequest.is_object())
					{
						SetActionStatus("Could not encode the queued economy action.");
						if (pending.expectsCommandHandle) ReleaseCommandHandleSlot();
					}
					else
					{
						actionRequest["sessionId"] = currentSessionId;
						if (!Post(actionRequest.dump(), response, actionError))
						{
							invalidateSession(actionError.empty()
								? "economy bridge failed during a command; command outcome is unknown"
								: actionError + "; command outcome is unknown");
						}
						else
						{
							const Json actionResponse = Json::parse(response, nullptr, false);
							if (!HasMatchingSession(actionResponse, currentSessionId))
							{
								invalidateSession(HasDifferentSession(actionResponse, currentSessionId)
									? "economy host session changed while submitting a command"
									: "economy host returned an invalid command response; command outcome is unknown");
							}
							else if (actionResponse.is_discarded() || !actionResponse.value("ok", false))
							{
								SetActionStatus(ProtocolFailure(actionResponse));
								if (pending.expectsCommandHandle) ReleaseCommandHandleSlot();
							}
							else
							{
								const Json result = actionResponse.value("result", Json::object());
								if (!result.is_object())
								{
									SetActionStatus("economy host returned an invalid command result");
									if (pending.expectsCommandHandle) ReleaseCommandHandleSlot();
								}
								else if (result.value("errorCode", std::string()) != "none")
								{
									SetActionStatus("economy host rejected the command: " + result.value("errorCode", "unknown_error"));
									if (pending.expectsCommandHandle) ReleaseCommandHandleSlot();
								}
								else if (result.contains("handle"))
								{
									const std::string handle = result["handle"].get<std::string>();
									pendingCommandHandles.emplace_back(handle, pending.message);
									SetActionStatus(pending.message + " (command " + handle + ")");
								}
								else
								{
									SetActionStatus(pending.message);
									if (pending.expectsCommandHandle) ReleaseCommandHandleSlot();
								}
							}
						}
					}
				}
				if (!haveHello)
				{
					waitForNextPoll();
					continue;
				}

				if (Post(Json{{"method", "world"}, {"sessionId", currentSessionId}}.dump(), response, error))
				{
					const Json world = Json::parse(response, nullptr, false);
					if (!world.is_discarded() && world.value("ok", false) &&
						world.contains("result") && world["result"].is_object() &&
						HasMatchingSession(world, currentSessionId))
					{
						std::lock_guard<std::mutex> lock(mutex_);
						worldJson_ = response;
						pollError_.clear();
						error.clear();
						++sequence_;
					}
					else
					{
						error = HasDifferentSession(world, currentSessionId)
							? "economy host session changed while reading world state"
							: ProtocolFailure(world);
						invalidateSession(error);
					}
				}
				else
				{
					invalidateSession(error.empty() ? "economy bridge world request failed" : error);
				}
				if (!pendingCommandHandles.empty())
				{
					std::size_t handlesPolled = 0;
					for (auto handle = pendingCommandHandles.begin();
						handle != pendingCommandHandles.end() && handlesPolled == 0;)
					{
						std::string pollResponse;
						std::string pollError;
						const std::string pollRequest = Json{{"method", "poll"}, {"handle", handle->first},
							{"sessionId", currentSessionId}}.dump();
						if (!Post(pollRequest, pollResponse, pollError))
						{
							SetActionStatus(pollError.empty() ? "Could not read the economy command result." : pollError);
							++handlesPolled;
							std::rotate(pendingCommandHandles.begin(), pendingCommandHandles.begin() + 1,
								pendingCommandHandles.end());
							break;
						}
						++handlesPolled;
						const Json polled = Json::parse(pollResponse, nullptr, false);
						if (!polled.is_discarded() && !HasMatchingSession(polled, currentSessionId))
						{
							invalidateSession("economy host session changed while reading a command result");
							break;
						}
						if (polled.is_discarded() || !polled.value("ok", false) ||
							!polled.contains("result") || !polled["result"].is_object())
						{
							SetActionStatus(ProtocolFailure(polled));
							std::rotate(pendingCommandHandles.begin(), pendingCommandHandles.begin() + 1,
								pendingCommandHandles.end());
							break;
						}
						const Json& result = polled["result"];
						if (result.value("errorCode", std::string()) != "none")
						{
							SetActionStatus("Could not read command " + handle->first + ": " +
								result.value("errorCode", "unknown_error"));
							ReleaseCommandHandleSlot();
							handle = pendingCommandHandles.erase(handle);
							continue;
						}
						const std::string state = result.value("state", "unknown");
						if (state == "queued")
						{
							std::rotate(pendingCommandHandles.begin(), pendingCommandHandles.begin() + 1,
								pendingCommandHandles.end());
							break;
						}
						if (state == "accepted")
						{
							SetActionStatus(handle->second + " was applied (command " + handle->first + ").");
						}
						else if (state == "rejected")
						{
							SetActionStatus(handle->second + " was rejected (" +
								result.value("failure", "unknown_failure") + "): " +
								result.value("message", "no details") + ".");
						}
						else
						{
							std::rotate(pendingCommandHandles.begin(), pendingCommandHandles.begin() + 1,
								pendingCommandHandles.end());
							break;
						}
						ReleaseCommandHandleSlot();
						handle = pendingCommandHandles.erase(handle);
					}
				}
			}
			if (!error.empty())
			{
				std::lock_guard<std::mutex> lock(mutex_);
				pollError_ = error;
			}
			waitForNextPoll();
		}
	}

	bool EconomyOverlay::Enqueue(std::string request, std::string acceptedMessage, bool expectsCommandHandle)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!regionIdentityVerified_)
		{
			actionStatus_ = "The host center/radius has not been verified against this map preview.";
			return false;
		}
		if (!controllerRole_)
		{
			actionStatus_ = "This economy connection is observer-only; start the host with --role controller.";
			return false;
		}
		if (pendingRequests_.size() >= kMaxPendingRequests)
		{
			actionStatus_ = "Economy action queue is full; wait for pending actions to send.";
			return false;
		}
		if (expectsCommandHandle && trackedCommandCount_ >= kMaxPendingRequests)
		{
			actionStatus_ = "Command result tracking is full; step the world to apply pending actions.";
			return false;
		}
		pendingRequests_.push_back({std::move(request), std::move(acceptedMessage), expectsCommandHandle});
		if (expectsCommandHandle) ++trackedCommandCount_;
		actionStatus_ = pendingRequests_.back().message;
		return true;
	}

	void EconomyOverlay::ReleaseCommandHandleSlot()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (trackedCommandCount_ > 0) --trackedCommandCount_;
	}

	void EconomyOverlay::SetActionStatus(std::string status)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		actionStatus_ = std::move(status);
	}

	std::string EconomyOverlay::GetActionStatus() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return actionStatus_;
	}

	std::optional<EconomyBuildingStatus> EconomyOverlay::GetBuildingForInstance(wi::ecs::Entity instance) const
	{
		const auto binding = std::find_if(instanceBuildingIds_.begin(), instanceBuildingIds_.end(),
			[instance](const auto& item) { return item.first == instance; });
		if (binding == instanceBuildingIds_.end()) return std::nullopt;
		const auto building = std::find_if(buildings_.begin(), buildings_.end(), [&](const EconomyBuildingStatus& item) {
			return item.id == binding->second;
		});
		return building == buildings_.end() ? std::nullopt : std::optional<EconomyBuildingStatus>(*building);
	}

	bool EconomyOverlay::QueueToggleBuilding(std::uint64_t buildingId)
	{
		const auto building = std::find_if(buildings_.begin(), buildings_.end(), [&](const EconomyBuildingStatus& item) {
			return item.id == buildingId;
		});
		if (building == buildings_.end() || !haveWorldRevision_)
		{
			SetActionStatus("No current economy snapshot contains the selected building.");
			return false;
		}
		Json request = {
			{"method", "submit"},
			{"revision", std::to_string(worldRevision_)},
			{"command", {{"kind", "set_building_enabled"}, {"buildingId", buildingId}, {"enabled", !building->enabled}}}
		};
		return Enqueue(request.dump(), std::string("Queued ") + (building->enabled ? "disable" : "enable") +
			" for building " + std::to_string(buildingId) + "; advance the world to apply it.", true);
	}

	bool EconomyOverlay::QueueStep(std::uint32_t ticks)
	{
		if (ticks == 0 || ticks > kMaxQueuedStepTicks)
		{
			SetActionStatus("Step size must be between 1 and 100000 ticks.");
			return false;
		}
		return Enqueue(Json{{"method", "step"}, {"ticks", ticks}}.dump(),
			"Advanced the connected economy by " + std::to_string(ticks) + " ticks.");
	}

	bool EconomyOverlay::QueueHaulOrder(std::uint64_t sourceId, std::uint64_t destinationId)
	{
		const auto source = std::find_if(buildings_.begin(), buildings_.end(), [&](const EconomyBuildingStatus& item) {
			return item.id == sourceId;
		});
		const auto destination = std::find_if(buildings_.begin(), buildings_.end(), [&](const EconomyBuildingStatus& item) {
			return item.id == destinationId;
		});
		if (!haveWorldRevision_ || source == buildings_.end() || destination == buildings_.end())
		{
			SetActionStatus("Choose two buildings present in the latest economy snapshot.");
			return false;
		}

		std::string catalog;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			catalog = catalogJson_;
		}
		const Json catalogDoc = Json::parse(catalog, nullptr, false);
		if (catalogDoc.is_discarded() || !catalogDoc.value("ok", false) ||
			!catalogDoc.contains("result") || !catalogDoc["result"].is_object())
		{
			SetActionStatus("The economy catalog is not available yet.");
			return false;
		}
		const Json& catalogResult = catalogDoc["result"];
		std::string resource;
		const Json& buildingTypes = catalogResult.value("buildingTypes", Json::array());
		const Json* sourceType = nullptr;
		const Json* destinationType = nullptr;
		if (buildingTypes.is_array())
		{
			for (const Json& type : buildingTypes)
			{
				if (!type.is_object()) continue;
				if (type.value("name", "") == source->type) sourceType = &type;
				if (type.value("name", "") == destination->type) destinationType = &type;
			}
		}
		if (sourceType != nullptr && destinationType != nullptr)
		{
			const Json& sourceRecipes = sourceType->value("recipes", Json::array());
			const Json& destinationRecipes = destinationType->value("recipes", Json::array());
			if (sourceRecipes.is_array() && destinationRecipes.is_array())
			{
				for (const Json& sourceRecipe : sourceRecipes)
				{
					const Json outputs = sourceRecipe.value("outputs", Json::array());
					if (!outputs.is_array()) continue;
					for (const Json& output : outputs)
					{
						if (!output.is_object()) continue;
						const std::string candidate = output.value("resource", "");
						if (candidate.empty()) continue;
						bool consumed = false;
						for (const Json& destinationRecipe : destinationRecipes)
						{
							const Json inputs = destinationRecipe.value("inputs", Json::array());
							if (!inputs.is_array()) continue;
							for (const Json& input : inputs)
							{
								if (input.is_object() && input.value("resource", "") == candidate)
								{
									consumed = true;
									break;
								}
							}
							if (consumed) break;
						}
						if (consumed)
						{
							resource = candidate;
							break;
						}
					}
					if (!resource.empty()) break;
				}
			}
		}
		if (resource.empty())
		{
			SetActionStatus("The selected building types have no compatible catalog output/input resource.");
			return false;
		}

		const Json& vehicles = catalogResult.value("vehicleTypes", Json::array());
		if (!vehicles.is_array() || vehicles.empty() || !vehicles.front().is_object())
		{
			SetActionStatus("The economy catalog has no vehicle type for a haul order.");
			return false;
		}
		const std::string vehicleType = vehicles.front().value("name", "");
		if (vehicleType.empty())
		{
			SetActionStatus("The first economy vehicle type has no usable name.");
			return false;
		}
		Json request = {
			{"method", "submit"},
			{"revision", std::to_string(worldRevision_)},
			{"command", {{"kind", "create_haul_order"}, {"source", sourceId},
				{"destination", destinationId}, {"resource", resource}, {"vehicleType", vehicleType}}}
		};
		return Enqueue(request.dump(), "Queued " + resource + " haul from building " +
			std::to_string(sourceId) + " to " + std::to_string(destinationId) +
			" using " + vehicleType + "; advance the world to apply it.", true);
	}

	std::string EconomyOverlay::Update(wi::scene::Scene& scene, const std::vector<RegionInstance>& instances,
		const openrepublic::content::MapPlacementRequest& request, double expectedRadiusMeters)
	{
		std::string hello;
		std::string world;
		std::string error;
		std::uint64_t sequence = 0;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			hello = helloJson_;
			world = worldJson_;
			error = pollError_;
			sequence = sequence_;
		}
		const auto clearPresentation = [&]()
		{
			{
				std::lock_guard<std::mutex> lock(mutex_);
				regionIdentityVerified_ = false;
			}
			buildings_.clear();
			instanceBuildingIds_.clear();
			haveWorldRevision_ = false;
			regionRadiusReduced_ = false;
			forceApplySnapshot_ = true;
			for (const RegionInstance& instance : instances)
			{
				if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(instance.object))
				{
					object->color = XMFLOAT4(1, 1, 1, 1);
				}
			}
			for (const wi::ecs::Entity truck : truckEntities_)
			{
				if (wi::scene::TransformComponent* transform = scene.transforms.GetComponent(truck))
				{
					transform->ClearTransform();
					transform->Translate(XMFLOAT3(0, -10000, 0));
					transform->UpdateTransform();
				}
			}
		};
		if (!error.empty())
		{
			clearPresentation();
			return "Economy overlay: " + error;
		}
		if (world.empty())
		{
			clearPresentation();
			return "Economy overlay: waiting for the bridge...";
		}
		const Json helloDoc = Json::parse(hello, nullptr, false);
		const Json worldDoc = Json::parse(world, nullptr, false);
		if (helloDoc.is_discarded() || !helloDoc.value("ok", false) ||
			!helloDoc.contains("result") || !helloDoc["result"].is_object() ||
			worldDoc.is_discarded() || !worldDoc.value("ok", false))
		{
			clearPresentation();
			return "Economy overlay: unreadable response";
		}
		const Json info = helloDoc["result"].value("regionInfo", Json());
		if (!info.is_object())
		{
			clearPresentation();
			return "Economy overlay: the world has no regionInfo (start the host with --region)";
		}
		const std::uint64_t hostCenter = info.value("centerPlacement", std::numeric_limits<std::uint64_t>::max());
		const double hostRadius = info.value("radiusMeters", -1.0);
		const double radiusTolerance = std::max(1e-6, std::abs(expectedRadiusMeters) * 1e-9);
		const bool regionMatches = std::isfinite(expectedRadiusMeters) && expectedRadiusMeters > 0.0 &&
			std::isfinite(hostRadius) && hostCenter == static_cast<std::uint64_t>(request.placementIndex) &&
			std::abs(hostRadius - expectedRadiusMeters) <= radiusTolerance;
		// Revalidate every UI update: the selected preview can change while the most
		// recent host snapshot has not advanced yet.
		{
			std::lock_guard<std::mutex> lock(mutex_);
			regionIdentityVerified_ = regionMatches;
		}
		if (!regionMatches)
		{
			clearPresentation();
			std::ostringstream mismatch;
			mismatch << "Economy bridge center/radius mismatch: host " << hostCenter << "/" << hostRadius
				<< " differs from preview " << request.placementIndex << "/" << expectedRadiusMeters
				<< ". Restart the host with matching --center and --radius.";
			if (helloDoc["result"].value("region", std::string()).find("Selection radius reduced from") != std::string::npos)
			{
				mismatch << " The host also reduced its import radius to fit the grid.";
			}
			return mismatch.str();
		}
		if (sequence == appliedSequence_ && !forceApplySnapshot_)
		{
			return std::string();
		}
		appliedSequence_ = sequence;
		forceApplySnapshot_ = false;
		haveWorldRevision_ = false;
		buildings_.clear();
		instanceBuildingIds_.clear();
		const std::string regionDescription = helloDoc["result"].value("region", "");
		regionRadiusReduced_ = regionDescription.find("Selection radius reduced from") != std::string::npos;
		const double originX = info.value("originX", 0.0);
		const double originZ = info.value("originZ", 0.0);
		const double cell = info.value("cellSizeMeters", 4.0);
		const Json& state = worldDoc["result"]["world"];
		std::uint64_t revision = 0;
		if (!state.contains("revision") || !ParseDecimalU64(state["revision"], revision))
		{
			clearPresentation();
			return "Economy overlay: world snapshot has an invalid revision";
		}
		worldRevision_ = revision;
		haveWorldRevision_ = true;
		const auto& center = request.placement;
		const auto transform = openrepublic::content::MakeMapTerrainTransform(request.metadata);

		// Match imported economy buildings to rendered map instances by type and
		// map-space cell centre. This is an approximation because the import does not
		// preserve source placement IDs. Bind only mutually unique nearest pairs; leave
		// ambiguous candidates unbound so UI actions cannot guess between buildings.
		struct MatchCandidate
		{
			double distance{};
			std::size_t buildingIndex{};
			std::size_t instanceIndex{};
			std::uint64_t buildingId{};
		};
		std::vector<MatchCandidate> candidates;
		std::vector<EconomyBuildingStatus> latestBuildings;
		std::size_t running = 0;
		std::size_t total = 0;
		const Json economyBuildings = state.value("buildings", Json::array());
		if (economyBuildings.is_array())
		{
			latestBuildings.reserve(economyBuildings.size());
			for (const Json& building : economyBuildings)
			{
				if (!building.is_object()) continue;
				EconomyBuildingStatus status;
				status.id = building.value("id", std::uint64_t{});
				status.type = building.value("type", "");
				status.enabled = building.value("enabled", false);
				status.stallReason = building.value("stallReason", "");
				status.stallExplanation = building.value("stallExplanation", "");
				const Json inventory = building.value("inventory", Json::array());
				if (inventory.is_array())
				{
					for (const Json& item : inventory)
					{
						if (!item.is_object()) continue;
						EconomyInventoryStatus value;
						value.resource = item.value("resource", "");
						ParseDecimalU64(item.value("amount", Json()), value.amount);
						ParseDecimalU64(item.value("capacity", Json()), value.capacity);
						status.inventory.push_back(std::move(value));
					}
				}
				latestBuildings.push_back(status);
				++total;
				if (status.stallReason == "running") ++running;
				// Malformed snapshots with repeated API IDs remain visible in the
				// copied status list, but cannot create ambiguous entity bindings.
				const auto duplicateId = std::find_if(latestBuildings.begin(), latestBuildings.end() - 1,
					[&](const EconomyBuildingStatus& existing) { return existing.id == status.id; });
				if (duplicateId != latestBuildings.end() - 1) continue;

				const Json& cellPosition = building.value("cell", Json::object());
				if (!cellPosition.is_object() || status.type.empty()) continue;
				const double wx = originX + (cellPosition.value("x", 0) + 0.5) * cell;
				const double wz = originZ + (cellPosition.value("z", 0) + 0.5) * cell;
				for (std::size_t i = 0; i < instances.size(); ++i)
				{
					if (instances[i].baseType != status.type) continue;
					const double distance = std::hypot(instances[i].x - wx, instances[i].z - wz);
					if (distance <= kMaxBuildingMatchDistanceMeters)
					{
						candidates.push_back({distance, latestBuildings.size() - 1, i, status.id});
					}
				}
			}
		}
		for (const RegionInstance& instance : instances)
		{
			if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(instance.object))
			{
				object->color = XMFLOAT4(1, 1, 1, 1);
			}
		}
		std::vector<std::pair<wi::ecs::Entity, std::uint64_t>> bindings;
		constexpr double kMatchTieToleranceMeters = 1e-6;
		std::vector<std::size_t> bestForBuilding(latestBuildings.size(), candidates.size());
		std::vector<std::size_t> bestForInstance(instances.size(), candidates.size());
		std::vector<std::uint8_t> tiedBuilding(latestBuildings.size(), 0);
		std::vector<std::uint8_t> tiedInstance(instances.size(), 0);
		const auto updateBest = [&](std::size_t candidateIndex, std::size_t& bestIndex, std::uint8_t& tied)
		{
			if (bestIndex == candidates.size() ||
				candidates[candidateIndex].distance + kMatchTieToleranceMeters < candidates[bestIndex].distance)
			{
				bestIndex = candidateIndex;
				tied = 0;
			}
			else if (std::abs(candidates[candidateIndex].distance - candidates[bestIndex].distance) <=
				kMatchTieToleranceMeters)
			{
				tied = 1;
			}
		};
		for (std::size_t i = 0; i < candidates.size(); ++i)
		{
			const MatchCandidate& candidate = candidates[i];
			updateBest(i, bestForBuilding[candidate.buildingIndex], tiedBuilding[candidate.buildingIndex]);
			updateBest(i, bestForInstance[candidate.instanceIndex], tiedInstance[candidate.instanceIndex]);
		}
		for (std::size_t i = 0; i < candidates.size(); ++i)
		{
			const MatchCandidate& candidate = candidates[i];
			if (bestForBuilding[candidate.buildingIndex] != i || bestForInstance[candidate.instanceIndex] != i ||
				tiedBuilding[candidate.buildingIndex] || tiedInstance[candidate.instanceIndex])
			{
				continue;
			}
			const wi::ecs::Entity entity = instances[candidate.instanceIndex].object;
			bindings.emplace_back(entity, candidate.buildingId);
			const std::string& reason = latestBuildings[candidate.buildingIndex].stallReason;
			XMFLOAT4 color(1, 1, 1, 1);
			if (reason == "running") color = XMFLOAT4(0.55f, 1.0f, 0.55f, 1.0f);
			else if (reason == "missing_input") color = XMFLOAT4(1.0f, 0.85f, 0.25f, 1.0f);
			else if (reason == "output_full") color = XMFLOAT4(1.0f, 0.5f, 0.2f, 1.0f);
			else if (reason == "disabled") color = XMFLOAT4(0.45f, 0.45f, 0.5f, 1.0f);
			if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(entity))
			{
				object->color = color;
			}
		}
		buildings_ = std::move(latestBuildings);
		instanceBuildingIds_ = std::move(bindings);

		// Trucks: small markers at the vehicles' cells, draped on the terrain.
		const Json vehicles = state.value("vehicles", Json::array());
		while (truckEntities_.size() < vehicles.size())
		{
			const wi::ecs::Entity cube = scene.Entity_CreateCube("economy truck marker");
			if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(cube))
			{
				material->SetBaseColor(XMFLOAT4(1.0f, 0.2f, 0.1f, 1.0f));
				material->CreateRenderData(true);
			}
			truckEntities_.push_back(cube);
		}
		for (std::size_t i = 0; i < truckEntities_.size(); ++i)
		{
			wi::scene::TransformComponent* transformComponent = scene.transforms.GetComponent(truckEntities_[i]);
			if (transformComponent == nullptr)
			{
				continue;
			}
			if (i >= vehicles.size())
			{
				transformComponent->ClearTransform();
				transformComponent->Translate(XMFLOAT3(0, -10000, 0));
				transformComponent->UpdateTransform();
				continue;
			}
			const double wx = originX + (vehicles[i]["cell"].value("x", 0) + 0.5) * cell;
			const double wz = originZ + (vehicles[i]["cell"].value("z", 0) + 0.5) * cell;
			double height = 0.0;
			openrepublic::content::SampleTerrainHeight(request.heightmap, transform,
				std::min(std::max(wx, transform.originX), transform.originX + transform.sizeX),
				std::min(std::max(wz, transform.originZ), transform.originZ + transform.sizeZ), height, nullptr);
			transformComponent->ClearTransform();
			transformComponent->Scale(XMFLOAT3(3.0f, 3.0f, 3.0f));
			transformComponent->Translate(XMFLOAT3(static_cast<float>(wx - center.X()),
				static_cast<float>(height) + 3.0f, static_cast<float>(wz - center.Z())));
			transformComponent->UpdateTransform();
		}

		std::ostringstream status;
		status << "Economy overlay: tick " << state.value("tick", "?") << ", " << running << "/" << total
			<< " buildings running, " << vehicles.size() << " trucks, " << instanceBuildingIds_.size()
			<< " buildings bound";
		if (regionRadiusReduced_)
		{
			status << " (economy import radius reduced to fit its grid)";
		}
		return status.str();
	}
}
