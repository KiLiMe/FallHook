// AI CONTEXT: Restores control markup into already-resolved MESG destination text.
// Depends only on standard strings.
// Runtime assumptions: version-neutral Fallout 4 MESG button/DESC control markup display preservation.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: AGENTS-approved MESG icon exception; runtime text supplies markup only after source-free target resolution.
// Exception note: do not remove as source-exact repair; this preserves runtime <> and [] control markup inside translated MESG text.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace MessageIconFormatter
{
	struct ApplyResult
	{
		std::string text;
		std::size_t replaced{ 0 };

		[[nodiscard]] bool changed() const noexcept
		{
			return replaced != 0;
		}
	};

	ApplyResult ApplyRuntimeControlMarkup(std::string_view runtimeText, std::string_view translation);
}
