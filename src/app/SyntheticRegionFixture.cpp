#include "SyntheticRegionFixture.h"

#include <iterator>
#include <utility>

using namespace DirectX;

namespace openrepublic::preview
{
	namespace
	{
		bool ConfigureCube(wi::scene::Scene& scene, wi::ecs::Entity entity,
			const XMFLOAT3& scale, const XMFLOAT3& position, const XMFLOAT4& color,
			std::string& error)
		{
			wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
			wi::scene::MaterialComponent* material = scene.materials.GetComponent(entity);
			wi::scene::ObjectComponent* object = scene.objects.GetComponent(entity);
			if (transform == nullptr || material == nullptr || object == nullptr)
			{
				error = "Wicked Engine did not create every component for a synthetic fixture object.";
				return false;
			}

			transform->Scale(scale);
			transform->Translate(position);
			transform->UpdateTransform();
			material->SetBaseColor(color);
			material->SetRoughness(0.92f);
			material->SetReflectance(0.02f);
			material->CreateRenderData(true);
			return true;
		}

		wi::ecs::Entity CreateConfiguredCube(wi::scene::Scene& scene, const char* name,
			const XMFLOAT3& scale, const XMFLOAT3& position, const XMFLOAT4& color,
			std::string& error)
		{
			const wi::ecs::Entity entity = scene.Entity_CreateCube(name);
			if (entity == wi::ecs::INVALID_ENTITY ||
				!ConfigureCube(scene, entity, scale, position, color, error))
			{
				return wi::ecs::INVALID_ENTITY;
			}
			return entity;
		}
	}

	bool CreateSyntheticRegionFixture(wi::scene::Scene& scene,
		SyntheticRegionFixtureData& output, std::string& error)
	{
		SyntheticRegionFixtureData next;
		next.placements = {
			{ "farm", -12.0, 0.0, 0 },
			{ "mill", 0.0, 0.0, 1 },
			{ "bakery", 12.0, 0.0, 2 },
		};
		next.roads = { { -20.0, -4.0, 20.0, -4.0 } };
		next.description =
			"OpenRepublic synthetic/open fixture: Farm, Mill and Bakery beside one connected road; no WRSR inputs.";

		const wi::ecs::Entity ground = scene.Entity_CreatePlane("OpenRepublic synthetic fixture ground");
		if (ground == wi::ecs::INVALID_ENTITY)
		{
			error = "Wicked Engine could not create the synthetic fixture ground.";
			return false;
		}
		if (wi::scene::TransformComponent* transform = scene.transforms.GetComponent(ground))
		{
			transform->Scale(XMFLOAT3(30.0f, 1.0f, 12.0f));
			transform->Translate(XMFLOAT3(0.0f, -0.15f, -1.0f));
			transform->UpdateTransform();
		}
		else
		{
			error = "Wicked Engine did not create a transform for the synthetic fixture ground.";
			return false;
		}
		if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(ground))
		{
			material->SetBaseColor(XMFLOAT4(0.20f, 0.27f, 0.20f, 1.0f));
			material->SetRoughness(1.0f);
			material->SetReflectance(0.0f);
			material->CreateRenderData(true);
		}
		else
		{
			error = "Wicked Engine did not create a material for the synthetic fixture ground.";
			return false;
		}

		// The economy grid uses 4 m cells. A broad, thin cube is adequate as an
		// OpenRepublic-only road surface and does not encode any WRSR road format.
		const wi::ecs::Entity road = CreateConfiguredCube(scene,
			"OpenRepublic synthetic fixture road",
			XMFLOAT3(21.0f, 0.08f, 0.85f), XMFLOAT3(0.0f, 0.02f, -4.0f),
			XMFLOAT4(0.12f, 0.13f, 0.15f, 1.0f), error);
		if (road == wi::ecs::INVALID_ENTITY)
		{
			if (error.empty()) error = "Wicked Engine could not create the synthetic fixture road.";
			return false;
		}

		struct BuildingVisual
		{
			const char* type;
			const char* name;
			float originX;
			XMFLOAT3 scale;
			XMFLOAT4 color;
		};
		const BuildingVisual buildings[] = {
			{ "farm", "OpenRepublic synthetic fixture Farm", -12.0f,
				XMFLOAT3(3.8f, 2.1f, 3.5f), XMFLOAT4(0.62f, 0.53f, 0.35f, 1.0f) },
			{ "mill", "OpenRepublic synthetic fixture Mill", 0.0f,
				XMFLOAT3(3.3f, 4.4f, 3.2f), XMFLOAT4(0.48f, 0.53f, 0.58f, 1.0f) },
			{ "bakery", "OpenRepublic synthetic fixture Bakery", 12.0f,
				XMFLOAT3(3.8f, 2.8f, 3.5f), XMFLOAT4(0.66f, 0.39f, 0.31f, 1.0f) },
		};
		next.instances.reserve(3);
		for (std::size_t i = 0; i < std::size(buildings); ++i)
		{
			const BuildingVisual& building = buildings[i];
			const float centerX = building.originX + 4.0f;
			const wi::ecs::Entity entity = CreateConfiguredCube(scene, building.name,
				building.scale,
				XMFLOAT3(centerX, building.scale.y * 0.5f, 0.0f), building.color, error);
			if (entity == wi::ecs::INVALID_ENTITY)
			{
				if (error.empty()) error = "Wicked Engine could not create a synthetic fixture building.";
				return false;
			}
			next.instances.push_back({ entity, building.type,
				static_cast<double>(building.originX), 0.0, static_cast<std::uint64_t>(i) });
		}

		output = std::move(next);
		error.clear();
		return true;
	}
}
