// AI CONTEXT: Declares the version-neutral runtime module dispatcher contract.
// Depends on F4SE runtime version types and selected module callbacks.
// Runtime assumptions: no owning game version; concrete modules own version-specific logic.
// Version-specific logic: none; this interface only selects already-compiled modules.
// Source-free policy: dispatcher does not inspect translation text or enable source-text lookup.
#pragma once

#include "F4SE/F4SE.h"

#include <span>
#include <string_view>

namespace RuntimeVersionDispatcher
{
	inline constexpr REL::Version kRuntime110163 = F4SE::RUNTIME_1_10_163;
	inline constexpr REL::Version kRuntime111191 = F4SE::RUNTIME_1_11_191;

	struct Module
	{
		std::string_view name;
		REL::Version runtime;
		bool (*Load)(const F4SE::LoadInterface* f4se);
		void (*HandleMessage)(F4SE::MessagingInterface::Message* message);
		void (*Shutdown)();
	};

	[[nodiscard]] std::span<const Module* const> Modules() noexcept;
	[[nodiscard]] const Module* Find(const REL::Version& runtime) noexcept;
}
