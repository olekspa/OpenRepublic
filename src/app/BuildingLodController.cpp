#include "BuildingLodController.h"

#include <algorithm>
#include <cmath>

BuildingLodController::BuildingLodController(float nearDistance, float farDistance)
{
	Configure(nearDistance, farDistance);
}

bool BuildingLodController::Configure(float nearDistance, float farDistance)
{
	if (!std::isfinite(nearDistance) || !std::isfinite(farDistance) || nearDistance <= 0.0f ||
		farDistance <= nearDistance)
	{
		return false;
	}
	nearDistance_ = nearDistance;
	farDistance_ = farDistance;
	return true;
}

void BuildingLodController::Clear()
{
	instances_.clear();
}

void BuildingLodController::AddInstance(wi::ecs::Entity instance, const MeshVariants& meshes,
	int maximumLod, const DirectX::XMFLOAT3& position)
{
	if (instance == wi::ecs::INVALID_ENTITY || maximumLod < 0 || maximumLod > 2 ||
		!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
	{
		return;
	}
	Entry entry;
	entry.instance = instance;
	entry.meshes = meshes;
	entry.position = position;
	entry.maximumLod = maximumLod;
	instances_.push_back(entry);
}

int BuildingLodController::AvailableLod(const Entry& entry, int requestedLod)
{
	const int capped = std::min(std::max(requestedLod, 0), entry.maximumLod);
	for (int lod = capped; lod >= 0; --lod)
	{
		if (entry.meshes[static_cast<std::size_t>(lod)] != wi::ecs::INVALID_ENTITY)
		{
			return lod;
		}
	}
	// LOD0 is the highest-detail tier and has no lower numeric fallback. If it
	// is absent, walk toward coarser tiers so dynamic mode still has a valid mesh.
	for (int lod = capped + 1; lod <= entry.maximumLod; ++lod)
	{
		if (entry.meshes[static_cast<std::size_t>(lod)] != wi::ecs::INVALID_ENTITY)
		{
			return lod;
		}
	}
	return -1;
}

int BuildingLodController::SelectLod(const Entry& entry, float distance) const
{
	constexpr float kHysteresisFraction = 0.08f;
	const float nearExit = nearDistance_ * (1.0f - kHysteresisFraction);
	const float farExit = farDistance_ * (1.0f - kHysteresisFraction);
	int desired = 0;
	if (entry.currentLod == 0)
	{
		desired = distance >= nearDistance_ ? 1 : 0;
		if (distance >= farDistance_) desired = 2;
	}
	else if (entry.currentLod == 1)
	{
		desired = distance < nearExit ? 0 : (distance >= farDistance_ ? 2 : 1);
	}
	else if (entry.currentLod == 2)
	{
		desired = distance < farExit ? (distance < nearExit ? 0 : 1) : 2;
	}
	else
	{
		desired = distance < nearDistance_ ? 0 : (distance < farDistance_ ? 1 : 2);
	}
	return AvailableLod(entry, desired);
}

void BuildingLodController::Update(wi::scene::Scene& scene, const DirectX::XMFLOAT3& cameraPosition)
{
	if (!std::isfinite(cameraPosition.x) || !std::isfinite(cameraPosition.y) || !std::isfinite(cameraPosition.z))
	{
		return;
	}
	for (Entry& entry : instances_)
	{
		wi::scene::ObjectComponent* object = scene.objects.GetComponent(entry.instance);
		if (object == nullptr)
		{
			continue;
		}
		const float dx = entry.position.x - cameraPosition.x;
		const float dy = entry.position.y - cameraPosition.y;
		const float dz = entry.position.z - cameraPosition.z;
		const int selected = SelectLod(entry, std::sqrt(dx * dx + dy * dy + dz * dz));
		if (selected < 0)
		{
			continue;
		}
		const wi::ecs::Entity mesh = entry.meshes[static_cast<std::size_t>(selected)];
		if (object->meshID != mesh)
		{
			object->meshID = mesh;
		}
		entry.currentLod = selected;
	}
}
