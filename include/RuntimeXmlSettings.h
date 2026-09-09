// AI CONTEXT: Reads the [XML] load-order mode from FallHook.ini.
// Depends only on standard types and XmlLoadOrder mode values.
// Runtime assumptions: version-neutral setting read before catalog build.
// Version-specific logic: none; this file must not add runtime compatibility gates.
// Source-free policy: selects file ordering only; no translation text is read here.
#pragma once

#include "XmlLoadOrder.h"

namespace RuntimeXmlSettings
{
	struct Values
	{
		XmlLoadOrder::Mode loadOrderMode{ XmlLoadOrder::Mode::kPlugin };
	};

	[[nodiscard]] Values Load();
}
