// AI CONTEXT: Builds the 1.11.240 XDI dialogue option prompt/response mini-map.
// Depends on RuntimeFormResolver, RuntimeApplySettings, and TranslationCatalog records.
// Runtime scope is XDI DialogueMenu option patching only.
// Version-specific logic: resolves 1.11.240 runtime INFO form IDs.
// Source-free policy: uses INFO form identity and NAM1 index only; never reads XML Source or visible text.
#include "PCH.h"

#include "111240/RuntimeXdiDialogueOptionTranslations.h"

#include "RuntimeApplySettings.h"
#include "111240/RuntimeFormResolver.h"

#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace Runtime111240
{
namespace
{
	constexpr std::uint32_t kUnsetIndex{ std::numeric_limits<std::uint32_t>::max() };

	struct OptionEntry
	{
		std::string prompt;
		std::string response;
		std::uint32_t responseIndex{ kUnsetIndex };
		bool hasPrompt{ false };
		bool hasResponse{ false };
	};

	struct OptionSnapshot
	{
		std::unordered_map<std::uint32_t, OptionEntry> byInfoForm;
		bool traceEnabled{ false };
	};

	std::mutex g_rebuildLock;
	const TranslationCatalogBuildResult* g_catalog{ nullptr };
	std::atomic<std::shared_ptr<const OptionSnapshot>> g_snapshot;

	[[nodiscard]] bool hasText(std::string_view text)
	{
		return text.find_first_not_of(" \t\r\n") != std::string_view::npos;
	}

	[[nodiscard]] bool isPromptRecord(const TranslationCatalogRecord& record) noexcept
	{
		return record.recordSignature == "INFO RNAM" &&
			   record.data.translationType == TranslationType::kRuntime2 &&
			   hasText(record.data.replacerText);
	}

	[[nodiscard]] bool isResponseRecord(const TranslationCatalogRecord& record) noexcept
	{
		return record.recordSignature == "INFO NAM1" &&
			   record.data.translationType == TranslationType::kRuntimeIndex &&
			   hasText(record.data.replacerText);
	}

	[[nodiscard]] std::optional<std::uint32_t> runtimeFormID(const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			return RuntimeFormResolver::ResolveRawFormID(*record.data.formID, record.pluginName);
		}
		if (auto* form = RuntimeFormResolver::ResolveEditorForm(record.data))
		{
			return form->formID;
		}
		return std::nullopt;
	}

	[[nodiscard]] bool isBetterResponseIndex(std::uint32_t incoming, std::uint32_t current) noexcept
	{
		if (current == kUnsetIndex)
		{
			return true;
		}
		return incoming < current;
	}
}

namespace RuntimeXdiDialogueOptionTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force)
	{
		std::scoped_lock lock{ g_rebuildLock };
		if (!force && g_catalog == std::addressof(catalog))
		{
			return;
		}

		auto snapshot = std::make_shared<OptionSnapshot>();
		snapshot->traceEnabled = RuntimeApplySettings::Load().TraceEnabled();
		snapshot->byInfoForm.reserve(catalog.records.size() / 8);

		std::size_t prompts = 0;
		std::size_t responses = 0;
		for (const auto& record : catalog.records)
		{
			if (!isPromptRecord(record) && !isResponseRecord(record))
			{
				continue;
			}
			const auto formID = runtimeFormID(record);
			if (!formID)
			{
				continue;
			}
			auto& entry = snapshot->byInfoForm[*formID];
			if (isPromptRecord(record))
			{
				entry.prompt = record.data.replacerText;
				entry.hasPrompt = true;
				++prompts;
			}
			else
			{
				const auto index = record.data.index.value_or(kUnsetIndex);
				if (isBetterResponseIndex(index, entry.responseIndex))
				{
					entry.response = record.data.replacerText;
					entry.responseIndex = index;
					entry.hasResponse = true;
				}
				++responses;
			}
		}

		g_catalog = std::addressof(catalog);
		g_snapshot.store(std::static_pointer_cast<const OptionSnapshot>(snapshot), std::memory_order_release);
		if (snapshot->traceEnabled)
		{
			REX::INFO(
				"{} xdi-dialogue option map built: infoForms={} prompts={} responses={}.",
				Plugin::NAME,
				snapshot->byInfoForm.size(),
				prompts,
				responses);
		}
	}

	OptionText Resolve(std::uint32_t infoFormID)
	{
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot || infoFormID == 0)
		{
			return {};
		}
		const auto found = snapshot->byInfoForm.find(infoFormID);
		if (found == snapshot->byInfoForm.end())
		{
			return {};
		}
		return {
			.prompt = found->second.prompt,
			.response = found->second.response,
			.infoFormID = infoFormID,
			.hasPrompt = found->second.hasPrompt,
			.hasResponse = found->second.hasResponse
		};
	}
}

} // namespace Runtime111240
