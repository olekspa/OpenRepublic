#pragma once

#include "WickedEngine.h"

#include <array>
#include <cstddef>
#include <vector>

// Per-frame presentation-only LOD selection for map-region building instances.
// It owns no scene entities; all mesh and instance IDs belong to the Scene.
class BuildingLodController
{
public:
	using MeshVariants = std::array<wi::ecs::Entity, 3>;

	explicit BuildingLodController(float nearDistance = 250.0f, float farDistance = 700.0f);

	bool Configure(float nearDistance, float farDistance);
	void Clear();
	void AddInstance(wi::ecs::Entity instance, const MeshVariants& meshes,
		int maximumLod, const DirectX::XMFLOAT3& position);
	void Update(wi::scene::Scene& scene, const DirectX::XMFLOAT3& cameraPosition);

	std::size_t InstanceCount() const { return instances_.size(); }
	float NearDistance() const { return nearDistance_; }
	float FarDistance() const { return farDistance_; }

private:
	struct Entry
	{
		wi::ecs::Entity instance = wi::ecs::INVALID_ENTITY;
		MeshVariants meshes{ wi::ecs::INVALID_ENTITY, wi::ecs::INVALID_ENTITY, wi::ecs::INVALID_ENTITY };
		DirectX::XMFLOAT3 position{};
		int maximumLod = 0;
		int currentLod = -1;
	};

	int SelectLod(const Entry& entry, float distance) const;
	static int AvailableLod(const Entry& entry, int requestedLod);

	std::vector<Entry> instances_;
	float nearDistance_ = 250.0f;
	float farDistance_ = 700.0f;
};
