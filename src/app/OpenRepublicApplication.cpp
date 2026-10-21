#include "OpenRepublicApplication.h"

#include <windows.h>

namespace openrepublic
{
	namespace
	{
		constexpr char kPrototypeBuildingId[] = "OR-0001";
		constexpr XMFLOAT4 kBuildingColor(0.72f, 0.43f, 0.20f, 1.0f);
		constexpr XMFLOAT4 kSelectedBuildingColor(1.0f, 0.78f, 0.20f, 1.0f);
		constexpr float kFlySpeed = 4.0f;
		constexpr float kWheelZoomSpeed = 0.75f;
	}

	void OpenRepublicApplication::Initialize()
	{
		wi::Application::Initialize();

		// Synthetic geometry only: this smoke scene has no WRSR content or data.
		previewWorld.Entity_CreatePlane("OpenRepublic ground");
		prototypeBuilding = previewWorld.Entity_CreateCube("OpenRepublic prototype building");
		if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(prototypeBuilding))
		{
			transform->Translate(XMFLOAT3(0, 0.75f, 0));
			transform->Scale(XMFLOAT3(0.75f, 0.75f, 0.75f));
			transform->UpdateTransform();
		}
		if (wi::scene::MaterialComponent* material = previewWorld.materials.GetComponent(prototypeBuilding))
		{
			material->SetBaseColor(kBuildingColor);
		}

		const wi::ecs::Entity sun = previewWorld.Entity_CreateLight(
			"OpenRepublic preview light",
			XMFLOAT3(0, 0, 0),
			XMFLOAT3(1.0f, 0.88f, 0.72f),
			4.0f,
			100.0f,
			wi::scene::LightComponent::DIRECTIONAL
		);
		if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(sun))
		{
			transform->RotateRollPitchYaw(XMFLOAT3(-0.7f, 0.6f, 0));
			transform->UpdateTransform();
		}

		renderPath = std::make_unique<wi::RenderPath3D>();
		renderPath->scene = &previewWorld;
		renderPath->setBloomEnabled(false);
		renderPath->setDepthOfFieldEnabled(false);

		wi::scene::TransformComponent cameraTransform;
		cameraTransform.Translate(XMFLOAT3(0, 2.0f, -5.0f));
		cameraTransform.RotateRollPitchYaw(XMFLOAT3(0.18f, 0, 0));
		cameraTransform.UpdateTransform();
		renderPath->camera->TransformCamera(cameraTransform);
		renderPath->camera->UpdateCamera();

		ActivatePath(renderPath.get());
	}

	void OpenRepublicApplication::Update(float dt)
	{
		wi::Application::Update(dt);

		if (!is_window_active || renderPath == nullptr || renderPath->camera == nullptr)
		{
			return;
		}

		if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
		{
			SelectPrototypeBuilding(false);
		}

		if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
		{
			PickAtPointer();
		}

		wi::scene::CameraComponent& camera = *renderPath->camera;
		XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&camera.At));
		XMVECTOR up = XMLoadFloat3(&camera.Up);
		XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
		XMVECTOR movement = XMVectorZero();

		if (wi::input::Down(static_cast<wi::input::BUTTON>('W')) || wi::input::Down(wi::input::KEYBOARD_BUTTON_UP))
		{
			movement = XMVectorAdd(movement, forward);
		}
		if (wi::input::Down(static_cast<wi::input::BUTTON>('S')) || wi::input::Down(wi::input::KEYBOARD_BUTTON_DOWN))
		{
			movement = XMVectorSubtract(movement, forward);
		}
		if (wi::input::Down(static_cast<wi::input::BUTTON>('D')) || wi::input::Down(wi::input::KEYBOARD_BUTTON_RIGHT))
		{
			movement = XMVectorAdd(movement, right);
		}
		if (wi::input::Down(static_cast<wi::input::BUTTON>('A')) || wi::input::Down(wi::input::KEYBOARD_BUTTON_LEFT))
		{
			movement = XMVectorSubtract(movement, right);
		}
		if (wi::input::Down(wi::input::KEYBOARD_BUTTON_SPACE))
		{
			movement = XMVectorAdd(movement, XMVectorSet(0, 1, 0, 0));
		}
		if (wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL))
		{
			movement = XMVectorSubtract(movement, XMVectorSet(0, 1, 0, 0));
		}

		const XMFLOAT4 pointer = wi::input::GetPointer();
		bool cameraMoved = false;
		if (XMVectorGetX(XMVector3LengthSq(movement)) > 0)
		{
			movement = XMVector3Normalize(movement);
			const XMVECTOR eye = XMLoadFloat3(&camera.Eye);
			XMStoreFloat3(&camera.Eye, XMVectorAdd(eye, XMVectorScale(movement, kFlySpeed * dt)));
			cameraMoved = true;
		}
		if (pointer.z != 0)
		{
			const XMVECTOR eye = XMLoadFloat3(&camera.Eye);
			XMStoreFloat3(&camera.Eye, XMVectorAdd(eye, XMVectorScale(forward, pointer.z * kWheelZoomSpeed)));
			cameraMoved = true;
		}
		if (cameraMoved)
		{
			camera.UpdateCamera();
		}
	}

	const char* OpenRepublicApplication::GetOpenRepublicId(wi::ecs::Entity entity) const
	{
		return entity == prototypeBuilding ? kPrototypeBuildingId : nullptr;
	}

	void OpenRepublicApplication::SelectPrototypeBuilding(bool selected)
	{
		if (wi::scene::MaterialComponent* material = previewWorld.materials.GetComponent(prototypeBuilding))
		{
			material->SetBaseColor(selected ? kSelectedBuildingColor : kBuildingColor);
		}

		if (window != nullptr)
		{
			SetWindowTextW(window, selected
				? L"OpenRepublic | Selected OR-0001 | WASD/Arrows move, wheel zoom, Esc clear"
				: L"OpenRepublic | WASD/Arrows move, Space/Ctrl rise/lower, wheel zoom, click select, Esc clear");
		}
	}

	void OpenRepublicApplication::PickAtPointer()
	{
		if (renderPath == nullptr || renderPath->camera == nullptr)
		{
			return;
		}

		const XMFLOAT4 pointer = wi::input::GetPointer();
		if (pointer.x < 0 || pointer.y < 0 || pointer.x >= renderPath->GetLogicalWidth() || pointer.y >= renderPath->GetLogicalHeight())
		{
			return;
		}

		const wi::primitive::Ray ray = wi::renderer::GetPickRay(
			static_cast<long>(pointer.x),
			static_cast<long>(pointer.y),
			*renderPath,
			*renderPath->camera
		);
		const wi::scene::Scene::RayIntersectionResult hit = previewWorld.Intersects(ray);
		const char* openRepublicId = GetOpenRepublicId(hit.entity);
		SelectPrototypeBuilding(openRepublicId != nullptr);
	}
}
