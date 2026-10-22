#pragma once

#include "openrepublic/economy/economy.h"
#include "openrepublic/runtime/native_economy_tools.h"
#include "wiGUI.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace openrepublic::preview
{
	/** Native economy construction and haul-selection toolbar.
	 *
	 * This class owns presentation widgets only. Commands are delegated to the
	 * supplied callbacks; selection never mutates an economy world directly.
	 */
	class NativeEconomyTools final
	{
	public:
		struct Callbacks
		{
			std::function<void()> place;
			std::function<void()> road;
			std::function<void()> cancel;
			std::function<void()> remove;
			std::function<void()> retargetOrder;
			std::function<void()> cancelOrder;
			std::function<void()> toggleRoad;
		};

		void Create(wi::gui::GUI& gui, const economy::Catalog& catalog, Callbacks callbacks,
			bool nativeConstruction = true);

		/** Positions the construction and haul rows below the caller's top row.
		 *  Returns the y-coordinate available for the next status/control row. */
		float Layout(float viewportWidth, float startY);

		/** Rebuilds the resource dropdown only when its eligible resource IDs
		 *  change, preserving the current resource when it remains eligible. */
		void RefreshHaulChoices(const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog,
			std::optional<economy::BuildingId> source,
			std::optional<economy::BuildingId> destination);

		/** Refreshes active haul-order labels only when displayed contents change. */
		void RefreshOrders(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog);
		/** Refreshes native road choices, preserving selection by exact road ID. */
		void RefreshRoads(const economy::WorldSnapshot& snapshot);

		economy::BuildingTypeId SelectedBuildingType() const noexcept;
		economy::ResourceId SelectedResource() const noexcept;
		economy::VehicleTypeId SelectedVehicleType() const noexcept;
		economy::OrderId SelectedOrder() const noexcept;
		economy::RoadId SelectedRoad() const noexcept;

		/** True while a dropdown or other toolbar widget is actively capturing input. */
		bool HasActiveInput() const noexcept;
		/** Tests the visible, enabled toolbar widget rectangles against a screen point. */
		bool PointerOnControls(XMFLOAT2 pointer) const noexcept;

	private:
		static void StyleButton(wi::gui::Button& button);
		static void StyleCombo(wi::gui::ComboBox& combo);
		void RegisterWidgets(wi::gui::GUI& gui);
		void RebuildResourceChoices(const std::vector<economy::ResourceId>& resources,
			const economy::Catalog& catalog, const std::string& emptyText);

		wi::gui::GUI* gui_ = nullptr;
		Callbacks callbacks_;
		wi::gui::ComboBox buildingType_;
		wi::gui::ComboBox resource_;
		wi::gui::ComboBox vehicleType_;
		wi::gui::ComboBox order_;
		wi::gui::ComboBox roadChoice_;
		wi::gui::Button place_;
		wi::gui::Button road_;
		wi::gui::Button cancel_;
		wi::gui::Button remove_;
		wi::gui::Button retargetOrder_;
		wi::gui::Button cancelOrder_;
		wi::gui::Button toggleRoad_;
		std::vector<economy::ResourceId> haulResources_;
		std::vector<economy::OrderId> orderIds_;
		std::vector<economy::RoadId> roadIds_;
		bool haveHaulResources_ = false;
		bool haveOrderRefresh_ = false;
		bool haveRoadRefresh_ = false;
		bool hasLayout_ = false;
		bool nativeConstruction_ = true;
	};
}
