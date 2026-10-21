#pragma once

#include "WickedEngine.h"

#include <memory>

namespace openrepublic
{
	// Owns the presentation path and its view-only demonstration scene.
	// Authoritative simulation state will live outside this class.
	class OpenRepublicApplication final : public wi::Application
	{
	public:
		void Initialize() override;
		void Update(float dt) override;

	private:
		void SelectPrototypeBuilding(bool selected);
		void PickAtPointer();
		const char* GetOpenRepublicId(wi::ecs::Entity entity) const;

		wi::scene::Scene previewWorld;
		std::unique_ptr<wi::RenderPath3D> renderPath;
		wi::ecs::Entity prototypeBuilding = wi::ecs::INVALID_ENTITY;
	};
}
