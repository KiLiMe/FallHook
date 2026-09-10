// AI CONTEXT: Implements version-neutral selection of the active FallHook runtime module.
// Depends on concrete compiled runtime module registrars.
// Runtime assumptions: no owning game version; exactly one compatible module is selected by runtime.
// Version-specific logic: none; module files own hook offsets and runtime layout assumptions.
// Source-free policy: only dispatches startup/messages; no text lookup or mutation happens here.
#include "PCH.h"

#include "RuntimeVersionDispatcher.h"

#include "110163/RuntimeModule110163.h"
#include "111191/RuntimeModule111191.h"

namespace
{
	const RuntimeVersionDispatcher::Module* const kModules[] = {
		&RuntimeModule110163::GetModule(),
		&Runtime111191::RuntimeModule111191::GetModule()
	};

	// A module owns a concrete runtime layout. Because FallHook locates
	// every hook through the Address Library (REL::ID), the same module
	// runs on any runtime in the same major.minor family — e.g. the
	// 1.11.191 module covers all 1.11.x patches automatically.
	[[nodiscard]] bool ModuleAccepts(
		const RuntimeVersionDispatcher::Module& a_module,
		const REL::Version& a_runtime) noexcept
	{
		return a_module.runtime.major() == a_runtime.major() &&
			a_module.runtime.minor() == a_runtime.minor();
	}
}

namespace RuntimeVersionDispatcher
{
	std::span<const Module* const> Modules() noexcept
	{
		return kModules;
	}

	const Module* Find(const REL::Version& runtime) noexcept
	{
		for (const auto* module : Modules())
		{
			if (module && ModuleAccepts(*module, runtime))
			{
				return module;
			}
		}
		return nullptr;
	}
}
