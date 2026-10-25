#include "openrepublic/runtime/native_toolbar_layout.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	int failures = 0;

	void Check(bool condition, const char* message)
	{
		if (!condition)
		{
			std::cerr << "FAIL: " << message << '\n';
			++failures;
		}
	}

	void CheckFits(float viewportWidth, const openrepublic::runtime::NativeToolbarLayout& layout)
	{
		for (const auto& item : layout.items)
			Check(item.x >= 0.0f && item.x + item.width <= viewportWidth - 18.0f,
				"item stays inside the right viewport margin");
	}
}

int main()
{
	using openrepublic::runtime::LayoutNativeToolbar;
	const auto wide = LayoutNativeToolbar(1920.0f / 1.4f, 14);
	Check(wide.items.size() == 14 && wide.rows == 2, "wide viewport wraps the full toolbar onto two rows");
	Check(wide.items[10].y == 18.0f && wide.items[11].y == 52.0f,
		"wide viewport places eleven items on row one and wraps the next item");
	CheckFits(1920.0f / 1.4f, wide);

	const auto compact = LayoutNativeToolbar(1280.0f / 1.4f, 14);
	Check(compact.items.size() == 14 && compact.rows == 2, "1280 pixel viewport wraps the full toolbar onto two rows");
	Check(compact.items[6].y == 18.0f && compact.items[7].y == 52.0f,
		"1280 pixel viewport wraps after seven controls");
	CheckFits(1280.0f / 1.4f, compact);

	const auto empty = LayoutNativeToolbar(1280.0f, 0, 42.0f);
	Check(empty.rows == 0 && empty.items.empty() && empty.nextY == 42.0f,
		"empty toolbar preserves the caller start position");
	const auto invalid = LayoutNativeToolbar(std::numeric_limits<float>::quiet_NaN(), 5, 42.0f);
	Check(invalid.items.empty() && invalid.nextY == 42.0f,
		"non-finite viewport returns a safe empty layout");

	if (failures != 0) return 1;
	std::cout << "All native-toolbar-layout tests passed\n";
	return 0;
}
