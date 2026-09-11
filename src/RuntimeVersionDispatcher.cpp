// AI CONTEXT: Implements version-neutral selection of the active FallHook runtime module.
// Depends on concrete compiled runtime module registrars.
// Runtime assumptions: no owning game version; exactly one compatible module is selected by runtime.
// Version-specific logic: none; module files own hook offsets and runtime layout assumptions.
// Source-free policy: only dispatches startup/messages; no text lookup or mutation happens here.
#include "PCH.h"

#include "RuntimeVersionDispatcher.h"

#include "110163/RuntimeModule110163.h"
#include "111191/RuntimeModule111191.h"
#include "111240/RuntimeModule111240.h"

namespace
{
	const RuntimeVersionDispatcher::Module* const kModules[] = {
		&RuntimeModule110163::GetModule(),
		&Runtime111191::RuntimeModule111191::GetModule(),
		&Runtime111240::RuntimeModule111240::GetModule()
	};
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
			if (module && module->runtime == runtime)
			{
				return module;
			}
		}
		return nullptr;
	}
}
