#include "EconomyOverlay.h"

#include "json.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>

namespace openrepublic::preview
{
	using Json = nlohmann::json;

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
		while (!stop_)
		{
			std::string response;
			std::string error;
			if (!haveHello)
			{
				if (Post("{\"method\":\"hello\"}", response, error))
				{
					std::lock_guard<std::mutex> lock(mutex_);
					helloJson_ = response;
					pollError_.clear();
					haveHello = true;
				}
			}
			if (haveHello)
			{
				if (Post("{\"method\":\"world\"}", response, error))
				{
					std::lock_guard<std::mutex> lock(mutex_);
					worldJson_ = response;
					pollError_.clear();
					++sequence_;
				}
			}
			if (!error.empty())
			{
				std::lock_guard<std::mutex> lock(mutex_);
				pollError_ = error;
			}
			for (int i = 0; i < 10 && !stop_; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
	}

	std::string EconomyOverlay::Update(wi::scene::Scene& scene, const std::vector<RegionInstance>& instances,
		const openrepublic::content::MapPlacementRequest& request)
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
		if (world.empty())
		{
			return error.empty() ? "Economy overlay: waiting for the bridge..." : "Economy overlay: " + error;
		}
		if (sequence == appliedSequence_)
		{
			return std::string();
		}
		appliedSequence_ = sequence;

		const Json helloDoc = Json::parse(hello, nullptr, false);
		const Json worldDoc = Json::parse(world, nullptr, false);
		if (helloDoc.is_discarded() || worldDoc.is_discarded() || !worldDoc.value("ok", false))
		{
			return "Economy overlay: unreadable response";
		}
		const Json info = helloDoc.contains("result") ? helloDoc["result"].value("regionInfo", Json()) : Json();
		if (!info.is_object())
		{
			return "Economy overlay: the world has no regionInfo (start the host with --region)";
		}
		const double originX = info.value("originX", 0.0);
		const double originZ = info.value("originZ", 0.0);
		const double cell = info.value("cellSizeMeters", 4.0);
		const Json& state = worldDoc["result"]["world"];
		const auto& center = request.placement;
		const auto transform = openrepublic::content::MakeMapTerrainTransform(request.metadata);

		// Buildings: tint the nearest instance of the same type by stall state.
		std::size_t running = 0;
		std::size_t total = 0;
		std::vector<bool> used(instances.size(), false);
		for (const Json& building : state.value("buildings", Json::array()))
		{
			++total;
			const double wx = originX + (building["cell"].value("x", 0) + 0.5) * cell;
			const double wz = originZ + (building["cell"].value("z", 0) + 0.5) * cell;
			const std::string type = building.value("type", "");
			const std::string reason = building.value("stallReason", "");
			if (reason == "running")
			{
				++running;
			}
			std::size_t best = instances.size();
			double bestDistance = 12.0;
			for (std::size_t i = 0; i < instances.size(); ++i)
			{
				if (used[i] || instances[i].baseType != type)
				{
					continue;
				}
				const double distance = std::hypot(instances[i].x - wx, instances[i].z - wz);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					best = i;
				}
			}
			if (best == instances.size())
			{
				continue;
			}
			used[best] = true;
			XMFLOAT4 color(1, 1, 1, 1);
			if (reason == "running") color = XMFLOAT4(0.55f, 1.0f, 0.55f, 1.0f);
			else if (reason == "missing_input") color = XMFLOAT4(1.0f, 0.85f, 0.25f, 1.0f);
			else if (reason == "output_full") color = XMFLOAT4(1.0f, 0.5f, 0.2f, 1.0f);
			else if (reason == "disabled") color = XMFLOAT4(0.45f, 0.45f, 0.5f, 1.0f);
			if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(instances[best].object))
			{
				object->color = color;
			}
		}

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
			<< " buildings running, " << vehicles.size() << " trucks";
		return status.str();
	}
}
