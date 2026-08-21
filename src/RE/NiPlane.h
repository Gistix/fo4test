#pragma once

#include "RE/NetImmerse/NiPoint.h"

namespace RE
{
	class NiPlane
	{
	public:
		NiPoint3 normal{ 0.0f, 0.0f, 0.0f };  // 00
		float    constant{ 0.0f };            // 0C
	};
	static_assert(sizeof(NiPlane) == 0x10);
}
