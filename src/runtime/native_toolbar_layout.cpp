#include "openrepublic/runtime/native_toolbar_layout.h"

#include <algorithm>
#include <cmath>

namespace openrepublic::runtime
{
	NativeToolbarLayout LayoutNativeToolbar(float viewportWidth, std::size_t itemCount,
		float startY, float itemWidth, float itemHeight, float horizontalMargin,
		float horizontalGap, float rowGap)
	{
		NativeToolbarLayout result;
		if (!std::isfinite(viewportWidth) || !std::isfinite(startY) || !std::isfinite(itemWidth) ||
			!std::isfinite(itemHeight) || !std::isfinite(horizontalMargin) ||
			!std::isfinite(horizontalGap) || !std::isfinite(rowGap) ||
			itemWidth <= 0.0f || itemHeight <= 0.0f || horizontalGap < 0.0f || rowGap < 0.0f)
		{
			result.nextY = std::isfinite(startY) ? startY : 0.0f;
			return result;
		}

		viewportWidth = std::max(0.0f, viewportWidth);
		horizontalMargin = std::clamp(horizontalMargin, 0.0f, viewportWidth * 0.5f);
		const float availableWidth = std::max(1.0f, viewportWidth - horizontalMargin * 2.0f);
		const float width = std::min(itemWidth, availableWidth);
		const float right = viewportWidth - horizontalMargin;
		float x = horizontalMargin;
		float y = startY;
		result.items.reserve(itemCount);
		for (std::size_t i = 0; i < itemCount; ++i)
		{
			if (i != 0 && x + width > right + 0.001f)
			{
				x = horizontalMargin;
				y += itemHeight + rowGap;
				++result.rows;
			}
			else if (i == 0)
			{
				result.rows = 1;
			}
			result.items.push_back({ x, y, width, itemHeight });
			x += width + horizontalGap;
		}
		result.nextY = itemCount == 0 ? startY : y + itemHeight;
		return result;
	}
}
