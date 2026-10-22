#include "NativeEconomyTools.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace openrepublic::preview
{
	namespace
	{
		constexpr wi::Color kToolButtonIdle(36, 60, 80, 255);
		constexpr wi::Color kToolButtonFocus(52, 88, 116, 255);
		constexpr wi::Color kToolButtonActive(28, 46, 62, 255);
		constexpr wi::Color kToolComboIdle(30, 42, 54, 255);
		constexpr wi::Color kToolComboFocus(45, 65, 82, 255);
		constexpr wi::Color kToolComboActive(25, 36, 48, 255);
		constexpr wi::Color kToolDropdown(25, 34, 44, 250);
		constexpr wi::Color kToolText(242, 247, 255, 255);
		constexpr float kLeft = 18.0f;
		constexpr float kGap = 6.0f;
		constexpr float kControlHeight = 28.0f;
		constexpr float kRowGap = 6.0f;

		bool IsSelected(const wi::gui::ComboBox& combo) noexcept
		{
			return combo.GetSelected() >= 0;
		}

		template<typename TId>
		TId SelectedId(const wi::gui::ComboBox& combo, TId invalid) noexcept
		{
			if (!IsSelected(combo))
			{
				return invalid;
			}
			const std::uint64_t value = combo.GetSelectedUserdata();
			if (value > static_cast<std::uint64_t>(std::numeric_limits<TId>::max()))
			{
				return invalid;
			}
			return static_cast<TId>(value);
		}

		bool WidgetContains(const wi::gui::Widget& widget, XMFLOAT2 pointer) noexcept
		{
			if (!widget.IsVisible() || !widget.IsEnabled())
			{
				return false;
			}
			const XMFLOAT2 position = widget.GetPos();
			const XMFLOAT2 size = widget.GetSize();
			return pointer.x >= position.x && pointer.y >= position.y &&
				pointer.x <= position.x + size.x && pointer.y <= position.y + size.y;
		}
	}

	void NativeEconomyTools::StyleButton(wi::gui::Button& button)
	{
		button.SetColor(kToolButtonIdle, wi::gui::IDLE);
		button.SetColor(kToolButtonFocus, wi::gui::FOCUS);
		button.SetColor(kToolButtonActive, wi::gui::ACTIVE);
		button.SetColor(kToolButtonIdle, wi::gui::DEACTIVATING);
		button.font.params.color = kToolText;
	}

	void NativeEconomyTools::StyleCombo(wi::gui::ComboBox& combo)
	{
		combo.SetText("");
		combo.SetMaxVisibleItemCount(6);
		combo.SetColor(kToolComboIdle, wi::gui::IDLE);
		combo.SetColor(kToolComboFocus, wi::gui::FOCUS);
		combo.SetColor(kToolComboActive, wi::gui::ACTIVE);
		combo.SetColor(kToolComboIdle, wi::gui::DEACTIVATING);
		combo.SetColor(kToolDropdown, wi::gui::WIDGET_ID_COMBO_DROPDOWN);
		combo.font.params.color = kToolText;
		combo.selected_font.params.color = kToolText;
	}

	void NativeEconomyTools::RegisterWidgets(wi::gui::GUI& gui)
	{
		gui.AddWidget(&buildingType_);
		gui.AddWidget(&place_);
		gui.AddWidget(&road_);
		gui.AddWidget(&cancel_);
		gui.AddWidget(&remove_);
		gui.AddWidget(&resource_);
		gui.AddWidget(&vehicleType_);
		gui.AddWidget(&order_);
		gui.AddWidget(&retargetOrder_);
		gui.AddWidget(&cancelOrder_);
		gui.AddWidget(&roadChoice_);
		gui.AddWidget(&toggleRoad_);
		gui.AddWidget(&utilityResource_);
		gui.AddWidget(&connectUtility_);
		gui.AddWidget(&disconnectUtility_);
	}

	void NativeEconomyTools::Create(wi::gui::GUI& gui, const economy::Catalog& catalog, Callbacks callbacks,
		bool nativeConstruction)
	{
		if (gui_ != nullptr)
		{
			gui_->RemoveWidget(&buildingType_);
			gui_->RemoveWidget(&place_);
			gui_->RemoveWidget(&road_);
			gui_->RemoveWidget(&cancel_);
			gui_->RemoveWidget(&remove_);
			gui_->RemoveWidget(&resource_);
			gui_->RemoveWidget(&vehicleType_);
			gui_->RemoveWidget(&order_);
			gui_->RemoveWidget(&retargetOrder_);
			gui_->RemoveWidget(&cancelOrder_);
			gui_->RemoveWidget(&roadChoice_);
			gui_->RemoveWidget(&toggleRoad_);
			gui_->RemoveWidget(&utilityResource_);
			gui_->RemoveWidget(&connectUtility_);
			gui_->RemoveWidget(&disconnectUtility_);
		}

		gui_ = &gui;
		callbacks_ = std::move(callbacks);
		nativeConstruction_ = nativeConstruction;
		haulResources_.clear();
		haveHaulResources_ = false;
		haveOrderRefresh_ = false;
		haveRoadRefresh_ = false;
		hasLayout_ = false;
		hasUtilityResources_ = false;

		buildingType_.Create("NativeEconomyBuildingType");
		buildingType_.ClearItems();
		buildingType_.SetTooltip("Choose a building. Click its minimum X/Z footprint cell. Its access cell below must remain clear; roads connect deliveries.");
		buildingType_.SetInvalidSelectionText("No building types available");
		for (const economy::BuildingTypeDef& type : catalog.BuildingTypes())
		{
			buildingType_.AddItem(type.name + " (" + std::to_string(type.width) + "x" +
				std::to_string(type.depth) + ")", type.id);
		}
		StyleCombo(buildingType_);
		if (buildingType_.GetItemCount() != 0) buildingType_.SetSelectedWithoutCallback(0);

		place_.Create("NativeEconomyPlaceBuilding");
		place_.SetText("Place building");
		place_.SetTooltip("Place the selected native building type at the cursor.");
		StyleButton(place_);
		place_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.place) callbacks_.place();
		});

		road_.Create("NativeEconomyBuildRoad");
		road_.SetText("Build road");
		road_.SetTooltip("Enter native road placement mode.");
		StyleButton(road_);
		road_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.road) callbacks_.road();
		});

		cancel_.Create("NativeEconomyCancelTool");
		cancel_.SetText("Cancel");
		cancel_.SetTooltip("Cancel the active placement or road tool.");
		StyleButton(cancel_);
		cancel_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.cancel) callbacks_.cancel();
		});

		remove_.Create("NativeEconomyRemoveBuilding");
		remove_.SetText("Remove selected");
		remove_.SetTooltip("Demolish the selected building and discard its inventory. Buildings referenced by haul orders cannot be removed.");
		StyleButton(remove_);
		remove_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.remove) callbacks_.remove();
		});

		resource_.Create("NativeEconomyHaulResource");
		resource_.ClearItems();
		resource_.SetTooltip("Choose a resource both haul endpoints can store.");
		resource_.SetInvalidSelectionText("Choose haul endpoints");
		StyleCombo(resource_);

		vehicleType_.Create("NativeEconomyHaulVehicleType");
		vehicleType_.ClearItems();
		vehicleType_.SetTooltip("Choose the vehicle type for the haul order.");
		vehicleType_.SetInvalidSelectionText("No vehicle types available");
		for (const economy::VehicleTypeDef& type : catalog.VehicleTypes())
		{
			vehicleType_.AddItem(type.name, type.id);
		}
		StyleCombo(vehicleType_);
		if (vehicleType_.GetItemCount() != 0) vehicleType_.SetSelectedWithoutCallback(0);

		order_.Create("NativeEconomyHaulOrder");
		order_.ClearItems();
		order_.SetTooltip("Active haul orders. Loaded orders cannot be cancelled; their cargo is preserved.");
		order_.SetInvalidSelectionText("No active haul orders");
		StyleCombo(order_);

		retargetOrder_.Create("NativeEconomyRetargetHaulOrder");
		retargetOrder_.SetText("Retarget to selected");
		retargetOrder_.SetTooltip("Retarget the selected haul to the currently selected destination building.");
		StyleButton(retargetOrder_);
		retargetOrder_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.retargetOrder) callbacks_.retargetOrder();
		});

		cancelOrder_.Create("NativeEconomyCancelHaulOrder");
		cancelOrder_.SetText("Cancel haul");
		cancelOrder_.SetTooltip("Cancel the selected haul order. Cancellation is rejected while cargo is in transit, preserving that cargo.");
		StyleButton(cancelOrder_);
		cancelOrder_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.cancelOrder) callbacks_.cancelOrder();
		});

		roadChoice_.Create("NativeEconomyRoadChoice");
		roadChoice_.ClearItems();
		roadChoice_.SetTooltip("Choose a native road by ID and endpoints. Disabled roads remain listed.");
		roadChoice_.SetInvalidSelectionText("No native roads");
		StyleCombo(roadChoice_);

		toggleRoad_.Create("NativeEconomyToggleRoad");
		toggleRoad_.SetText("Toggle road");
		toggleRoad_.SetTooltip("Enable or disable the selected native road for routing.");
		StyleButton(toggleRoad_);
		toggleRoad_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.toggleRoad) callbacks_.toggleRoad();
		});

		utilityResource_.Create("NativeEconomyUtilityResource");
		utilityResource_.ClearItems();
		utilityResource_.SetTooltip("Choose an explicit utility resource to connect.");
		utilityResource_.SetInvalidSelectionText("No utility resources");
		for (const economy::ResourceDef& resource : catalog.Resources())
		{
			if (resource.utility)
			{
				utilityResource_.AddItem(resource.name, resource.id);
				hasUtilityResources_ = true;
			}
		}
		StyleCombo(utilityResource_);
		if (utilityResource_.GetItemCount() != 0)
			utilityResource_.SetSelectedWithoutCallback(0);

		connectUtility_.Create("NativeEconomyConnectUtility");
		connectUtility_.SetText("Connect utility");
		connectUtility_.SetTooltip("Connect the selected utility resource from the haul source to the selected destination. Set the haul source, then select a destination. This is an idealized connection with shared-grid supply and no cargo transport.");
		StyleButton(connectUtility_);
		connectUtility_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.connectUtility) callbacks_.connectUtility();
		});

		disconnectUtility_.Create("NativeEconomyDisconnectUtility");
		disconnectUtility_.SetText("Disconnect utility");
		disconnectUtility_.SetTooltip("Disconnect the selected utility resource from the selected destination. Set the haul source, then select a destination. This removes an idealized shared-grid connection; it moves no cargo.");
		StyleButton(disconnectUtility_);
		disconnectUtility_.OnClick([this](const wi::gui::EventArgs&) {
			if (callbacks_.disconnectUtility) callbacks_.disconnectUtility();
		});

		const std::array<wi::gui::Widget*, 3> utilityControls = {
			&utilityResource_, &connectUtility_, &disconnectUtility_
		};
		for (wi::gui::Widget* widget : utilityControls)
		{
			widget->SetVisible(hasUtilityResources_);
			widget->SetEnabled(hasUtilityResources_);
		}

		// The same toolbar can be recreated for a world that exposes logistics
		// without exposing construction. Reset both properties on every Create so
		// a previous mode cannot leak widget state into the new one.
		const std::array<wi::gui::Widget*, 7> constructionControls = {
			&buildingType_, &place_, &road_, &cancel_, &remove_, &roadChoice_, &toggleRoad_
		};
		for (wi::gui::Widget* widget : constructionControls)
		{
			widget->SetVisible(nativeConstruction_);
			widget->SetEnabled(nativeConstruction_);
		}
		const std::array<wi::gui::Widget*, 5> logisticsControls = {
			&resource_, &vehicleType_, &order_, &retargetOrder_, &cancelOrder_
		};
		for (wi::gui::Widget* widget : logisticsControls)
		{
			widget->SetVisible(true);
			widget->SetEnabled(true);
		}

		RegisterWidgets(gui);
	}

	float NativeEconomyTools::Layout(float viewportWidth, float startY)
	{
		if (!std::isfinite(viewportWidth)) viewportWidth = 0.0f;
		if (!std::isfinite(startY)) startY = 0.0f;
		viewportWidth = std::max(0.0f, viewportWidth);
		const float left = std::min(kLeft, viewportWidth * 0.5f);
		const float available = std::max(1.0f, viewportWidth - 2.0f * left);

		const auto layoutRow = [left, available](float y, const std::array<wi::gui::Widget*, 5>& widgets,
			const std::array<float, 5>& widths, std::size_t count) {
			float x = left;
			bool hasItem = false;
			for (std::size_t i = 0; i < count; ++i)
			{
				const float width = std::min(widths[i], available);
				if (hasItem && x + width > left + available)
				{
					y += kControlHeight + kRowGap;
					x = left;
					hasItem = false;
				}
				widgets[i]->SetPos(XMFLOAT2(x, y));
				widgets[i]->SetSize(XMFLOAT2(width, kControlHeight));
				x += width + kGap;
				hasItem = true;
			}
			return y + kControlHeight;
		};

		float y = startY;
		if (nativeConstruction_)
		{
			const std::array<wi::gui::Widget*, 5> constructionWidgets = {
				&buildingType_, &place_, &road_, &cancel_, &remove_
			};
			const std::array<float, 5> constructionWidths = { 180.0f, 120.0f, 110.0f, 100.0f, 140.0f };
			y = layoutRow(y, constructionWidgets, constructionWidths, constructionWidgets.size()) + kRowGap;
		}

		const std::array<wi::gui::Widget*, 5> haulWidgets = {
			&resource_, &vehicleType_, &roadChoice_, &toggleRoad_, nullptr
		};
		const std::array<float, 5> haulWidths = { 220.0f, 220.0f, 220.0f, 132.0f, 0.0f };
		y = layoutRow(y, haulWidgets, haulWidths, nativeConstruction_ ? 4 : 2);
		const std::array<wi::gui::Widget*, 5> orderWidgets = {
			&order_, &retargetOrder_, &cancelOrder_, nullptr, nullptr
		};
		const std::array<float, 5> orderWidths = { 440.0f, 180.0f, 120.0f, 0.0f, 0.0f };
		y = layoutRow(y + kRowGap, orderWidgets, orderWidths, 3);
		if (hasUtilityResources_)
		{
			const std::array<wi::gui::Widget*, 5> utilityWidgets = {
				&utilityResource_, &connectUtility_, &disconnectUtility_, nullptr, nullptr
			};
			const std::array<float, 5> utilityWidths = { 220.0f, 160.0f, 170.0f, 0.0f, 0.0f };
			y = layoutRow(y + kRowGap, utilityWidgets, utilityWidths, 3);
		}
		hasLayout_ = true;
		return y + 8.0f;
	}

	void NativeEconomyTools::RefreshRoads(const economy::WorldSnapshot& snapshot)
	{
		std::vector<economy::RoadId> ids;
		std::vector<std::string> labels;
		ids.reserve(snapshot.roads.size());
		labels.reserve(snapshot.roads.size());
		for (const auto& road : snapshot.roads)
		{
			ids.push_back(road.id);
			labels.push_back("#" + std::to_string(road.id) + " (" +
				std::to_string(road.a.x) + "," + std::to_string(road.a.z) + ")->(" +
				std::to_string(road.b.x) + "," + std::to_string(road.b.z) + ") | " +
				(road.enabled ? "enabled" : "disabled"));
		}
		if (ids == roadIds_)
		{
			// Keep an open dropdown stable while a road is toggled externally.
			for (int i = 0; i < static_cast<int>(labels.size()); ++i)
				if (roadChoice_.GetItemText(i) != labels[static_cast<std::size_t>(i)])
					roadChoice_.SetItemText(i, labels[static_cast<std::size_t>(i)]);
			return;
		}
		const economy::RoadId previous = SelectedRoad();
		roadChoice_.ClearItems();
		int selected = -1;
		for (std::size_t i = 0; i < ids.size(); ++i)
		{
			roadChoice_.AddItem(labels[i], ids[i]);
			if (ids[i] == previous) selected = static_cast<int>(i);
		}
		if (selected < 0 && !haveRoadRefresh_ && !ids.empty()) selected = 0;
		roadChoice_.SetSelectedWithoutCallback(selected);
		roadChoice_.SetInvalidSelectionText(ids.empty() ? "No native roads" : "Choose a road");
		roadChoice_.SetText("");
		roadIds_ = std::move(ids);
		haveRoadRefresh_ = true;
	}

	void NativeEconomyTools::RefreshOrders(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog)
	{
		std::vector<economy::OrderId> ids;
		std::vector<std::string> labels;
		ids.reserve(snapshot.orders.size());
		labels.reserve(snapshot.orders.size());
		const auto buildingName = [&snapshot, &catalog](economy::BuildingId id) {
			const auto building = std::find_if(snapshot.buildings.begin(), snapshot.buildings.end(), [id](const auto& value) { return value.id == id; });
			if (building == snapshot.buildings.end()) return std::string("building #") + std::to_string(id);
			const auto* type = catalog.BuildingType(building->type);
			return (type ? type->name : std::string("building")) + std::string(" #") + std::to_string(id);
		};
		const auto stateName = [](economy::HaulState state) {
			switch (state) {
			case economy::HaulState::Idle: return "idle";
			case economy::HaulState::ToDestination: return "to destination";
			case economy::HaulState::ToSource: return "to source";
			case economy::HaulState::Waiting: return "waiting";
			case economy::HaulState::NoRoute: return "no route";
			}
			return "unknown";
		};
		for (const auto& value : snapshot.orders) {
			ids.push_back(value.id);
			const auto* resource = catalog.Resource(value.resource);
			labels.push_back("#" + std::to_string(value.id) + " " + buildingName(value.source) + " -> " +
				buildingName(value.destination) + " | " + (resource ? resource->name : std::string("resource #") + std::to_string(value.resource)) +
				" cargo " + std::to_string(value.cargo) + " | " + stateName(value.state));
		}
		if (ids == orderIds_) {
			// Update live cargo/state without destroying an open dropdown.
			for (int i = 0; i < static_cast<int>(labels.size()); ++i)
				if (order_.GetItemText(i) != labels[static_cast<std::size_t>(i)])
					order_.SetItemText(i, labels[static_cast<std::size_t>(i)]);
			return;
		}
		const economy::OrderId previous = SelectedOrder();
		order_.ClearItems();
		int selected = -1;
		for (std::size_t i = 0; i < ids.size(); ++i) {
			order_.AddItem(labels[i], ids[i]);
			if (ids[i] == previous) selected = static_cast<int>(i);
		}
		if (selected < 0 && !haveOrderRefresh_ && !ids.empty()) selected = 0;
		order_.SetSelectedWithoutCallback(selected);
		order_.SetInvalidSelectionText(ids.empty() ? "No active haul orders" : "Choose a haul order");
		order_.SetText("");
		orderIds_ = std::move(ids);
		haveOrderRefresh_ = true;
	}

	void NativeEconomyTools::RebuildResourceChoices(const std::vector<economy::ResourceId>& resources,
		const economy::Catalog& catalog, const std::string& emptyText)
	{
		const economy::ResourceId previous = SelectedResource();
		resource_.ClearItems();
		for (const economy::ResourceId id : resources)
		{
			const economy::ResourceDef* definition = catalog.Resource(id);
			if (definition != nullptr)
			{
				resource_.AddItem(definition->name, definition->id);
			}
		}

		int selectedIndex = -1;
		if (previous != economy::InvalidResourceId)
		{
			for (int i = 0; i < static_cast<int>(resource_.GetItemCount()); ++i)
			{
				if (resource_.GetItemUserData(i) == previous)
				{
					selectedIndex = i;
					break;
				}
			}
		}
		if (selectedIndex < 0 && resource_.GetItemCount() != 0)
		{
			selectedIndex = 0;
		}
		resource_.SetSelectedWithoutCallback(selectedIndex);
		resource_.SetInvalidSelectionText(emptyText);
		resource_.SetText("");
		haulResources_ = resources;
		haveHaulResources_ = true;
	}

	void NativeEconomyTools::RefreshHaulChoices(const economy::WorldSnapshot& snapshot,
		const economy::Catalog& catalog, std::optional<economy::BuildingId> source,
		std::optional<economy::BuildingId> destination)
	{
		if (const auto* vehicle = catalog.VehicleType(SelectedVehicleType()))
			vehicleType_.SetTooltip(vehicle->name + ": capacity " + std::to_string(vehicle->capacity) +
				" catalog units, " + std::to_string(vehicle->cellsPerTick) + " cells/tick.");
		else vehicleType_.SetTooltip("Choose the vehicle type for the haul order.");
		std::vector<economy::ResourceId> resources;
		std::string emptyText;
		if (!source || !destination || *source == 0 || *destination == 0 || *source == *destination)
		{
			emptyText = "Choose haul endpoints";
		}
		else
		{
			resources = runtime::ListNativeHaulResources(snapshot, catalog, *source, *destination);
			emptyText = resources.empty() ? "No shared storage" : "Choose a resource";
		}

		if (!haveHaulResources_ || resources != haulResources_)
		{
			RebuildResourceChoices(resources, catalog, emptyText);
		}
		else if (resources.empty())
		{
			// Endpoint availability can change while the empty resource vector stays
			// empty, so keep the useful prompt current without clearing the menu.
			resource_.SetInvalidSelectionText(emptyText);
		}
	}

	economy::BuildingTypeId NativeEconomyTools::SelectedBuildingType() const noexcept
	{
		return SelectedId(buildingType_, economy::InvalidBuildingTypeId);
	}

	economy::ResourceId NativeEconomyTools::SelectedResource() const noexcept
	{
		return SelectedId(resource_, economy::InvalidResourceId);
	}

	economy::ResourceId NativeEconomyTools::SelectedUtilityResource() const noexcept
	{
		return SelectedId(utilityResource_, economy::InvalidResourceId);
	}

	economy::VehicleTypeId NativeEconomyTools::SelectedVehicleType() const noexcept
	{
		return SelectedId(vehicleType_, economy::InvalidVehicleTypeId);
	}

	economy::OrderId NativeEconomyTools::SelectedOrder() const noexcept
	{
		return SelectedId(order_, economy::OrderId{0});
	}

	economy::RoadId NativeEconomyTools::SelectedRoad() const noexcept
	{
		return SelectedId(roadChoice_, economy::RoadId{0});
	}

	bool NativeEconomyTools::HasActiveInput() const noexcept
	{
		const auto active = [](const wi::gui::Widget& widget) {
			return widget.IsVisible() && widget.IsEnabled() && widget.GetState() >= wi::gui::ACTIVE;
		};
		return active(buildingType_) || active(place_) || active(road_) || active(cancel_) ||
			active(remove_) || active(resource_) || active(vehicleType_) || active(order_) || active(retargetOrder_) || active(cancelOrder_) ||
			active(roadChoice_) || active(toggleRoad_) || active(utilityResource_) ||
			active(connectUtility_) || active(disconnectUtility_);
	}

	bool NativeEconomyTools::PointerOnControls(XMFLOAT2 pointer) const noexcept
	{
		if (!hasLayout_) return false;
		return WidgetContains(buildingType_, pointer) || WidgetContains(place_, pointer) ||
			WidgetContains(road_, pointer) || WidgetContains(cancel_, pointer) ||
			WidgetContains(remove_, pointer) || WidgetContains(resource_, pointer) ||
			WidgetContains(vehicleType_, pointer) || WidgetContains(order_, pointer) ||
			WidgetContains(retargetOrder_, pointer) || WidgetContains(cancelOrder_, pointer) ||
			WidgetContains(roadChoice_, pointer) || WidgetContains(toggleRoad_, pointer) ||
			WidgetContains(utilityResource_, pointer) || WidgetContains(connectUtility_, pointer) ||
			WidgetContains(disconnectUtility_, pointer);
	}
}
