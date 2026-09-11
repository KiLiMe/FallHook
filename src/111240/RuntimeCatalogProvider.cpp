// AI CONTEXT: Implements shared lazy catalog readiness/build access for runtime hooks.
// Depends on RuntimeLoadWatchdog, RuntimePreload, and CommonLibF4 TESDataHandler.
// Runtime scope is Fallout 4 1.11.240 catalog availability for runtime hook maps.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: only builds/returns source-free catalog data; never uses Source text.
#include "PCH.h"

#include "111240/RuntimeCatalogProvider.h"

#include "RuntimeLoadWatchdog.h"
#include "111240/RuntimePreload.h"

#include "RE/T/TESDataHandler.h"

namespace Runtime111240
{
namespace RuntimeCatalogProvider
{
	bool HasLoadedPluginList() noexcept
	{
		const auto* handler = RE::TESDataHandler::GetSingleton();
		return handler &&
			(handler->compiledFileCollection.files.size() != 0 ||
			 handler->compiledFileCollection.smallFiles.size() != 0);
	}

	const TranslationPipelineResult* Ensure(std::string_view phaseName)
	{
		if (!HasLoadedPluginList())
		{
			return nullptr;
		}

		RuntimeLoadWatchdog::ScopedPhase phase{ phaseName, 0.0 };
		RuntimePreload::BuildCatalogOnce();
		return RuntimePreload::GetCatalogBuildResult();
	}
}

} // namespace Runtime111240
