#include "NativeEconomyScene.h"
#include "openrepublic/runtime/native_economy_tools.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace openrepublic::preview
{
	namespace
	{
		using Plan = runtime::NativeEconomyScenePlan;
		using BuildingPrimitive = runtime::NativeBuildingPrimitive;
		using RoadPrimitive = runtime::NativeRoadPrimitive;

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

		wi::ecs::Entity CreateBuilding(wi::scene::Scene& scene, const BuildingPrimitive& primitive)
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
			transform->Translate(XMFLOAT3(x, halfHeight, z));
			return entity;
		}

		wi::ecs::Entity CreateRoad(wi::scene::Scene& scene, const RoadPrimitive& primitive)
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
			transform->Translate(XMFLOAT3(x, roadHalfHeight, z));
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

		wi::ecs::Entity CreateGround(wi::scene::Scene& scene, const Plan& plan)
		{
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
		const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error)
	{
		error.clear();
		if (initialized_)
		{
			error = "native economy scene is already initialized";
			return false;
		}
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
		seenBuildings.reserve(plan.buildings.size());
		seenRoads.reserve(plan.roads.size());
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
			const wi::ecs::Entity entity = CreateBuilding(scene, primitive);
			if (entity == wi::ecs::INVALID_ENTITY)
			{
				rollback();
				error = "Wicked Engine could not create a native building primitive";
				return false;
			}
			createdEntities.push_back(entity);
			addedBuildings.push_back({ primitive.buildingId, { entity, primitive.typeId, primitive.centerX,
				primitive.centerZ, primitive.width, primitive.depth, primitive.height } });
		}
		for (const RoadPrimitive& primitive : plan.roads)
		{
			auto old = roads_.find(primitive.roadId);
			if (old != roads_.end() && sameRoad(old->second, primitive)) continue;
			const wi::ecs::Entity entity = CreateRoad(scene, primitive);
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
			const double centerY = std::max(a->second->height, b->second->height) + 0.25;
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
			newGround = CreateGround(scene, plan);
			if (newGround == wi::ecs::INVALID_ENTITY)
			{
				rollback();
				error = "Wicked Engine could not create the native grid ground";
				return false;
			}
			createdEntities.push_back(newGround);
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
		if (newGround != wi::ecs::INVALID_ENTITY) groundEntity_ = newGround;
		groundTransform->scale_local = XMFLOAT3(groundHalfWidth, 1.0f, groundHalfDepth);
		groundTransform->translation_local = XMFLOAT3(groundX, 0.0f, groundZ);
		groundTransform->UpdateTransform();

		outInstances = std::move(instances);
		cameraDistance_ = static_cast<float>(plan.cameraDistance);
		cameraTarget_ = DirectX::XMFLOAT3(
			static_cast<float>(plan.minX * 0.5 + plan.maxX * 0.5), 0.0f,
			static_cast<float>(plan.minZ * 0.5 + plan.maxZ * 0.5));
		initialized_ = true;
		return true;
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
		return DirectX::XMFLOAT3(x, 0.15f, z);
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
