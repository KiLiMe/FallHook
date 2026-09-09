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

	// A module owns one concrete runtime layout. The AE (1.11.x) module was
	// written and verified against 1.11.191; because FallHook locates every
	// hook through the Address Library (REL::ID), the same AE module also runs
	// on newer AE patches such as 1.11.240 without code changes.
	[[nodiscard]] bool ModuleAccepts(
		const RuntimeVersionDispatcher::Module& a_module,
		const REL::Version& a_runtime) noexcept
	{
		if (a_module.runtime == a_runtime) {
			return true;
		}

		// AE family: 1.11.191 module also covers 1.11.240.
		return a_module.runtime == RuntimeVersionDispatcher::kRuntime111191 &&
			a_runtime == RuntimeVersionDispatcher::kRuntime111240;
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
