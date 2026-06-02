// AI CONTEXT: Version-neutral F4SE plugin entrypoint and runtime module dispatcher.
// Depends on shared settings/watchdog helpers and RuntimeVersionDispatcher.
// Runtime assumptions: no owning game version; selects one compatible module from F4SE runtime.
// Version-specific logic: none; concrete runtime modules own hook offsets and data layout assumptions.
// Source-free policy: startup exposes no original-text lookup modes and no INI runtime override.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "RuntimeActivityWatch.h"
#include "RuntimeHookWatch.h"
#include "RuntimeLoadWatchdog.h"
#include "RuntimeSaveLoadGapTrace.h"
#include "RuntimeVersionDispatcher.h"
#include "RuntimeWatchdogSettings.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <spdlog/spdlog.h>

namespace
{
	const RuntimeVersionDispatcher::Module* g_activeModule{ nullptr };

	[[nodiscard]] const char* MessageTypeLabel(std::uint32_t type) noexcept
	{
		switch (type)
		{
		case F4SE::MessagingInterface::kPostLoad: return "PostLoad";
		case F4SE::MessagingInterface::kPostPostLoad: return "PostPostLoad";
		case F4SE::MessagingInterface::kInputLoaded: return "InputLoaded";
		case F4SE::MessagingInterface::kGameDataReady: return "GameDataReady";
		case F4SE::MessagingInterface::kPreLoadGame: return "PreLoadGame";
		case F4SE::MessagingInterface::kPostLoadGame: return "PostLoadGame";
		case F4SE::MessagingInterface::kNewGame: return "NewGame";
		default: return "Unknown";
		}
	}

	void F4SEAPI MessageListener(F4SE::MessagingInterface::Message* message)
	{
		if (!message)
		{
			return;
		}

		constexpr std::string_view saveLoadWaitPhase{ "waiting for PostLoadGame after PreLoadGame" };
		if (message->type == F4SE::MessagingInterface::kPostLoadGame ||
			message->type == F4SE::MessagingInterface::kNewGame)
		{
			if (RuntimeLoadWatchdog::SaveLoadGapActive())
			{
				RuntimeSaveLoadGapTrace::LogSummaryAndReset(MessageTypeLabel(message->type));
			}
			RuntimeLoadWatchdog::SetSaveLoadGapActive(false);
			RuntimeLoadWatchdog::ClearWaitPhase(saveLoadWaitPhase);
		}

		const std::string messagePhase = std::string{ "F4SE message " } + MessageTypeLabel(message->type);
		RuntimeLoadWatchdog::ScopedPhase messageScope{ messagePhase, 0.0 };
		REX::DEBUG("{} received F4SE message {} ({}).", Plugin::NAME, message->type, MessageTypeLabel(message->type));
		if (message->type == F4SE::MessagingInterface::kPreLoadGame)
		{
			RuntimeSaveLoadGapTrace::Reset();
			RuntimeLoadWatchdog::SetSaveLoadGapActive(true);
			RuntimeLoadWatchdog::SetWaitPhase(saveLoadWaitPhase);
		}
		if (g_activeModule && g_activeModule->HandleMessage)
		{
			g_activeModule->HandleMessage(message);
		}
	}
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_DETACH)
	{
		if (g_activeModule && g_activeModule->Shutdown)
		{
			g_activeModule->Shutdown();
			g_activeModule = nullptr;
		}
		RuntimeActivityWatch::Shutdown();
		RuntimeHookWatch::Shutdown();
		RuntimeSaveLoadGapTrace::Reset();
		RuntimeLoadWatchdog::SetSaveLoadGapActive(false);
		RuntimeLoadWatchdog::Shutdown();
	}
	return TRUE;
}

F4SE_EXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* f4se, F4SE::PluginInfo* pluginInfo)
{
	if (!f4se || !pluginInfo)
	{
		return false;
	}

	pluginInfo->infoVersion = F4SE::PluginInfo::kVersion;
	pluginInfo->name = Plugin::NAME.data();
	pluginInfo->version = Plugin::VERSION.pack();

	if (f4se->IsEditor())
	{
		return false;
	}

	return RuntimeVersionDispatcher::Find(f4se->RuntimeVersion()) != nullptr;
}

F4SEPluginVersion = []() noexcept {
	F4SE::PluginVersionData data{};
	data.PluginName(Plugin::NAME);
	data.PluginVersion(Plugin::VERSION);
	data.AuthorName("FallHook"sv);
	data.UsesAddressLibrary(true);
	data.IsLayoutDependent(true);
		data.CompatibleVersions({ RuntimeVersionDispatcher::kRuntime110163, RuntimeVersionDispatcher::kRuntime111191 });
	return data;
}();

F4SEPluginLoad(const F4SE::LoadInterface* f4se)
{
	if (!f4se)
	{
		return false;
	}

	const auto* module = RuntimeVersionDispatcher::Find(f4se->RuntimeVersion());
	if (!module)
	{
		return false;
	}

	F4SE::Init(
		f4se,
		F4SE::InitInfo{
			.log = true,
			.logName = Plugin::NAME.data(),
			.logPattern = "[%H:%M:%S:%e] [%l] %v",
			.trampoline = true,
			.trampolineSize = 4096
		});

	const auto settings = RuntimeApplySettings::Load();
	const auto watchdogSettings = RuntimeWatchdogSettings::Load();
	RuntimeLoadWatchdog::Configure(watchdogSettings.enable);
	RuntimeHookWatch::Configure(watchdogSettings.enable);
	RuntimeActivityWatch::Configure(watchdogSettings.activity);
	if (settings.TraceEnabled())
	{
		spdlog::set_level(spdlog::level::trace);
		spdlog::flush_on(spdlog::level::trace);
	}
	else
	{
		spdlog::set_level(spdlog::level::info);
		spdlog::flush_on(spdlog::level::info);
	}

	RuntimeLoadWatchdog::ScopedPhase loadPhase{ "F4SEPluginLoad total", 0.0 };
	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging)
	{
		REX::ERROR("{} could not acquire F4SE messaging interface.", Plugin::NAME);
		return false;
	}

	g_activeModule = module;
	{
		RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RegisterListener", 0.0 };
		messaging->RegisterListener(MessageListener);
	}

	REX::INFO("{} selected runtime module {} for Fallout 4 runtime {}.", Plugin::NAME, module->name, f4se->RuntimeVersion());
	if (!module->Load(f4se))
	{
		g_activeModule = nullptr;
		return false;
	}

	REX::INFO("{} watchdog enabled={} independently of debug trace flags.", Plugin::NAME, watchdogSettings.enable);
	REX::INFO("{} loaded for Fallout 4 runtime {}.", Plugin::NAME, f4se->RuntimeVersion());
	return true;
}
