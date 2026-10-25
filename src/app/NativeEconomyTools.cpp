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
		gui.AddWidget(&dispatchVehicle_); gui.AddWidget(&dispatchSource_); gui.AddWidget(&dispatchDestination_);
		gui.AddWidget(&dispatchResource_); gui.AddWidget(&dispatchAmount_);
		gui.AddWidget(&dispatch_); gui.AddWidget(&buyCoal_); gui.AddWidget(&autoRun_);
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
			gui_->RemoveWidget(&dispatchVehicle_); gui_->RemoveWidget(&dispatchSource_); gui_->RemoveWidget(&dispatchDestination_);
			gui_->RemoveWidget(&dispatchResource_); gui_->RemoveWidget(&dispatchAmount_);
			gui_->RemoveWidget(&dispatch_); gui_->RemoveWidget(&buyCoal_); gui_->RemoveWidget(&autoRun_);
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
		dispatchVehicle_.Create("NativeCoalTruck"); dispatchSource_.Create("NativeCoalSource");
		dispatchDestination_.Create("NativeCoalDestination"); dispatchResource_.Create("NativeCoalResource");
		dispatchAmount_.Create("NativeCoalAmount");
		for (auto* combo : { &dispatchVehicle_, &dispatchSource_, &dispatchDestination_, &dispatchResource_, &dispatchAmount_ }) StyleCombo(*combo);
		dispatchAmount_.AddItem("4 units", 4); dispatchAmount_.AddItem("8 units", 8); dispatchAmount_.SetSelectedWithoutCallback(0);
		dispatch_.Create("NativeDispatchTruck"); dispatch_.SetText("Dispatch"); StyleButton(dispatch_);
		dispatch_.OnClick([this](const wi::gui::EventArgs&) { if (callbacks_.dispatchTruck) callbacks_.dispatchTruck(); });
		buyCoal_.Create("NativeBuyCoal"); buyCoal_.SetText("Buy coal"); StyleButton(buyCoal_);
		buyCoal_.OnClick([this](const wi::gui::EventArgs&) { if (callbacks_.buyCoal) callbacks_.buyCoal(); });
		autoRun_.Create("NativeAutoRunLogistics"); autoRun_.SetText("Auto-run logistics: off"); StyleButton(autoRun_);
		autoRun_.OnClick([this](const wi::gui::EventArgs&) { autoRunEnabled_ = !autoRunEnabled_; SetAutoRun(autoRunEnabled_); if (callbacks_.toggleAutoRun) callbacks_.toggleAutoRun(); });

		buildingType_.Create("NativeEconomyBuildingType");
		buildingType_.ClearItems();
		buildingType_.SetTooltip("Choose a building. Click its minimum X/Z footprint cell. Its access cell below must remain clear; roads connect deliveries.");
		buildingType_.SetInvalidSelectionText("No building types available");
		for (const economy::BuildingTypeDef& type : catalog.BuildingTypes())
		{
			buildingType_.AddItem(type.name + " (" + std::to_string(type.width) + "x" +
				std::to_string(type.depth) + ")", type.id);
		}
		buildingType_.OnSelect([this, &catalog](const wi::gui::EventArgs&) {
			const auto* type = catalog.BuildingType(SelectedBuildingType());
			if (!type) return;
			std::string tip = type->name + " | footprint " + std::to_string(type->width) + " x " + std::to_string(type->depth) + " cells";
			for (const auto& recipe : type->recipes) {
				tip += "\n" + recipe.name + ": ";
				const auto append = [&catalog, &tip](const char* label, const std::vector<economy::RecipeAmount>& amounts) {
					if (amounts.empty()) return;
					tip += label;
					for (std::size_t i = 0; i < amounts.size(); ++i) {
						const auto* resource = catalog.Resource(amounts[i].resource);
						if (i) tip += ", ";
						tip += (resource ? resource->name : "resource") + std::string(" ") + std::to_string(amounts[i].amount);
					}
				};
				append("in ", recipe.inputs); append(" out ", recipe.outputs);
			}
			if (!type->constructionMaterials.empty() || type->constructionWorkTicks) {
				tip += "\nBuild requirements (OpenRepublic synthetic): ";
				for (std::size_t i = 0; i < type->constructionMaterials.size(); ++i) {
					const auto& amount = type->constructionMaterials[i]; const auto* resource = catalog.Resource(amount.resource);
					if (i) tip += ", ";
					tip += (resource ? resource->name : "resource") + std::string(" ") + std::to_string(amount.amount);
				}
				if (type->constructionWorkTicks) tip += "; work " + std::to_string(type->constructionWorkTicks) + " ticks";
			} else tip += "\nBuild requirements: none declared";
			buildingType_.SetTooltip(tip);
		});
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
		utilityResource_.OnSelect([this](const wi::gui::EventArgs&) {
			if (callbacks_.utilitySelectionChanged) callbacks_.utilitySelectionChanged();
		});

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
		const std::array<wi::gui::Widget*, 5> dispatchWidgets = { &dispatchVehicle_, &dispatchSource_, &dispatchDestination_, &dispatchResource_, &dispatchAmount_ };
		const std::array<float, 5> dispatchWidths = { 160.0f, 180.0f, 180.0f, 150.0f, 90.0f };
		y = layoutRow(y + kRowGap, dispatchWidgets, dispatchWidths, 5);
		const std::array<wi::gui::Widget*, 5> dispatchButtons = { &dispatch_, &buyCoal_, &autoRun_, nullptr, nullptr };
		const std::array<float, 5> dispatchButtonWidths = { 110.0f, 110.0f, 200.0f, 0.0f, 0.0f };
		y = layoutRow(y + kRowGap, dispatchButtons, dispatchButtonWidths, 3);
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

	void NativeEconomyTools::RefreshDispatchChoices(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog)
	{
		const auto selected = [](const wi::gui::ComboBox& c) -> std::uint64_t { return c.GetSelected() >= 0 ? c.GetSelectedUserdata() : 0; };
		const auto rebuild = [](wi::gui::ComboBox& combo, const std::vector<std::pair<std::string, std::uint64_t>>& items,
			std::uint64_t prior, std::vector<std::uint64_t>& oldIds) {
			std::vector<std::uint64_t> ids; ids.reserve(items.size());
			for (const auto& item : items) ids.push_back(item.second);
			if (ids == oldIds) {
				for (std::size_t i = 0; i < items.size(); ++i)
					if (combo.GetItemText(static_cast<int>(i)) != items[i].first) combo.SetItemText(static_cast<int>(i), items[i].first);
				return;
			}
			combo.ClearItems(); int index = -1;
			for (std::size_t i = 0; i < items.size(); ++i) { combo.AddItem(items[i].first, items[i].second); if (items[i].second == prior) index = static_cast<int>(i); }
			combo.SetSelectedWithoutCallback(index >= 0 ? index : (items.empty() ? -1 : 0)); oldIds = std::move(ids);
		};
		const auto pv=selected(dispatchVehicle_), ps=selected(dispatchSource_), pd=selected(dispatchDestination_), pr=selected(dispatchResource_);
		std::vector<std::pair<std::string,std::uint64_t>> vehicles, buildings, resources;
		for (const auto& v : snapshot.vehicles) if (v.ownerDepot) {
			const char* leg = "parked";
			switch (v.leg) { case economy::VehicleLeg::ToSource: leg="to source"; break; case economy::VehicleLeg::Loading: leg="loading"; break;
			case economy::VehicleLeg::ToDestination: leg="to destination"; break; case economy::VehicleLeg::Unloading: leg="unloading"; break;
			case economy::VehicleLeg::Returning: leg="returning"; break; default: break; }
			vehicles.emplace_back("Truck #"+std::to_string(v.id)+" | "+leg+" | cargo "+std::to_string(v.cargo),v.id);
		}
		for (const auto& b : snapshot.buildings) { const auto* t=catalog.BuildingType(b.type); buildings.emplace_back((t?t->name:"building")+std::string(" #")+std::to_string(b.id),b.id); }
		for (const auto& r : catalog.Resources()) if (!r.utility) resources.emplace_back(r.name,r.id);
		rebuild(dispatchVehicle_,vehicles,pv,dispatchVehicleIds_); rebuild(dispatchSource_,buildings,ps,dispatchSourceIds_);
		rebuild(dispatchDestination_,buildings,pd,dispatchDestinationIds_); rebuild(dispatchResource_,resources,pr,dispatchResourceIds_);
	}
	void NativeEconomyTools::SetAutoRun(bool enabled) { autoRunEnabled_=enabled; autoRun_.SetText(enabled ? "Auto-run logistics: on" : "Auto-run logistics: off"); }
	void NativeEconomyTools::SetCoalDemoControls(bool enabled) {
		buyCoal_.SetVisible(enabled); buyCoal_.SetEnabled(enabled);
		autoRun_.SetVisible(enabled); autoRun_.SetEnabled(enabled);
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
	economy::VehicleId NativeEconomyTools::SelectedDispatchVehicle() const noexcept { return SelectedId(dispatchVehicle_, economy::VehicleId{}); }
	economy::BuildingId NativeEconomyTools::SelectedDispatchSource() const noexcept { return SelectedId(dispatchSource_, economy::BuildingId{}); }
	economy::BuildingId NativeEconomyTools::SelectedDispatchDestination() const noexcept { return SelectedId(dispatchDestination_, economy::BuildingId{}); }
	economy::ResourceId NativeEconomyTools::SelectedDispatchResource() const noexcept { return SelectedId(dispatchResource_, economy::InvalidResourceId); }
	std::uint64_t NativeEconomyTools::SelectedDispatchAmount() const noexcept { return SelectedId(dispatchAmount_, std::uint64_t{}); }

	bool NativeEconomyTools::HasActiveInput() const noexcept
	{
		const auto active = [](const wi::gui::Widget& widget) {
			return widget.IsVisible() && widget.IsEnabled() && widget.GetState() >= wi::gui::ACTIVE;
		};
		return active(buildingType_) || active(place_) || active(road_) || active(cancel_) ||
			active(remove_) || active(resource_) || active(vehicleType_) || active(order_) || active(retargetOrder_) || active(cancelOrder_) ||
			active(roadChoice_) || active(toggleRoad_) || active(utilityResource_) ||
			active(connectUtility_) || active(disconnectUtility_) || active(dispatchVehicle_) || active(dispatchSource_) ||
			active(dispatchDestination_) || active(dispatchResource_) || active(dispatchAmount_) || active(dispatch_) || active(buyCoal_) || active(autoRun_);
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
			WidgetContains(disconnectUtility_, pointer) || WidgetContains(dispatchVehicle_, pointer) || WidgetContains(dispatchSource_, pointer) ||
			WidgetContains(dispatchDestination_, pointer) || WidgetContains(dispatchResource_, pointer) || WidgetContains(dispatchAmount_, pointer) ||
			WidgetContains(dispatch_, pointer) || WidgetContains(buyCoal_, pointer) || WidgetContains(autoRun_, pointer);
	}
}
