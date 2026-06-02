// AI CONTEXT: Reads the isolated source-keyed in-game text hook settings.
// Depends only on standard types; implementation reads FallHook.ini from the game plugin directory.
// Runtime assumptions: version-neutral setting consumed only by the selected runtime module.
// Version-specific logic: none; hook offsets and UI sites live in version modules.
// Source-free policy: this module is an explicit TXT-only exception and must not affect XML lookup.
#pragma once

namespace RuntimeInGameTextSettings
{
	struct Values
	{
		bool enable{ true };
		bool logRaw{ false };

		[[nodiscard]] bool Active() const noexcept
		{
			return enable || logRaw;
		}
	};

	[[nodiscard]] Values Load();
}
