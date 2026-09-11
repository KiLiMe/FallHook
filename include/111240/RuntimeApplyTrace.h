// AI CONTEXT: Emits gated per-entry runtime const-apply trace lines to FallHook.log.
// Depends on ConstApplyEntry identity data, RuntimeApplySettings, and CommonLibF4 form metadata.
// Runtime scope is Fallout 4 1.11.240 direct text mutation diagnostics.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: logs source-free identity fields only; never uses original Source text as identity.
#pragma once

#include "RuntimeApplySettings.h"

#include <cstdint>
#include <string_view>

struct ConstApplyEntry;

namespace RE
{
	class TESForm;
}

namespace Runtime111240::RuntimeApplyTrace
{
	void Reset();
	void Summary(const RuntimeApplySettings::Values& settings, std::string_view reason);
	void Entry(
		const RuntimeApplySettings::Values& settings,
		std::string_view stage,
		std::string_view target,
		std::uint32_t rawFormID,
		const ConstApplyEntry& entry,
		const RE::TESForm* form = nullptr);
}
