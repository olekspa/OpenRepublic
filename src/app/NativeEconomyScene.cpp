#include "NativeEconomyScene.h"
#include "ShowcaseProps.h"
#include "openrepublic/procgeo/props.h"
#include "ShowcaseEnvironment.h"
#include "openrepublic/runtime/native_economy_tools.h"
#include "openrepublic/runtime/ambient_traffic.h"
#include "openrepublic/procgeo/vehicles.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace openrepublic::preview
{
	namespace
	{
		using Plan = runtime::NativeEconomyScenePlan;
		using BuildingPrimitive = runtime::NativeBuildingPrimitive;
		using RoadPrimitive = runtime::NativeRoadPrimitive;
		constexpr float kRoadSurfaceY = 0.11f;

		wi::ecs::Entity CreateVehicleNode(wi::scene::Scene& scene, wi::ecs::Entity mesh,
			const std::string& name)
		{
			if (mesh == wi::ecs::INVALID_ENTITY) return wi::ecs::INVALID_ENTITY;
			const wi::ecs::Entity entity = scene.Entity_CreateObject(name);
			if (entity == wi::ecs::INVALID_ENTITY) return entity;
			wi::scene::ObjectComponent* object = scene.objects.GetComponent(entity);
			if (object == nullptr) { scene.Entity_Remove(entity); return wi::ecs::INVALID_ENTITY; }
			object->meshID = mesh;
			object->SetCastShadow(true);
			return entity;
		}

		bool IsSmokePolicyBuilding(std::string_view name)
		{
			// OpenRepublic presentation policy: authored heating-plant names smoke
			// only while a recipe is actively counting down.
			return name.find("heating_plant") != std::string_view::npos ||
				name.find("heatingplant") != std::string_view::npos;
		}

		bool ToFloat(double value, float& out) noexcept
		{
			if (!std::isfinite(value) || value < -static_cast<double>(std::numeric_limits<float>::max()) ||
				value > static_cast<double>(std::numeric_limits<float>::max()))
				return false;
			out = static_cast<float>(value);
			return std::isfinite(out);
		}

		bool ToPreciseFloat(double value, float& out) noexcept
		{
			if (!ToFloat(value, out) || std::abs(value - static_cast<double>(out)) > 0.0625)
				return false;
			const float lower = std::nextafter(out, -std::numeric_limits<float>::infinity());
			const float upper = std::nextafter(out, std::numeric_limits<float>::infinity());
			const double ulp = std::max(std::abs(static_cast<double>(out) - lower),
				std::abs(static_cast<double>(upper) - out));
			return std::isfinite(ulp) && ulp <= 0.125;
		}

		XMFLOAT4 BuildingColor(economy::BuildingTypeId type)
		{
			// A stable debug palette keyed only by the native type ID. It is a visual
			// distinction, not an attempted interpretation of game materials.
		static const XMFLOAT4 palette[] = {
				XMFLOAT4(0.72f, 0.49f, 0.30f, 1.0f), XMFLOAT4(0.47f, 0.61f, 0.70f, 1.0f),
				XMFLOAT4(0.68f, 0.66f, 0.43f, 1.0f), XMFLOAT4(0.65f, 0.48f, 0.58f, 1.0f),
				XMFLOAT4(0.44f, 0.63f, 0.51f, 1.0f), XMFLOAT4(0.71f, 0.58f, 0.39f, 1.0f),
			};
			return palette[type % (sizeof(palette) / sizeof(palette[0]))];
		}

		XMFLOAT4 UtilityColor(economy::ResourceId resource)
		{
			// OpenRepublic-authored debug colors, keyed by native resource ID.
			static const XMFLOAT4 palette[] = {
				XMFLOAT4(0.18f, 0.78f, 0.95f, 1.0f), XMFLOAT4(0.96f, 0.76f, 0.18f, 1.0f),
				XMFLOAT4(0.48f, 0.92f, 0.45f, 1.0f), XMFLOAT4(0.89f, 0.42f, 0.93f, 1.0f),
				XMFLOAT4(0.98f, 0.48f, 0.26f, 1.0f), XMFLOAT4(0.44f, 0.57f, 1.0f, 1.0f),
			};
			return palette[resource % (sizeof(palette) / sizeof(palette[0]))];
		}

		bool SetMaterialColor(wi::scene::Scene& scene, wi::ecs::Entity entity, const XMFLOAT4& color)
		{
			wi::scene::MaterialComponent* material = scene.materials.GetComponent(entity);
			if (material == nullptr)
				return false;
			material->SetBaseColor(color);
			material->CreateRenderData(true);
			return true;
		}

		wi::ecs::Entity CreateBuilding(wi::scene::Scene& scene, const BuildingPrimitive& primitive, double groundY)
		{
			const wi::ecs::Entity entity = scene.Entity_CreateCube(
				"OpenRepublic native building " + std::to_string(primitive.buildingId));
			if (entity == wi::ecs::INVALID_ENTITY)
				return wi::ecs::INVALID_ENTITY;

			float x, z, halfWidth, halfDepth, halfHeight;
			if (!ToFloat(primitive.centerX, x) || !ToFloat(primitive.centerZ, z) ||
				!ToFloat(primitive.width * 0.5, halfWidth) || !ToFloat(primitive.depth * 0.5, halfDepth) ||
				!ToFloat(primitive.height * 0.5, halfHeight))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
			if (transform == nullptr || !SetMaterialColor(scene, entity, BuildingColor(primitive.typeId)))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			transform->Scale(XMFLOAT3(halfWidth, halfHeight, halfDepth));
			transform->Translate(XMFLOAT3(x, static_cast<float>(groundY) + halfHeight, z));
			return entity;
		}

		wi::ecs::Entity CreateBuildingPad(wi::scene::Scene& scene, const BuildingPrimitive& primitive, double groundY)
		{
			const wi::ecs::Entity entity = scene.Entity_CreateCube("OpenRepublic levelled building pad");
			if (entity == wi::ecs::INVALID_ENTITY) return entity;
			const float halfHeight = 0.08f;
			wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
			if (transform == nullptr || !SetMaterialColor(scene, entity, XMFLOAT4(0.36f, 0.34f, 0.28f, 1.0f)))
			{
				scene.Entity_Remove(entity); return wi::ecs::INVALID_ENTITY;
			}
			transform->Scale(XMFLOAT3(static_cast<float>(primitive.width * 0.5), halfHeight,
				static_cast<float>(primitive.depth * 0.5)));
			transform->Translate(XMFLOAT3(static_cast<float>(primitive.centerX),
				static_cast<float>(groundY) - halfHeight, static_cast<float>(primitive.centerZ)));
			return entity;
		}

		wi::ecs::Entity CreateRoad(wi::scene::Scene& scene, const RoadPrimitive& primitive, double groundY)
		{
			const wi::ecs::Entity entity = scene.Entity_CreateCube(
				"OpenRepublic native road " + std::to_string(primitive.roadId));
			if (entity == wi::ecs::INVALID_ENTITY)
				return wi::ecs::INVALID_ENTITY;

			float x, z, halfWidth, halfDepth;
			if (!ToFloat(primitive.centerX, x) || !ToFloat(primitive.centerZ, z) ||
				!ToFloat(primitive.width * 0.5, halfWidth) || !ToFloat(primitive.depth * 0.5, halfDepth))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			constexpr float roadHalfHeight = 0.055f;
			wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
			const XMFLOAT4 color = primitive.enabled ? XMFLOAT4(0.22f, 0.24f, 0.25f, 1.0f) :
				XMFLOAT4(0.48f, 0.49f, 0.50f, 1.0f);
			if (transform == nullptr || !SetMaterialColor(scene, entity, color))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			transform->Scale(XMFLOAT3(halfWidth, roadHalfHeight, halfDepth));
			transform->Translate(XMFLOAT3(x, static_cast<float>(groundY) + roadHalfHeight, z));
			return entity;
		}

		wi::ecs::Entity CreateUtilityConnection(wi::scene::Scene& scene, economy::ResourceId resource,
			economy::BuildingId a, economy::BuildingId b, double centerX, double centerY, double centerZ,
			double halfLength, double yaw)
		{
			const wi::ecs::Entity entity = scene.Entity_CreateCube(
				"OpenRepublic native utility connection " + std::to_string(resource) + " " +
				std::to_string(a) + "-" + std::to_string(b));
			if (entity == wi::ecs::INVALID_ENTITY)
				return wi::ecs::INVALID_ENTITY;

			float x, y, z, halfLengthFloat;
			if (!ToFloat(centerX, x) || !ToFloat(centerY, y) || !ToFloat(centerZ, z) ||
				!ToFloat(halfLength, halfLengthFloat))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
			if (transform == nullptr || !SetMaterialColor(scene, entity, UtilityColor(resource)))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			constexpr float halfThickness = 0.045f;
			transform->Scale(XMFLOAT3(halfLengthFloat, halfThickness, halfThickness));
			transform->Rotate(XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				static_cast<float>(yaw)));
			transform->Translate(XMFLOAT3(x, y, z));
			return entity;
		}

		wi::ecs::Entity CreateGround(wi::scene::Scene& scene, const Plan& plan, bool terrainEnabled,
			const runtime::SyntheticTerrain& terrain)
		{
			if (terrainEnabled)
			{
				constexpr std::uint32_t samples = 129;
				const double half = std::max({ 136.0, std::abs(plan.minX), std::abs(plan.maxX),
					std::abs(plan.minZ), std::abs(plan.maxZ) }) + 12.0;
				std::vector<XMFLOAT3> positions(samples * samples);
				std::vector<std::uint32_t> colors(samples * samples);
				std::vector<XMFLOAT2> uvs(samples * samples);
				std::vector<std::uint32_t> indices;
				indices.reserve((samples - 1) * (samples - 1) * 6);
				preview::ShowcaseTerrainData shade;
				shade.centerX = 0.0; shade.centerZ = 0.0; shade.nearHalfExtent = 18.0;
				shade.hasWater = true; shade.waterLevel = static_cast<float>(terrain.waterLevel);
				shade.grid.minHeight = -6.0f; shade.grid.maxHeight = 9.0f;
				for (std::uint32_t z = 0; z < samples; ++z)
				{
					for (std::uint32_t x = 0; x < samples; ++x)
					{
						const double wx = -half + 2.0 * half * x / (samples - 1);
						const double wz = -half + 2.0 * half * z / (samples - 1);
						const double h = terrain.HeightAt(wx, wz);
						const auto color = ShadeShowcaseTerrain(shade, wx, wz, static_cast<float>(h),
							static_cast<float>(terrain.SlopeAt(wx, wz)));
						const std::size_t i = static_cast<std::size_t>(z) * samples + x;
						positions[i] = XMFLOAT3(static_cast<float>(wx), static_cast<float>(h), static_cast<float>(wz));
						colors[i] = wi::Color::fromFloat4(XMFLOAT4(color.x, color.y, color.z, 1.0f));
						uvs[i] = XMFLOAT2(static_cast<float>(wx / 22.0), static_cast<float>(wz / 22.0));
					}
				}
				for (std::uint32_t z = 0; z + 1 < samples; ++z)
					for (std::uint32_t x = 0; x + 1 < samples; ++x)
					{
						const std::uint32_t a = z * samples + x, b = a + 1, c = a + samples, d = c + 1;
						indices.insert(indices.end(), { a, b, c, b, d, c });
					}
				const wi::ecs::Entity entity = scene.Entity_CreateMeshFromData("OpenRepublic seeded native terrain",
					indices.size(), indices.data(), positions.size(), positions.data());
				wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(entity);
				wi::scene::MaterialComponent* material = scene.materials.GetComponent(entity);
				if (mesh == nullptr || material == nullptr)
				{
					scene.Entity_Remove(entity); return wi::ecs::INVALID_ENTITY;
				}
				mesh->vertex_colors = std::move(colors); mesh->vertex_uvset_0 = std::move(uvs);
				mesh->vertex_normals.resize(positions.size());
				for (std::uint32_t z = 0; z < samples; ++z)
					for (std::uint32_t x = 0; x < samples; ++x)
					{
						const auto l = x == 0 ? x : x - 1, r = std::min(x + 1, samples - 1);
						const auto d = z == 0 ? z : z - 1, u = std::min(z + 1, samples - 1);
						const float hx = positions[static_cast<std::size_t>(z) * samples + r].y - positions[static_cast<std::size_t>(z) * samples + l].y;
						const float hz = positions[static_cast<std::size_t>(u) * samples + x].y - positions[static_cast<std::size_t>(d) * samples + x].y;
						XMFLOAT3 n(-hx, 4.25f, -hz); const float length = std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
						mesh->vertex_normals[static_cast<std::size_t>(z) * samples + x] = XMFLOAT3(n.x/length,n.y/length,n.z/length);
					}
				mesh->CreateRenderData(); material->SetUseVertexColors(true); material->SetBaseColor(XMFLOAT4(1,1,1,1));
				material->SetRoughness(0.96f); material->SetReflectance(0.02f); preview::ApplyShowcaseGrass(*material, false);
				material->CreateRenderData(true);
				if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(entity)) object->SetCastShadow(false);
				return entity;
			}
			const wi::ecs::Entity entity = scene.Entity_CreatePlane("OpenRepublic native grid ground");
			if (entity == wi::ecs::INVALID_ENTITY)
				return wi::ecs::INVALID_ENTITY;

			const double width = std::max(8.0, plan.maxX - plan.minX + 8.0);
			const double depth = std::max(8.0, plan.maxZ - plan.minZ + 8.0);
			const double centerX = (plan.maxX + plan.minX) * 0.5;
			const double centerZ = (plan.maxZ + plan.minZ) * 0.5;
			float x, z, halfWidth, halfDepth;
			if (!ToFloat(centerX, x) || !ToFloat(centerZ, z) ||
				!ToFloat(width * 0.5, halfWidth) || !ToFloat(depth * 0.5, halfDepth))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
			if (transform == nullptr || !SetMaterialColor(scene, entity, XMFLOAT4(0.29f, 0.34f, 0.28f, 1.0f)))
			{
				scene.Entity_Remove(entity);
				return wi::ecs::INVALID_ENTITY;
			}
			transform->Scale(XMFLOAT3(halfWidth, 1.0f, halfDepth));
			transform->Translate(XMFLOAT3(x, 0.0f, z));
			return entity;
		}

		wi::ecs::Entity CreateNativeLake(wi::scene::Scene& scene, const runtime::SyntheticTerrain& terrain)
		{
			constexpr std::uint32_t segments = 96;
			constexpr float cx = 38.0f, cz = -27.0f, rx = 14.0f, rz = 9.8f;
			std::vector<XMFLOAT3> positions;
			std::vector<std::uint32_t> indices;
			positions.reserve(segments + 1); indices.reserve(segments * 3);
			positions.emplace_back(cx, static_cast<float>(terrain.waterLevel), cz);
			for (std::uint32_t i = 0; i < segments; ++i)
			{
				const float angle = 6.28318530718f * i / segments;
				positions.emplace_back(cx + rx * std::cos(angle), static_cast<float>(terrain.waterLevel),
					cz + rz * std::sin(angle));
				indices.insert(indices.end(), { 0u, i + 1, (i + 1) % segments + 1 });
			}
			const wi::ecs::Entity entity = scene.Entity_CreateMeshFromData("OpenRepublic synthetic lake",
				indices.size(), indices.data(), positions.size(), positions.data());
			if (auto* material = scene.materials.GetComponent(entity))
			{
				material->SetBaseColor(XMFLOAT4(0.035f, 0.16f, 0.23f, 0.92f));
				material->SetRoughness(0.16f); material->SetReflectance(0.72f); material->CreateRenderData(true);
			}
			else { scene.Entity_Remove(entity); return wi::ecs::INVALID_ENTITY; }
			if (auto* object = scene.objects.GetComponent(entity)) object->SetCastShadow(false);
			return entity;
		}

		bool FitsFloat(const Plan& plan)
		{
			float ignored;
			const double targetX = plan.minX * 0.5 + plan.maxX * 0.5;
			const double targetZ = plan.minZ * 0.5 + plan.maxZ * 0.5;
			if (!ToFloat(plan.cameraDistance, ignored) || !ToFloat(targetX, ignored) || !ToFloat(targetZ, ignored))
				return false;
			for (const BuildingPrimitive& building : plan.buildings)
			{
				if (!ToFloat(building.centerX, ignored) || !ToFloat(building.centerZ, ignored) ||
					!ToFloat(building.width, ignored) || !ToFloat(building.depth, ignored) ||
					!ToFloat(building.height, ignored)) return false;
			}
			for (const RoadPrimitive& road : plan.roads)
			{
				if (!ToFloat(road.centerX, ignored) || !ToFloat(road.centerZ, ignored) ||
					!ToFloat(road.width, ignored) || !ToFloat(road.depth, ignored)) return false;
			}
			return true;
		}

		bool ValidateRendererPlan(const Plan& plan, std::string& error)
		{
			if (!runtime::NativeEconomyPlanFitsFloatPrecision(plan, error))
				return false;
			if (!FitsFloat(plan))
			{
				error = "native scene camera or geometry exceeds the renderer's finite float range";
				return false;
			}
			return true;
		}

	}

	bool NativeEconomyScene::Initialize(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
		const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error,
		bool nativeTerrain)
	{
		error.clear();
		if (initialized_)
		{
			error = "native economy scene is already initialized";
			return false;
		}
		terrainEnabled_ = nativeTerrain;
		Plan plan;
		if (!runtime::BuildNativeEconomyScenePlan(snapshot, catalog, plan, error) || !ValidateRendererPlan(plan, error))
		{
			if (error.empty()) error = "native economy scene cannot retain per-cell float precision";
			return false;
		}
		transform_ = plan.transform;
		if (!Sync(scene, snapshot, catalog, outInstances, error))
		{
			transform_.reset();
			return false;
		}
		initialized_ = true;
		return true;
	}

	bool NativeEconomyScene::Rebase(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
		const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error)
	{
		error.clear();
		if (!initialized_)
		{
			error = "native economy scene must be initialized before rebasing";
			return false;
		}
		Plan plan;
		if (!runtime::BuildNativeEconomyScenePlan(snapshot, catalog, plan, error) || !ValidateRendererPlan(plan, error))
		{
			if (error.empty()) error = "native economy scene cannot retain per-cell float precision";
			return false;
		}
		const std::optional<runtime::NativeEconomyTransform> previousTransform = transform_;
		transform_ = plan.transform;
		if (!Sync(scene, snapshot, catalog, outInstances, error))
		{
			transform_ = previousTransform;
			return false;
		}
		return true;
	}

	bool NativeEconomyScene::Sync(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
		const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error)
	{
		error.clear();
		if (initialized_ && !transform_)
		{
			error = "native economy scene lost its fixed coordinate transform";
			return false;
		}
		Plan plan;
		if (!runtime::BuildNativeEconomyScenePlan(snapshot, catalog, plan, error,
			transform_ ? &*transform_ : nullptr) || !ValidateRendererPlan(plan, error))
		{
			if (error.empty()) error = "native economy scene cannot retain per-cell float precision";
			return false;
		}
		if (!transform_) transform_ = plan.transform;

		std::unordered_set<economy::BuildingId> seenBuildings;
		std::unordered_set<economy::RoadId> seenRoads;
		std::unordered_set<economy::VehicleId> seenVehicles;
		seenBuildings.reserve(plan.buildings.size());
		seenRoads.reserve(plan.roads.size());
		seenVehicles.reserve(snapshot.vehicles.size());
		for (const BuildingPrimitive& primitive : plan.buildings)
			if (!seenBuildings.insert(primitive.buildingId).second)
			{
				error = "native economy scene plan contains duplicate building IDs";
				return false;
			}
		for (const RoadPrimitive& primitive : plan.roads)
			if (!seenRoads.insert(primitive.roadId).second)
			{
				error = "native economy scene plan contains duplicate road IDs";
				return false;
			}

		struct NewBuilding { economy::BuildingId id; BuildingVisual visual; };
		struct NewRoad { economy::RoadId id; RoadVisual visual; };
		std::vector<NewBuilding> addedBuildings;
		std::vector<NewRoad> addedRoads;
		std::vector<UtilityVisual> desiredUtilities;
		std::vector<UtilityVisual> addedUtilities;
		std::vector<wi::ecs::Entity> createdEntities;
		auto sameBuilding = [](const BuildingVisual& visual, const BuildingPrimitive& primitive)
		{
			return visual.typeId == primitive.typeId && visual.centerX == primitive.centerX &&
				visual.centerZ == primitive.centerZ && visual.width == primitive.width &&
				visual.depth == primitive.depth && visual.height == primitive.height;
		};
		auto sameRoad = [](const RoadVisual& visual, const RoadPrimitive& primitive)
		{
			return visual.centerX == primitive.centerX && visual.centerZ == primitive.centerZ &&
				visual.width == primitive.width && visual.depth == primitive.depth && visual.enabled == primitive.enabled;
		};
		auto sameUtilityGeometry = [](const UtilityVisual& lhs, const UtilityVisual& rhs)
		{
			return lhs.resource == rhs.resource && lhs.a == rhs.a && lhs.b == rhs.b &&
				lhs.centerX == rhs.centerX && lhs.centerZ == rhs.centerZ && lhs.centerY == rhs.centerY &&
				lhs.halfLength == rhs.halfLength && lhs.yaw == rhs.yaw;
		};
		auto rollback = [&]()
		{
			for (wi::ecs::Entity entity : createdEntities) scene.Entity_Remove(entity);
		};

		for (const BuildingPrimitive& primitive : plan.buildings)
		{
			auto old = buildings_.find(primitive.buildingId);
			if (old != buildings_.end() && sameBuilding(old->second, primitive)) continue;
			const wi::ecs::Entity entity = CreateBuilding(scene, primitive,
				terrainEnabled_ ? terrain_.HeightAt(primitive.centerX, primitive.centerZ) : 0.0);
			if (entity == wi::ecs::INVALID_ENTITY)
			{
				rollback();
				error = "Wicked Engine could not create a native building primitive";
				return false;
			}
			createdEntities.push_back(entity);
			wi::ecs::Entity pad = wi::ecs::INVALID_ENTITY;
			if (terrainEnabled_)
			{
				pad = CreateBuildingPad(scene, primitive, terrain_.HeightAt(primitive.centerX, primitive.centerZ));
				if (pad == wi::ecs::INVALID_ENTITY)
				{
					rollback(); error = "Wicked Engine could not create a levelled terrain pad"; return false;
				}
				createdEntities.push_back(pad);
			}
			addedBuildings.push_back({ primitive.buildingId, { entity, pad, primitive.typeId, primitive.centerX,
				primitive.centerZ, primitive.width, primitive.depth, primitive.height } });
		}
		std::unordered_set<economy::RailId> seenRails;
		std::vector<std::pair<economy::RailSnapshot, wi::ecs::Entity>> addedRails;
		seenRails.reserve(snapshot.rails.size());
		for (const auto& rail : snapshot.rails)
		{
			if (!rail.id || !seenRails.insert(rail.id).second)
			{
				rollback(); error = "native economy snapshot contains a zero or duplicate rail ID"; return false;
			}
			const auto old = rails_.find(rail.id);
			if (old != rails_.end() && old->second.a == rail.a && old->second.b == rail.b) continue;
			runtime::NativeRailGeometry geometry;
			if (!runtime::MapNativeRailGeometry(rail.a, rail.b, plan.transform, geometry))
			{
				rollback(); error = "native economy snapshot contains invalid rail geometry"; return false;
			}
			const auto entity = CreateProceduralMeshEntity(scene, ("OpenRepublic native rail " + std::to_string(rail.id)).c_str(),
				openrepublic::procgeo::MakeRailSegment(static_cast<float>(geometry.length)), 0.82f, 0.18f);
			auto* transform = scene.transforms.GetComponent(entity);
			float x, z;
			if (entity == wi::ecs::INVALID_ENTITY || !transform || !ToFloat(geometry.center.x, x) || !ToFloat(geometry.center.z, z))
			{
				if (entity != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(entity);
				rollback(); error = "Wicked Engine could not create a native procedural rail segment"; return false;
			}
			transform->Rotate(XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), static_cast<float>(geometry.yawRadians)));
			transform->Translate(XMFLOAT3(x, static_cast<float>(GroundHeight(x, z)) + kRoadSurfaceY, z));
			createdEntities.push_back(entity);
			addedRails.emplace_back(rail, entity);
		}
		for (const RoadPrimitive& primitive : plan.roads)
		{
			auto old = roads_.find(primitive.roadId);
			if (old != roads_.end() && sameRoad(old->second, primitive)) continue;
			const wi::ecs::Entity entity = CreateRoad(scene, primitive,
				terrainEnabled_ ? terrain_.HeightAt(primitive.centerX, primitive.centerZ) : 0.0);
			if (entity == wi::ecs::INVALID_ENTITY)
			{
				rollback();
				error = "Wicked Engine could not create a native road primitive";
				return false;
			}
			createdEntities.push_back(entity);
			addedRoads.push_back({ primitive.roadId, { entity, primitive.centerX, primitive.centerZ,
				primitive.width, primitive.depth, primitive.enabled } });
		}
		struct NewVehicle { economy::VehicleId id; VehicleVisual visual; };
		std::vector<NewVehicle> addedVehicles;
		addedVehicles.reserve(snapshot.vehicles.size());
		if (!snapshot.vehicles.empty() && wheelMesh_ == wi::ecs::INVALID_ENTITY)
		{
			wheelMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic native wheel mesh",
				openrepublic::procgeo::MakeVehicleWheel(), 0.82f, 0.0f);
			cargoMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic native coal heap mesh",
				openrepublic::procgeo::MakeCoalHeap(), 0.09f, 0.0f);
			headlightsMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic native headlight mesh",
				openrepublic::procgeo::MakeVehicleHeadlightPair(), 0.25f, 0.0f);
			brakeLightsMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic native brake light mesh",
				openrepublic::procgeo::MakeVehicleBrakeLightPair(), 0.25f, 0.0f);
			exhaustMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic native exhaust puff mesh",
				openrepublic::procgeo::MakeVehicleExhaustPuff(), 0.95f, 0.0f);
			if (wheelMesh_ == wi::ecs::INVALID_ENTITY || cargoMesh_ == wi::ecs::INVALID_ENTITY ||
				headlightsMesh_ == wi::ecs::INVALID_ENTITY || brakeLightsMesh_ == wi::ecs::INVALID_ENTITY ||
				exhaustMesh_ == wi::ecs::INVALID_ENTITY)
			{
				if (wheelMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(wheelMesh_);
				if (cargoMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(cargoMesh_);
				if (headlightsMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(headlightsMesh_);
				if (brakeLightsMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(brakeLightsMesh_);
				if (exhaustMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(exhaustMesh_);
				wheelMesh_ = cargoMesh_ = headlightsMesh_ = brakeLightsMesh_ = exhaustMesh_ = wi::ecs::INVALID_ENTITY;
				error = "Wicked Engine could not create the native procedural truck meshes";
				return false;
			}
			if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(headlightsMesh_))
			{
				material->SetEmissiveColor(XMFLOAT4(1.0f, 0.78f, 0.46f, 0.65f));
				material->CreateRenderData(true);
			}
		}
		for (const auto& vehicle : snapshot.vehicles)
		{
			if (!seenVehicles.insert(vehicle.id).second)
			{
				rollback(); error = "native economy snapshot contains duplicate vehicle IDs"; return false;
			}
			VehicleVisual visual{};
			const auto old = vehicles_.find(vehicle.id);
			if (old != vehicles_.end()) visual = old->second;
			const economy::VehicleTypeDef* vehicleType = catalog.VehicleType(vehicle.type);
			if (vehicleType != nullptr)
			{
				visual.kind = openrepublic::procgeo::VehicleKindForCatalogName(vehicleType->name, vehicleType->railOnly);
				const auto catalogKind = visual.kind;
				const bool cargoTruck = catalogKind == openrepublic::procgeo::VehicleKind::CargoTruck ||
					catalogKind == openrepublic::procgeo::VehicleKind::BoxVan ||
					catalogKind == openrepublic::procgeo::VehicleKind::Flatbed ||
					catalogKind == openrepublic::procgeo::VehicleKind::DumpTruck ||
					catalogKind == openrepublic::procgeo::VehicleKind::Tanker;
				if (cargoTruck)
					for (const auto& order : snapshot.orders)
						if (order.id == vehicle.order)
						{
							if (const economy::ResourceDef* resource = catalog.Resource(order.resource))
								visual.kind = openrepublic::procgeo::TruckKindForResourceName(resource->name);
							break;
						}
			}
			if (visual.bodyMesh == wi::ecs::INVALID_ENTITY || (old != vehicles_.end() && visual.kind != old->second.kind))
			{
				const std::string meshName = "OpenRepublic native vehicle body mesh " + std::to_string(vehicle.id);
				visual.bodyMesh = CreateProceduralMeshEntity(scene, meshName.c_str(),
					openrepublic::procgeo::MakeVehicleBody(visual.kind, static_cast<std::uint32_t>(vehicle.id)), 0.5f, 0.1f);
				if (visual.bodyMesh == wi::ecs::INVALID_ENTITY)
				{
					rollback(); error = "Wicked Engine could not create a native vehicle body mesh"; return false;
				}
				createdEntities.push_back(visual.bodyMesh);
				if (old == vehicles_.end())
					visual.body = CreateVehicleNode(scene, visual.bodyMesh, "OpenRepublic native vehicle " + std::to_string(vehicle.id));
			}
			if (old == vehicles_.end())
			{
				visual.load = CreateVehicleNode(scene, cargoMesh_, "OpenRepublic native coal load " + std::to_string(vehicle.id));
				visual.headlights = CreateVehicleNode(scene, headlightsMesh_, "OpenRepublic native headlights " + std::to_string(vehicle.id));
				visual.brakeLights = CreateVehicleNode(scene, brakeLightsMesh_, "OpenRepublic native brake lights " + std::to_string(vehicle.id));
				visual.exhaust = CreateVehicleNode(scene, exhaustMesh_, "OpenRepublic native exhaust " + std::to_string(vehicle.id));
				visual.labelMesh = CreateProceduralMeshEntity(scene, "OpenRepublic native truck ID label mesh",
					openrepublic::procgeo::MakeVehicleIdLabel(vehicle.id), 1.0f, 0.0f);
				visual.label = CreateVehicleNode(scene, visual.labelMesh,
					"OpenRepublic native truck ID " + std::to_string(vehicle.id));
				for (wi::ecs::Entity& wheel : visual.wheels)
					wheel = CreateVehicleNode(scene, wheelMesh_, "OpenRepublic native truck wheel " + std::to_string(vehicle.id));
				if (visual.kind == openrepublic::procgeo::VehicleKind::Locomotive)
					for (std::size_t wagon = 0; wagon < visual.wagons.size(); ++wagon)
					{
						visual.wagonMeshes[wagon] = CreateProceduralMeshEntity(scene, "OpenRepublic native train wagon mesh",
							openrepublic::procgeo::MakeTrainWagon(static_cast<std::uint32_t>(vehicle.id + wagon)), 0.56f, 0.08f);
						visual.wagons[wagon] = CreateVehicleNode(scene, visual.wagonMeshes[wagon],
							"OpenRepublic native train wagon " + std::to_string(vehicle.id) + " " + std::to_string(wagon + 1));
					}
				if (visual.body == wi::ecs::INVALID_ENTITY || visual.load == wi::ecs::INVALID_ENTITY ||
					visual.headlights == wi::ecs::INVALID_ENTITY || visual.brakeLights == wi::ecs::INVALID_ENTITY ||
					visual.exhaust == wi::ecs::INVALID_ENTITY ||
					visual.labelMesh == wi::ecs::INVALID_ENTITY || visual.label == wi::ecs::INVALID_ENTITY ||
					std::any_of(visual.wheels.begin(), visual.wheels.end(), [](wi::ecs::Entity e) { return e == wi::ecs::INVALID_ENTITY; }) ||
					(visual.kind == openrepublic::procgeo::VehicleKind::Locomotive &&
					 std::any_of(visual.wagons.begin(), visual.wagons.end(), [](wi::ecs::Entity e) { return e == wi::ecs::INVALID_ENTITY; })))
				{
					if (visual.body != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.body);
					if (visual.bodyMesh != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.bodyMesh);
					if (visual.load != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.load);
					if (visual.headlights != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.headlights);
					if (visual.brakeLights != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.brakeLights);
					if (visual.exhaust != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.exhaust);
					if (visual.label != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.label);
					if (visual.labelMesh != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(visual.labelMesh);
					for (wi::ecs::Entity wheel : visual.wheels) if (wheel != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(wheel);
					for (wi::ecs::Entity wagon : visual.wagons) if (wagon != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(wagon);
					for (wi::ecs::Entity mesh : visual.wagonMeshes) if (mesh != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(mesh);
					rollback(); error = "Wicked Engine could not create a native vehicle node"; return false;
				}
				createdEntities.push_back(visual.body);
				createdEntities.push_back(visual.load);
				createdEntities.push_back(visual.headlights);
				createdEntities.push_back(visual.brakeLights);
				createdEntities.push_back(visual.exhaust);
				createdEntities.push_back(visual.label);
				createdEntities.push_back(visual.labelMesh);
				createdEntities.insert(createdEntities.end(), visual.wheels.begin(), visual.wheels.end());
				createdEntities.insert(createdEntities.end(), visual.wagons.begin(), visual.wagons.end());
				createdEntities.insert(createdEntities.end(), visual.wagonMeshes.begin(), visual.wagonMeshes.end());
				if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(visual.load))
				{
					material->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
					material->CreateRenderData(true);
				}
				visual.hasPrevious = false;
			}
			visual.wheelRotationBase = visual.wheelRotation;
			visual.previous = visual.hasPrevious ? visual.current : vehicle.cell;
			visual.current = vehicle.cell;
			visual.hasPrevious = true;
			visual.loaded = vehicle.cargo > 0;
			visual.idle = vehicle.state == economy::HaulState::Idle || vehicle.state == economy::HaulState::Waiting ||
				vehicle.state == economy::HaulState::NoRoute;
			addedVehicles.push_back({ vehicle.id, visual });
		}

		std::unordered_map<economy::BuildingId, const BuildingPrimitive*> presentationBuildings;
		for (const BuildingPrimitive& primitive : plan.buildings) presentationBuildings.emplace(primitive.buildingId, &primitive);
		std::vector<std::pair<economy::BuildingId, BuildingEffect>> desiredEffects;
		for (const auto& building : snapshot.buildings)
		{
			const auto* type = catalog.BuildingType(building.type);
			const bool active = type != nullptr && IsSmokePolicyBuilding(type->name) &&
				std::any_of(building.recipes.begin(), building.recipes.end(), [](const auto& recipe) { return recipe.remainingTicks > 0; });
			if (!active || desiredEffects.size() >= 8) continue;
			const auto old = buildingEffects_.find(building.id);
			if (old != buildingEffects_.end() && old->second.active)
			{
				desiredEffects.push_back({ building.id, old->second });
				continue;
			}
			const auto geometry = presentationBuildings.find(building.id);
			if (geometry == presentationBuildings.end()) continue;
			const auto& shape = *geometry->second;
			const wi::ecs::Entity emitterEntity = scene.Entity_CreateEmitter("OpenRepublic active production smoke",
				XMFLOAT3(static_cast<float>(shape.centerX), static_cast<float>(GroundHeight(shape.centerX, shape.centerZ) + shape.height + 0.3),
					static_cast<float>(shape.centerZ)));
			if (emitterEntity == wi::ecs::INVALID_ENTITY)
			{
				rollback(); error = "Wicked Engine could not create a production smoke emitter"; return false;
			}
			createdEntities.push_back(emitterEntity);
			if (wi::EmittedParticleSystem* emitter = scene.emitters.GetComponent(emitterEntity))
			{
				emitter->count = 4.0f; emitter->size = 2.5f; emitter->life = 9.0f;
				emitter->random_life = 0.35f; emitter->normal_factor = 0.0f; emitter->random_factor = 0.35f;
				emitter->velocity = XMFLOAT3(0.25f, 1.2f, 0.0f); emitter->scaleX = 1.3f;
				emitter->mass = -0.01f; emitter->drag = 0.99f; emitter->rotation = 0.4f;
			}
			if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(emitterEntity))
			{
				material->SetBaseColor(XMFLOAT4(0.58f, 0.57f, 0.54f, 0.48f));
				material->userBlendMode = wi::enums::BLENDMODE_ALPHA;
			}
			desiredEffects.push_back({ building.id, { emitterEntity, true } });
		}

		std::unordered_map<economy::BuildingId, const BuildingPrimitive*> buildingGeometry;
		buildingGeometry.reserve(plan.buildings.size());
		for (const BuildingPrimitive& primitive : plan.buildings)
			buildingGeometry.emplace(primitive.buildingId, &primitive);
		std::vector<economy::UtilityConnectionSnapshot> sortedConnections = snapshot.utilityConnections;
		for (economy::UtilityConnectionSnapshot& connection : sortedConnections)
			if (connection.b < connection.a) std::swap(connection.a, connection.b);
		std::sort(sortedConnections.begin(), sortedConnections.end(), [](const auto& lhs, const auto& rhs)
		{
			if (lhs.resource != rhs.resource) return lhs.resource < rhs.resource;
			if (lhs.a != rhs.a) return lhs.a < rhs.a;
			return lhs.b < rhs.b;
		});
		for (std::size_t i = 0; i < sortedConnections.size(); ++i)
		{
			const economy::UtilityConnectionSnapshot& connection = sortedConnections[i];
			if (connection.a == connection.b || (i > 0 && sortedConnections[i - 1].resource == connection.resource &&
				sortedConnections[i - 1].a == connection.a && sortedConnections[i - 1].b == connection.b))
			{
				rollback();
				error = "native economy snapshot contains an invalid or duplicate utility connection";
				return false;
			}
			const economy::ResourceDef* resource = catalog.Resource(connection.resource);
			const auto a = buildingGeometry.find(connection.a);
			const auto b = buildingGeometry.find(connection.b);
			if (resource == nullptr || !resource->utility || a == buildingGeometry.end() || b == buildingGeometry.end())
			{
				rollback();
				error = "native economy utility connection does not resolve to a utility resource and live buildings";
				return false;
			}
			const double dx = b->second->centerX - a->second->centerX;
			const double dz = b->second->centerZ - a->second->centerZ;
			const double length = std::hypot(dx, dz);
			const double centerX = a->second->centerX + dx * 0.5;
			const double centerZ = a->second->centerZ + dz * 0.5;
			const double centerY = std::max(GroundHeight(a->second->centerX, a->second->centerZ) + a->second->height,
				GroundHeight(b->second->centerX, b->second->centerZ) + b->second->height) + 0.25;
			const double yaw = std::atan2(-dz, dx);
			float ignored;
			if (!std::isfinite(length) || length <= 0.0 || !ToPreciseFloat(centerX, ignored) ||
				!ToPreciseFloat(centerZ, ignored) || !ToPreciseFloat(centerY, ignored) ||
				!ToPreciseFloat(length * 0.5, ignored) || !ToFloat(yaw, ignored))
			{
				rollback();
				error = "native utility connection exceeds the renderer's finite float range";
				return false;
			}
			desiredUtilities.push_back({ wi::ecs::INVALID_ENTITY, connection.resource, connection.a, connection.b,
				centerX, centerZ, centerY, length * 0.5, yaw });
		}
		bool utilityGeometryChanged = desiredUtilities.size() != utilityConnections_.size();
		if (!utilityGeometryChanged)
			for (std::size_t i = 0; i < desiredUtilities.size(); ++i)
				if (!sameUtilityGeometry(desiredUtilities[i], utilityConnections_[i]))
				{
					utilityGeometryChanged = true;
					break;
				}
		if (utilityGeometryChanged)
		{
			addedUtilities.reserve(desiredUtilities.size());
			for (UtilityVisual& visual : desiredUtilities)
			{
				visual.entity = CreateUtilityConnection(scene, visual.resource, visual.a, visual.b,
					visual.centerX, visual.centerY, visual.centerZ, visual.halfLength, visual.yaw);
				if (visual.entity == wi::ecs::INVALID_ENTITY)
				{
					rollback();
					error = "Wicked Engine could not create a native utility connection primitive";
					return false;
				}
				createdEntities.push_back(visual.entity);
				addedUtilities.push_back(visual);
			}
		}

		wi::ecs::Entity newGround = wi::ecs::INVALID_ENTITY;
		if (groundEntity_ == wi::ecs::INVALID_ENTITY)
		{
			newGround = CreateGround(scene, plan, terrainEnabled_, terrain_);
			if (newGround == wi::ecs::INVALID_ENTITY)
			{
				rollback();
				error = "Wicked Engine could not create the native grid ground";
				return false;
			}
			createdEntities.push_back(newGround);
		}
		wi::ecs::Entity newWater = wi::ecs::INVALID_ENTITY;
		if (terrainEnabled_ && waterEntity_ == wi::ecs::INVALID_ENTITY)
		{
			newWater = CreateNativeLake(scene, terrain_);
			if (newWater == wi::ecs::INVALID_ENTITY)
			{
				rollback(); error = "Wicked Engine could not create the synthetic lake"; return false;
			}
			createdEntities.push_back(newWater);
		}
		const wi::ecs::Entity targetGround = newGround != wi::ecs::INVALID_ENTITY ? newGround : groundEntity_;
		wi::scene::TransformComponent* groundTransform = scene.transforms.GetComponent(targetGround);
		const double groundWidth = std::max(8.0, plan.maxX - plan.minX + 8.0);
		const double groundDepth = std::max(8.0, plan.maxZ - plan.minZ + 8.0);
		const double groundCenterX = (plan.maxX + plan.minX) * 0.5;
		const double groundCenterZ = (plan.maxZ + plan.minZ) * 0.5;
		float groundHalfWidth, groundHalfDepth, groundX, groundZ;
		if (groundTransform == nullptr || !ToFloat(groundWidth * 0.5, groundHalfWidth) ||
			!ToFloat(groundDepth * 0.5, groundHalfDepth) || !ToFloat(groundCenterX, groundX) ||
			!ToFloat(groundCenterZ, groundZ))
		{
			rollback();
			error = "Wicked Engine could not update the native ground transform";
			return false;
		}
		std::vector<RegionInstance> instances;
		instances.reserve(plan.buildings.size());
		for (const BuildingPrimitive& primitive : plan.buildings)
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			const auto replacement = std::find_if(addedBuildings.begin(), addedBuildings.end(),
				[&](const NewBuilding& item) { return item.id == primitive.buildingId; });
			if (replacement != addedBuildings.end())
				entity = replacement->visual.entity;
			else
			{
				const auto existing = buildings_.find(primitive.buildingId);
				if (existing != buildings_.end()) entity = existing->second.entity;
			}
			const economy::BuildingTypeDef* type = catalog.BuildingType(primitive.typeId);
			if (entity == wi::ecs::INVALID_ENTITY || type == nullptr)
			{
				rollback();
				error = "native economy scene could not prepare a stable building binding";
				return false;
			}
			RegionInstance instance;
			instance.object = entity;
			instance.baseType = type->name;
			instance.x = primitive.centerX;
			instance.z = primitive.centerZ;
			instances.push_back(std::move(instance));
		}

		// Everything that can fail has been prepared. Reconcile by native IDs only;
		// package placement indices remain untouched and carry no native identity.
		for (auto it = buildings_.begin(); it != buildings_.end();)
		{
			const bool removed = seenBuildings.find(it->first) == seenBuildings.end();
			const bool replaced = std::any_of(addedBuildings.begin(), addedBuildings.end(),
				[&](const NewBuilding& item) { return item.id == it->first; });
			if (removed || replaced)
			{
				buildingByEntity_.erase(it->second.entity);
				scene.Entity_Remove(it->second.entity);
				if (it->second.pad != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(it->second.pad);
				it = buildings_.erase(it);
			}
			else ++it;
		}
		for (auto it = roads_.begin(); it != roads_.end();)
		{
			const bool removed = seenRoads.find(it->first) == seenRoads.end();
			const bool replaced = std::any_of(addedRoads.begin(), addedRoads.end(),
				[&](const NewRoad& item) { return item.id == it->first; });
			if (removed || replaced)
			{
				scene.Entity_Remove(it->second.entity);
				it = roads_.erase(it);
			}
			else ++it;
		}
		for (auto it = rails_.begin(); it != rails_.end();)
		{
			const bool replaced = std::any_of(snapshot.rails.begin(), snapshot.rails.end(), [&](const auto& rail) {
				return rail.id == it->first && (rail.a != it->second.a || rail.b != it->second.b);
			});
			if (!seenRails.count(it->first) || replaced) { scene.Entity_Remove(it->second.entity); it = rails_.erase(it); }
			else ++it;
		}
		for (auto it = vehicles_.begin(); it != vehicles_.end();)
		{
			if (seenVehicles.find(it->first) == seenVehicles.end())
			{
				scene.Entity_Remove(it->second.body);
				scene.Entity_Remove(it->second.load);
				scene.Entity_Remove(it->second.headlights);
				scene.Entity_Remove(it->second.brakeLights);
				scene.Entity_Remove(it->second.exhaust);
				scene.Entity_Remove(it->second.label);
				scene.Entity_Remove(it->second.labelMesh);
				for (wi::ecs::Entity wheel : it->second.wheels) scene.Entity_Remove(wheel);
				scene.Entity_Remove(it->second.bodyMesh);
				for (wi::ecs::Entity wagon : it->second.wagons) scene.Entity_Remove(wagon);
				for (wi::ecs::Entity mesh : it->second.wagonMeshes) scene.Entity_Remove(mesh);
				it = vehicles_.erase(it);
			}
			else ++it;
		}
		std::unordered_set<economy::BuildingId> activeEffectIds;
		for (const auto& desired : desiredEffects) activeEffectIds.insert(desired.first);
		for (auto it = buildingEffects_.begin(); it != buildingEffects_.end();)
		{
			if (activeEffectIds.find(it->first) == activeEffectIds.end())
			{
				scene.Entity_Remove(it->second.entity);
				it = buildingEffects_.erase(it);
			}
			else ++it;
		}
		if (utilityGeometryChanged)
		{
			for (const UtilityVisual& visual : utilityConnections_) scene.Entity_Remove(visual.entity);
			utilityConnections_ = std::move(addedUtilities);
		}
		for (NewBuilding& item : addedBuildings)
		{
			buildingByEntity_[item.visual.entity] = item.id;
			buildings_.emplace(item.id, item.visual);
		}
		for (NewRoad& item : addedRoads) roads_.emplace(item.id, item.visual);
		for (const auto& rail : addedRails) rails_[rail.first.id] = RailVisual{rail.second, rail.first.a, rail.first.b};
		for (NewVehicle& item : addedVehicles)
		{
			auto old = vehicles_.find(item.id);
			if (old != vehicles_.end() && old->second.bodyMesh != item.visual.bodyMesh)
			{
				if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(item.visual.body))
					object->meshID = item.visual.bodyMesh;
				scene.Entity_Remove(old->second.bodyMesh);
			}
			vehicles_[item.id] = item.visual;
		}
		for (const auto& desired : desiredEffects) buildingEffects_[desired.first] = desired.second;
		if (newGround != wi::ecs::INVALID_ENTITY) groundEntity_ = newGround;
		if (newWater != wi::ecs::INVALID_ENTITY) waterEntity_ = newWater;
		if (!terrainEnabled_)
		{
			groundTransform->scale_local = XMFLOAT3(groundHalfWidth, 1.0f, groundHalfDepth);
			groundTransform->translation_local = XMFLOAT3(groundX, 0.0f, groundZ);
			groundTransform->UpdateTransform();
		}

		outInstances = std::move(instances);
		cameraDistance_ = static_cast<float>(plan.cameraDistance);
		cameraTarget_ = DirectX::XMFLOAT3(
			static_cast<float>(plan.minX * 0.5 + plan.maxX * 0.5), 0.0f,
			static_cast<float>(plan.minZ * 0.5 + plan.maxZ * 0.5));
		initialized_ = true;
		if (!SyncAmbientTraffic(scene, snapshot, error)) return false;
		return true;
	}

	bool NativeEconomyScene::SyncAmbientTraffic(wi::scene::Scene& scene,
		const economy::WorldSnapshot& snapshot, std::string& error)
	{
		if (!ambientTrafficEnabled_)
		{
			for (const AmbientVehicleVisual& visual : ambientVehicles_) scene.Entity_Remove(visual.entity);
			ambientVehicles_.clear();
			if (ambientCarMesh_ != wi::ecs::INVALID_ENTITY) { scene.Entity_Remove(ambientCarMesh_); ambientCarMesh_ = wi::ecs::INVALID_ENTITY; }
			return true;
		}
		if (ambientCarMesh_ == wi::ecs::INVALID_ENTITY)
		{
			ambientCarMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic ambient traffic car mesh",
				openrepublic::procgeo::MakeVehicleBody(openrepublic::procgeo::VehicleKind::Car, 91u), 0.18f, 0.0f);
			if (ambientCarMesh_ == wi::ecs::INVALID_ENTITY) { error = "could not create ambient traffic car mesh"; return false; }
		}
		std::vector<std::vector<economy::Cell>> lines;
		lines.reserve(snapshot.roads.size());
		for (const auto& road : snapshot.roads) if (road.enabled) lines.push_back({road.a, road.b});
		const auto traffic = runtime::ComputeAmbientTraffic(lines, snapshot.tick, 8);
		while (ambientVehicles_.size() > traffic.size())
		{
			scene.Entity_Remove(ambientVehicles_.back().entity);
			ambientVehicles_.pop_back();
		}
		while (ambientVehicles_.size() < traffic.size())
		{
			const wi::ecs::Entity entity = CreateVehicleNode(scene, ambientCarMesh_,
				"OpenRepublic ambient road car " + std::to_string(ambientVehicles_.size()));
			if (entity == wi::ecs::INVALID_ENTITY) { error = "could not create ambient traffic vehicle"; return false; }
			ambientVehicles_.push_back({entity});
		}
		const float scale = static_cast<float>(runtime::NativeVehicleDisplayScale(
			openrepublic::procgeo::VehicleLength(openrepublic::procgeo::VehicleKind::Car), 0.65));
		for (std::size_t i = 0; i < traffic.size(); ++i)
		{
			const auto& item = traffic[i];
			const float x = static_cast<float>((item.position.x - transform_->centerCellX) * runtime::NativeEconomyTransform::DisplayUnitsPerCell);
			const float z = static_cast<float>((item.position.z - transform_->centerCellZ) * runtime::NativeEconomyTransform::DisplayUnitsPerCell);
			wi::scene::TransformComponent* transform = scene.transforms.GetComponent(ambientVehicles_[i].entity);
			if (transform == nullptr) continue;
			transform->ClearTransform();
			transform->Scale(XMFLOAT3(scale, scale, scale));
			transform->Rotate(XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), static_cast<float>(item.yawRadians)));
			transform->Translate(XMFLOAT3(x, static_cast<float>(GroundHeight(x, z)) + kRoadSurfaceY, z));
			transform->UpdateTransform();
		}
		return true;
	}

	bool NativeEconomyScene::SyncVehiclesOnly(wi::scene::Scene& scene,
		const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog,
		const runtime::NativeEconomyTransform& transform, std::string& error)
	{
		error.clear();
		if (!std::isfinite(transform.centerCellX) || !std::isfinite(transform.centerCellZ))
		{
			error = "imported region vehicle transform is not finite";
			return false;
		}
		std::unordered_set<economy::VehicleId> seen;
		seen.reserve(snapshot.vehicles.size());
		for (const auto& vehicle : snapshot.vehicles)
		{
			if (vehicle.id == 0 || !seen.insert(vehicle.id).second || catalog.VehicleType(vehicle.type) == nullptr)
			{
				error = "imported region snapshot has an invalid or duplicate vehicle";
				return false;
			}
		}
		if (!snapshot.vehicles.empty() && truckMesh_ == wi::ecs::INVALID_ENTITY)
		{
			truckMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic region truck mesh",
				openrepublic::procgeo::MakeCargoTruckBody(5), 0.5f, 0.1f);
			wheelMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic region truck wheel mesh",
				openrepublic::procgeo::MakeVehicleWheel(), 0.82f, 0.0f);
			cargoMesh_ = CreateProceduralMeshEntity(scene, "OpenRepublic region truck cargo mesh",
				openrepublic::procgeo::MakeCoalHeap(), 0.09f, 0.0f);
			if (truckMesh_ == wi::ecs::INVALID_ENTITY || wheelMesh_ == wi::ecs::INVALID_ENTITY ||
				cargoMesh_ == wi::ecs::INVALID_ENTITY)
			{
				if (truckMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(truckMesh_);
				if (wheelMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(wheelMesh_);
				if (cargoMesh_ != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(cargoMesh_);
				truckMesh_ = wheelMesh_ = cargoMesh_ = wi::ecs::INVALID_ENTITY;
				error = "Wicked Engine could not create imported region vehicle meshes";
				return false;
			}
		}
		for (const auto& vehicle : snapshot.vehicles)
		{
			VehicleVisual visual{};
			const auto old = vehicles_.find(vehicle.id);
			if (old != vehicles_.end()) visual = old->second;
			else
			{
				visual.body = CreateVehicleNode(scene, truckMesh_, "OpenRepublic region truck " + std::to_string(vehicle.id));
				visual.load = CreateVehicleNode(scene, cargoMesh_, "OpenRepublic region truck cargo " + std::to_string(vehicle.id));
				visual.labelMesh = CreateProceduralMeshEntity(scene, "OpenRepublic region truck ID mesh",
					openrepublic::procgeo::MakeVehicleIdLabel(vehicle.id), 1.0f, 0.0f);
				visual.label = CreateVehicleNode(scene, visual.labelMesh,
					"OpenRepublic region truck ID " + std::to_string(vehicle.id));
				for (wi::ecs::Entity& wheel : visual.wheels)
					wheel = CreateVehicleNode(scene, wheelMesh_, "OpenRepublic region truck wheel " + std::to_string(vehicle.id));
				if (visual.body == wi::ecs::INVALID_ENTITY || visual.load == wi::ecs::INVALID_ENTITY ||
					visual.labelMesh == wi::ecs::INVALID_ENTITY || visual.label == wi::ecs::INVALID_ENTITY ||
					std::any_of(visual.wheels.begin(), visual.wheels.end(),
						[](wi::ecs::Entity entity) { return entity == wi::ecs::INVALID_ENTITY; }))
				{
					for (wi::ecs::Entity entity : { visual.body, visual.load, visual.label, visual.labelMesh })
						if (entity != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(entity);
					for (wi::ecs::Entity entity : visual.wheels)
						if (entity != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(entity);
					error = "Wicked Engine could not create imported region vehicle nodes";
					return false;
				}
			}
			visual.wheelRotationBase = visual.wheelRotation;
			visual.previous = visual.hasPrevious ? visual.current : vehicle.cell;
			visual.current = vehicle.cell;
			visual.hasPrevious = true;
			visual.loaded = vehicle.cargo > 0;
			visual.idle = vehicle.ownerDepot != 0 ? vehicle.order == 0 :
				(vehicle.state == economy::HaulState::Idle || vehicle.state == economy::HaulState::Waiting ||
				 vehicle.state == economy::HaulState::NoRoute);
			vehicles_[vehicle.id] = visual;
		}
		for (auto it = vehicles_.begin(); it != vehicles_.end();)
		{
			if (seen.find(it->first) == seen.end())
			{
				const VehicleVisual& visual = it->second;
				scene.Entity_Remove(visual.body); scene.Entity_Remove(visual.load);
				scene.Entity_Remove(visual.label); scene.Entity_Remove(visual.labelMesh);
				for (wi::ecs::Entity wheel : visual.wheels) scene.Entity_Remove(wheel);
				it = vehicles_.erase(it);
			}
			else ++it;
		}
		transform_ = transform;
		error.clear();
		return true;
	}

	void NativeEconomyScene::UpdateVehicleMotion(wi::scene::Scene& scene, double alpha,
		const std::function<double(double, double)>& groundHeight)
	{
		if (!transform_) return;
		std::vector<runtime::TrafficIntersectionSample> trafficSamples;
		std::vector<runtime::TrafficFollowingSample> followingSamples;
		trafficSamples.reserve(vehicles_.size());
		followingSamples.reserve(vehicles_.size());
		for (const auto& entry : vehicles_)
		{
			const auto sample = entry.second.kind == openrepublic::procgeo::VehicleKind::Locomotive
				? runtime::InterpolateNativeRailMotion(entry.second.previous, entry.second.current, alpha,
					entry.second.yaw, *transform_)
				: runtime::InterpolateNativeVehicleMotion(entry.second.previous,
					entry.second.current, alpha, entry.second.yaw, *transform_);
			trafficSamples.push_back({ entry.first, sample.position });
			const auto a = entry.second.previous;
			const auto b = entry.second.current;
			const int dx = b.x - a.x, dz = b.z - a.z;
			if (std::abs(dx) + std::abs(dz) != 1) continue;
			const auto packed = [](economy::Cell cell) {
				return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.x)) << 32) |
					static_cast<std::uint32_t>(cell.z);
			};
			std::uint64_t pa = packed(a), pb = packed(b);
			int direction = 1;
			if (pb < pa) { std::swap(pa, pb); direction = -1; }
			const std::uint64_t segmentId = (pa * 0x9E3779B97F4A7C15ull) ^ pb;
			const runtime::NativePoint start = transform_->CellCenter(direction > 0 ? a : b);
			const runtime::NativePoint end = transform_->CellCenter(direction > 0 ? b : a);
			const double segmentLength = std::hypot(end.x - start.x, end.z - start.z);
			const double distance = std::clamp(std::hypot(sample.position.x - start.x, sample.position.z - start.z), 0.0, segmentLength);
			const float scale = static_cast<float>(runtime::NativeVehicleDisplayScale(
				openrepublic::procgeo::VehicleLength(entry.second.kind)));
			followingSamples.push_back({ entry.first, segmentId, 1, distance, 4.0,
				std::max(0.5, 3.0 * static_cast<double>(scale)) });
		}
		const auto following = runtime::ResolveTrafficFollowing(followingSamples, 0.25, 1.0);
		std::unordered_map<economy::VehicleId, runtime::TrafficFollowingResult> followingById;
		for (const auto& result : following) followingById.emplace(result.vehicleId, result);
		for (auto& entry : vehicles_)
		{
			VehicleVisual& visual = entry.second;
			const float modelScale = static_cast<float>(runtime::NativeVehicleDisplayScale(
				openrepublic::procgeo::VehicleLength(visual.kind)));
			const runtime::NativeVehicleMotion motion = visual.kind == openrepublic::procgeo::VehicleKind::Locomotive
				? runtime::InterpolateNativeRailMotion(visual.previous, visual.current, alpha, visual.yaw, *transform_, 0.48 * modelScale)
				: runtime::InterpolateNativeVehicleMotion(visual.previous, visual.current, alpha, visual.yaw, *transform_, 0.48 * modelScale);
			runtime::NativePoint polishedPosition = motion.position;
			double polishedYaw = motion.yawRadians;
			if (trafficPolishEnabled_ && motion.moving)
			{
				const runtime::NativePoint tangent{ std::sin(motion.yawRadians), std::cos(motion.yawRadians) };
				polishedPosition = runtime::OffsetToRightLane(motion.position, tangent, 0.12);
				const auto queued = followingById.find(entry.first);
				if (queued != followingById.end())
				{
					const auto a = visual.previous;
					const auto b = visual.current;
					const auto start = transform_->CellCenter((b.x < a.x || b.z < a.z) ? b : a);
					const double currentDistance = std::hypot(motion.position.x - start.x, motion.position.z - start.z);
					const double delta = queued->second.queuedPosition - currentDistance;
					polishedPosition.x += tangent.x * delta;
					polishedPosition.z += tangent.z * delta;
				}
				polishedYaw = runtime::LimitTrafficHeading(visual.yaw, motion.yawRadians, 0.20);
				const auto yielding = runtime::ResolveTrafficIntersectionYield(entry.first, motion.position,
					trafficSamples, 0.35, 3);
				if (yielding.shouldWait)
				{
					polishedPosition = transform_->CellCenter(visual.previous);
					polishedYaw = visual.yaw;
				}
			}
			if (motion.moving) visual.yaw = static_cast<float>(polishedYaw);
			visual.wheelRotation = visual.wheelRotationBase + static_cast<float>(motion.wheelRotationRadians);
			XMFLOAT4 yawRotation;
			XMStoreFloat4(&yawRotation, XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), visual.yaw));
			const double sampledHeight = groundHeight ? groundHeight(polishedPosition.x, polishedPosition.z) :
				GroundHeight(polishedPosition.x, polishedPosition.z);
			const float groundY = static_cast<float>(std::isfinite(sampledHeight) ? sampledHeight : 0.0) + kRoadSurfaceY;
			const auto positionNode = [&](wi::ecs::Entity entity, float localX, float localY, float localZ,
				const XMFLOAT3& scale, const XMFLOAT4& rotation)
			{
				wi::scene::TransformComponent* node = scene.transforms.GetComponent(entity);
				if (!node) return;
				const float x = static_cast<float>(polishedPosition.x) +
					(std::cos(visual.yaw) * localX + std::sin(visual.yaw) * localZ) * modelScale;
				const float z = static_cast<float>(polishedPosition.z) +
					(-std::sin(visual.yaw) * localX + std::cos(visual.yaw) * localZ) * modelScale;
				node->ClearTransform(); node->Scale(scale); node->Rotate(rotation);
				node->Translate(XMFLOAT3(x, groundY + localY * modelScale, z)); node->UpdateTransform();
			};
			positionNode(visual.body, 0, 0, 0, XMFLOAT3(modelScale, modelScale, modelScale), yawRotation);
			for (std::size_t wagon = 0; wagon < visual.wagons.size(); ++wagon)
				if (visual.wagons[wagon] != wi::ecs::INVALID_ENTITY)
					positionNode(visual.wagons[wagon], 0, 0, -4.5f - 4.0f * static_cast<float>(wagon),
						XMFLOAT3(modelScale, modelScale, modelScale), yawRotation);
			float frontZ = 3.78f, rearZ = -3.78f, lampY = 0.96f;
			if (visual.kind == openrepublic::procgeo::VehicleKind::Car) { frontZ = 1.89f; rearZ = -1.89f; lampY = 0.75f; }
			else if (visual.kind == openrepublic::procgeo::VehicleKind::Bus) { frontZ = 3.67f; rearZ = -3.67f; lampY = 0.94f; }
			else if (visual.kind == openrepublic::procgeo::VehicleKind::Locomotive) { frontZ = 2.57f; rearZ = -2.32f; lampY = 1.45f; }
			positionNode(visual.headlights, 0, lampY, frontZ, XMFLOAT3(modelScale, modelScale, modelScale), yawRotation);
			positionNode(visual.brakeLights, 0, lampY, rearZ, XMFLOAT3(modelScale, modelScale, modelScale), yawRotation);
			positionNode(visual.exhaust, 0.22f, 1.42f, rearZ - 0.28f, XMFLOAT3(modelScale, modelScale, modelScale), yawRotation);
			if (wi::scene::ObjectComponent* particle = scene.objects.GetComponent(visual.exhaust)) particle->SetRenderable(motion.moving);
			if (wi::scene::ObjectComponent* lights = scene.objects.GetComponent(visual.headlights)) lights->SetRenderable(darkLighting_);
			if (wi::scene::ObjectComponent* lights = scene.objects.GetComponent(visual.brakeLights))
				lights->color = visual.idle ? XMFLOAT4(1.0f, 0.22f, 0.14f, 1.0f) : XMFLOAT4(0.70f, 0.25f, 0.20f, 1.0f);
			const float loadY = visual.loaded ? 1.55f : -10000.0f;
			positionNode(visual.load, 0, loadY, -1.55f,
				XMFLOAT3(modelScale, modelScale, modelScale), yawRotation);
			if (wi::scene::TransformComponent* label = scene.transforms.GetComponent(visual.label))
			{
				label->ClearTransform(); label->Scale(XMFLOAT3(0.82f, 0.82f, 0.82f));
				label->Translate(XMFLOAT3(static_cast<float>(polishedPosition.x),
					groundY + 5.0f * modelScale, static_cast<float>(polishedPosition.z)));
				label->UpdateTransform();
			}
			if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(visual.body))
				object->color = visual.idle ? XMFLOAT4(0.90f, 0.66f, 0.64f, 1.0f) : XMFLOAT4(1, 1, 1, 1);
			float halfTrack = 1.12f, front = 3.05f, rear = -2.62f, radius = 0.48f;
			if (visual.kind == openrepublic::procgeo::VehicleKind::Car) { halfTrack = 0.79f; front = 1.22f; rear = -1.25f; radius = 0.34f; }
			else if (visual.kind == openrepublic::procgeo::VehicleKind::Bus) { halfTrack = 1.06f; front = 2.45f; rear = -2.42f; radius = 0.46f; }
			else if (visual.kind == openrepublic::procgeo::VehicleKind::Locomotive) { halfTrack = 0.96f; front = 1.25f; rear = -1.35f; radius = 0.52f; }
			else if (visual.kind == openrepublic::procgeo::VehicleKind::TrainWagon) { halfTrack = 1.10f; front = 1.46f; rear = -1.46f; radius = 0.50f; }
			const float wheelOffsets[4][2] = {{-halfTrack, front}, {halfTrack, front}, {-halfTrack, rear}, {halfTrack, rear}};
			XMFLOAT4 rollRotation;
			XMStoreFloat4(&rollRotation, XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), visual.wheelRotation));
			XMFLOAT4 combined;
			XMStoreFloat4(&combined, XMQuaternionMultiply(XMLoadFloat4(&yawRotation), XMLoadFloat4(&rollRotation)));
			for (std::size_t i = 0; i < visual.wheels.size(); ++i)
			{
				if (visual.kind == openrepublic::procgeo::VehicleKind::Locomotive)
				{
					if (wi::scene::ObjectComponent* wheel = scene.objects.GetComponent(visual.wheels[i])) wheel->SetRenderable(false);
					continue;
				}
				positionNode(visual.wheels[i], wheelOffsets[i][0], radius, wheelOffsets[i][1],
					XMFLOAT3(modelScale, modelScale, modelScale), combined);
			}
		}
	}

	std::optional<economy::BuildingId> NativeEconomyScene::BuildingForEntity(wi::ecs::Entity entity) const
	{
		const auto found = buildingByEntity_.find(entity);
		return found == buildingByEntity_.end() ? std::nullopt : std::optional<economy::BuildingId>(found->second);
	}

	DirectX::XMFLOAT3 NativeEconomyScene::CellPosition(economy::Cell cell) const
	{
		if (!transform_) return DirectX::XMFLOAT3(0, 0, 0);
		const runtime::NativePoint point = transform_->CellCenter(cell);
		float x, z;
		if (!ToFloat(point.x, x) || !ToFloat(point.z, z)) return DirectX::XMFLOAT3(0, 0, 0);
		return DirectX::XMFLOAT3(x, static_cast<float>(GroundHeight(x, z)) + 0.15f, z);
	}

	std::optional<DirectX::XMFLOAT3> NativeEconomyScene::VehiclePosition(economy::VehicleId id, double alpha) const
	{
		if (!transform_) return std::nullopt;
		const auto found = vehicles_.find(id);
		if (found == vehicles_.end()) return std::nullopt;
		const VehicleVisual& visual = found->second;
		const runtime::NativeVehicleMotion motion = visual.kind == openrepublic::procgeo::VehicleKind::Locomotive
			? runtime::InterpolateNativeRailMotion(visual.previous, visual.current, alpha, visual.yaw, *transform_)
			: runtime::InterpolateNativeVehicleMotion(visual.previous, visual.current, alpha, visual.yaw, *transform_);
		float x = 0.0f, z = 0.0f;
		if (!ToFloat(motion.position.x, x) || !ToFloat(motion.position.z, z)) return std::nullopt;
		return DirectX::XMFLOAT3(x, static_cast<float>(GroundHeight(x, z)) + 0.15f, z);
	}

	double NativeEconomyScene::GroundHeight(double worldX, double worldZ) const noexcept
	{
		return terrainEnabled_ ? terrain_.HeightAt(worldX, worldZ) : 0.0;
	}

	bool NativeEconomyScene::CanPlaceFootprint(economy::Cell cell, std::uint32_t width,
		std::uint32_t depth) const noexcept
	{
		if (!terrainEnabled_) return true;
		if (!transform_) return false;
		const runtime::NativePoint center = transform_->FootprintCenter(cell, width, depth);
		return terrain_.IsBuildable(center.x, center.z, width * runtime::NativeEconomyTransform::DisplayUnitsPerCell,
			depth * runtime::NativeEconomyTransform::DisplayUnitsPerCell);
	}

	float NativeEconomyScene::CameraDistance() const noexcept { return cameraDistance_; }
	bool NativeEconomyScene::CellFromGroundPoint(double worldX, double worldZ, std::uint32_t gridWidth,
		std::uint32_t gridDepth, economy::Cell& outCell) const noexcept
	{
		return transform_ && runtime::NativeDisplayPointToCell(*transform_, worldX, worldZ,
			gridWidth, gridDepth, outCell);
	}
	DirectX::XMFLOAT3 NativeEconomyScene::CameraTarget() const noexcept { return cameraTarget_; }
}
