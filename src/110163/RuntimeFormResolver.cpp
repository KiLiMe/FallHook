// AI CONTEXT: Resolves plugin/FormID and editor-ID identities against live game forms.
// Depends on CommonLibF4 TESDataHandler/TESForm and RuntimeFormResolver declarations.
// Runtime scope is Fallout 4 1.10.163 loaded plugin resolution.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: uses only stable form identity; no original text comparison is performed.
#include "PCH.h"

#include "110163/RuntimeFormResolver.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace
{
	std::mutex g_pluginLock;
	std::unordered_map<std::string, const RE::TESFile*> g_loadedPlugins;
	std::mutex g_formLock;
	std::unordered_map<std::uint32_t, RE::TESForm*> g_formsByRuntimeID;
	std::unordered_map<std::string, RE::TESForm*> g_formsByEditorID;
	bool g_pluginSnapshotBuilt{ false };

	struct RawFormKey
	{
		std::uint32_t rawFormID{ 0 };
		std::string plugin;

		bool operator==(const RawFormKey&) const = default;
	};

	struct RawFormKeyHash
	{
		std::size_t operator()(const RawFormKey& key) const noexcept
		{
			const auto rawHash = std::hash<std::uint32_t>{}(key.rawFormID);
			const auto pluginHash = std::hash<std::string>{}(key.plugin);
			return rawHash ^ (pluginHash + 0x9E3779B9u + (rawHash << 6) + (rawHash >> 2));
		}
	};

	std::mutex g_rawIDLock;
	std::unordered_map<RawFormKey, std::uint32_t, RawFormKeyHash> g_runtimeIDsByRaw;

	std::string pluginKey(std::string_view pluginName)
	{
		std::string key{ pluginName };
		std::ranges::transform(key, key.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return key;
	}

	void addLoadedPluginLocked(const RE::TESFile* file)
	{
		if (!file || !file->IsActive() || file->GetFilename().empty())
		{
			return;
		}
		g_loadedPlugins.insert_or_assign(pluginKey(file->GetFilename()), file);
	}

	bool buildLoadedPluginSnapshotLocked(const RE::TESDataHandler* handler)
	{
		if (!handler)
		{
			return false;
		}

		for (const auto* file : handler->compiledFileCollection.files)
		{
			addLoadedPluginLocked(file);
		}
		for (const auto* file : handler->compiledFileCollection.smallFiles)
		{
			addLoadedPluginLocked(file);
		}

		g_pluginSnapshotBuilt = !g_loadedPlugins.empty();
		return g_pluginSnapshotBuilt;
	}

	const RE::TESFile* findLoadedPlugin(std::string_view pluginName)
	{
		const auto handler = RE::TESDataHandler::GetSingleton();
		if (!handler || pluginName.empty())
		{
			return nullptr;
		}
		const auto key = pluginKey(pluginName);

		std::scoped_lock lock{ g_pluginLock };
		if (!g_pluginSnapshotBuilt)
		{
			buildLoadedPluginSnapshotLocked(handler);
		}
		if (const auto it = g_loadedPlugins.find(key); it != g_loadedPlugins.end())
		{
			return it->second;
		}

		if (const auto* file = handler->LookupLoadedModByName(pluginName))
		{
			addLoadedPluginLocked(file);
			return file;
		}
		if (const auto* file = handler->LookupLoadedLightModByName(pluginName))
		{
			addLoadedPluginLocked(file);
			return file;
		}
		if (const auto* file = handler->LookupModByName(pluginName))
		{
			addLoadedPluginLocked(file);
			return file;
		}
		return nullptr;
	}

	std::uint32_t trimRawFormID(std::uint32_t rawFormID, const RE::TESFile* file)
	{
		if (!file)
		{
			return rawFormID;
		}

		return file->IsLight() ? (rawFormID & 0xFFF) : (rawFormID & 0x00FFFFFF);
	}

	RE::TESFormID makeRuntimeFormID(const RE::TESFile* file, std::uint32_t rawFormID)
	{
		if (!file)
		{
			return rawFormID;
		}

		const auto local = trimRawFormID(rawFormID, file);
		if (file->IsLight())
		{
			return 0xFE000000 | (static_cast<RE::TESFormID>(file->smallFileCompileIndex) << 12) | local;
		}

		return (static_cast<RE::TESFormID>(file->compileIndex) << 24) | local;
	}

	[[nodiscard]] bool canCacheMiss() noexcept
	{
		const auto* handler = RE::TESDataHandler::GetSingleton();
		return handler && !handler->loadingFiles;
	}
}

namespace RuntimeFormResolver
{
	std::optional<std::uint32_t> ResolveRawFormID(std::uint32_t rawFormID, std::string_view pluginName)
	{
		if (rawFormID == 0 || pluginName.empty())
		{
			return std::nullopt;
		}

		RawFormKey key{ rawFormID, pluginKey(pluginName) };
		{
			std::scoped_lock lock{ g_rawIDLock };
			if (const auto it = g_runtimeIDsByRaw.find(key); it != g_runtimeIDsByRaw.end())
			{
				return it->second ? std::optional<std::uint32_t>{ it->second } : std::nullopt;
			}
		}

		const auto* file = findLoadedPlugin(pluginName);
		if (!file)
		{
			if (canCacheMiss())
			{
				std::scoped_lock lock{ g_rawIDLock };
				g_runtimeIDsByRaw.insert_or_assign(std::move(key), 0);
			}
			return std::nullopt;
		}

		const auto local = trimRawFormID(rawFormID, file);
		const auto runtimeFormID = makeRuntimeFormID(file, local);
		{
			std::scoped_lock lock{ g_rawIDLock };
			g_runtimeIDsByRaw.insert_or_assign(std::move(key), runtimeFormID);
		}
		return runtimeFormID;
	}

	RE::TESForm* ResolveRawForm(std::uint32_t rawFormID, std::string_view pluginName)
	{
		const auto runtimeFormID = ResolveRawFormID(rawFormID, pluginName);
		if (!runtimeFormID)
		{
			return nullptr;
		}

		{
			std::scoped_lock lock{ g_formLock };
			if (const auto it = g_formsByRuntimeID.find(*runtimeFormID); it != g_formsByRuntimeID.end())
			{
				return it->second;
			}
		}

		auto* form = RE::TESForm::GetFormByID(*runtimeFormID);
		if (form || canCacheMiss())
		{
			std::scoped_lock lock{ g_formLock };
			g_formsByRuntimeID.insert_or_assign(*runtimeFormID, form);
		}
		return form;
	}

	RE::TESForm* ResolveEditorForm(const SourceFreeTranslationData& data)
	{
		if (!data.editorID || data.editorID->empty())
		{
			return nullptr;
		}

		{
			std::scoped_lock lock{ g_formLock };
			if (const auto it = g_formsByEditorID.find(*data.editorID); it != g_formsByEditorID.end())
			{
				return it->second;
			}
		}

		auto* form = RE::TESForm::GetFormByEditorID(RE::BSFixedString{ data.editorID->c_str() });
		if (form || canCacheMiss())
		{
			std::scoped_lock lock{ g_formLock };
			g_formsByEditorID.insert_or_assign(*data.editorID, form);
		}
		return form;
	}

	void ClearLiveFormCache()
	{
		std::scoped_lock lock{ g_formLock };
		g_formsByRuntimeID.clear();
		g_formsByEditorID.clear();
	}
}
