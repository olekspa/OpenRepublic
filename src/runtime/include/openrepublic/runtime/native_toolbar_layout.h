#pragma once

#include <cstddef>
#include <vector>

namespace openrepublic::runtime
{
	struct NativeToolbarItemLayout
	{
		float x{};
		float y{};
		float width{};
		float height{};
	};

	struct NativeToolbarLayout
	{
		std::vector<NativeToolbarItemLayout> items;
		float nextY{};
		std::size_t rows{};
	};

	/** Wraps fixed-width toolbar controls to keep every item within the viewport. */
	NativeToolbarLayout LayoutNativeToolbar(float viewportWidth, std::size_t itemCount,
		float startY = 18.0f, float itemWidth = 114.0f, float itemHeight = 28.0f,
		float horizontalMargin = 18.0f, float horizontalGap = 6.0f, float rowGap = 6.0f);
}
