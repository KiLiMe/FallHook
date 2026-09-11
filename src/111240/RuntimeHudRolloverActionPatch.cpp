// AI CONTEXT: Applies the 1.11.240 HUDRollover action replacement path.
// Depends on activation/PERK destination maps, activation look context, catalog provider, and memory safety helpers.
// Runtime scope is Fallout 4 1.11.240 HUDRollover trap/furniture/robot action slots only.
// Version-specific logic: patches moved ShowRolloverParameters +0x08 and +0x10 after hook verification.
// Source-free policy: never compares visible action text; uses form/sID/editor identity only.
#include "PCH.h"

#include "111240/RuntimeHudRolloverActionPatch.h"

#include "111240/RuntimeActivationTextTranslations.h"
#include "111240/RuntimeCatalogProvider.h"
#include "111240/RuntimePerkActivateChoiceTranslations.h"
#include "RuntimeActivityWatch.h"
#include "RuntimeMemorySafety.h"

#include "RE/B/BSFixedString.h"
#include "RE/B/BSStringPool.h"
#include "RE/E/ENUM_FORM_ID.h"
#include "RE/T/TESForm.h"

#include <atomic>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Runtime111240::RuntimeHudRolloverActionPatch
{
namespace
{
	constexpr std::size_t kPrimaryActionSlot{ 0x08 };
	constexpr std::size_t kSecondaryActionSlot{ 0x10 };
	constexpr std::size_t kMaxTextBytes{ 96 };
	constexpr std::uint32_t kArmorStandActorLocalID{ 0x000008B3 };

	std::mutex g_catalogLock;
	std::atomic_bool g_catalogReady{ false };
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };

	enum class PatchSlotStatus
	{
		kApplied,
		kEmptyText,
		kNoReadableEntry,
		kNotWritable,
		kNoReplacement
	};

	void ensureTranslations()
	{
		if (g_catalogReady.load(std::memory_order_acquire) || !RuntimeCatalogProvider::HasLoadedPluginList())
		{
			return;
		}

		RuntimeActivityWatch::WorkScope activity{ "HUDRollover ensure action maps" };
		std::scoped_lock lock{ g_catalogLock };
		if (g_catalogReady.load(std::memory_order_relaxed))
		{
			return;
		}

		if (const auto* catalogResult = RuntimeCatalogProvider::Ensure("RuntimeHudRolloverActionPatch build action maps"))
		{
			RuntimeActivationTextTranslations::Rebuild(catalogResult->prepared.activationText);
			RuntimePerkActivateChoiceTranslations::Rebuild(catalogResult->prepared.perkActivateChoice);
			g_catalogReady.store(true, std::memory_order_release);
		}
	}

	[[nodiscard]] const RE::BSStringPool::Entry* readableStringEntry(const void* pointer)
	{
		auto* entry = static_cast<const RE::BSStringPool::Entry*>(pointer);
		for (std::uint32_t depth = 0; depth < 8; ++depth)
		{
			if (!RuntimeMemorySafety::IsReadableMemory(entry, sizeof(RE::BSStringPool::Entry)))
			{
				return nullptr;
			}
			if (entry->shallow())
			{
				entry = entry->_right;
				continue;
			}
			if (entry->wide() || entry->_length == 0 || entry->_length > kMaxTextBytes)
			{
				return nullptr;
			}
			const auto* text = reinterpret_cast<const char*>(entry + 1);
			return RuntimeMemorySafety::IsReadableMemory(text, entry->_length + 1) && text[entry->_length] == '\0' ?
				entry :
				nullptr;
		}
		return nullptr;
	}

	[[nodiscard]] bool hasStringEntryAt(void* parameters, std::size_t offset)
	{
		const auto pointer = RuntimeMemorySafety::ReadUnaligned<std::uintptr_t>(parameters, offset);
		if (!pointer || *pointer < 0x10000)
		{
			return false;
		}
		return readableStringEntry(reinterpret_cast<const void*>(*pointer)) != nullptr;
	}

	[[nodiscard]] const void* cachedFixedStringEntry(const std::string& text)
	{
		thread_local std::vector<std::pair<std::string, RE::BSFixedStringCS>> cache;
		for (const auto& [key, fixed] : cache)
		{
			if (key == text)
			{
				return *reinterpret_cast<const void* const*>(std::addressof(fixed));
			}
		}
		if (cache.size() >= 32)
		{
			return nullptr;
		}
		cache.emplace_back(text, RE::BSFixedStringCS{ text.c_str() });
		return *reinterpret_cast<const void* const*>(std::addressof(cache.back().second));
	}

	[[nodiscard]] RE::TESForm* formByID(std::uint32_t formID)
	{
		if (formID == 0)
		{
			return nullptr;
		}
		auto* form = RE::TESForm::GetFormByID(formID);
		return RuntimeMemorySafety::IsReadableMemory(form, sizeof(RE::TESForm)) ? form : nullptr;
	}

	[[nodiscard]] bool isArmorStandActor(const RuntimeActivationLookContext::Snapshot& context) noexcept
	{
		return context.refTypeID == static_cast<std::uint32_t>(RE::ENUM_FORM_ID::kACHR) &&
			context.baseTypeID == static_cast<std::uint32_t>(RE::ENUM_FORM_ID::kNPC_) &&
			context.baseLocal == kArmorStandActorLocalID;
	}

	[[nodiscard]] bool isNpcSecondaryAction(const RuntimeActivationLookContext::Snapshot& context) noexcept
	{
		return context.ref != 0 &&
			context.base != 0 &&
			context.refTypeID == static_cast<std::uint32_t>(RE::ENUM_FORM_ID::kACHR) &&
			context.baseTypeID == static_cast<std::uint32_t>(RE::ENUM_FORM_ID::kNPC_);
	}

	[[nodiscard]] std::optional<std::string> activationText(const RuntimeActivationLookContext::Snapshot& context)
	{
		RuntimeActivityWatch::WorkScope activity{ "HUDRollover activation lookup" };
		if (auto* base = formByID(context.base))
		{
			if (auto text = RuntimeActivationTextTranslations::Lookup(base, std::nullopt); text && !text->empty())
			{
				return text;
			}
		}
		if (auto* ref = formByID(context.ref))
		{
			if (auto text = RuntimeActivationTextTranslations::Lookup(ref, std::nullopt); text && !text->empty())
			{
				return text;
			}
		}
		if (isArmorStandActor(context))
		{
			return RuntimeActivationTextTranslations::KnownFurnitureUseText();
		}
		return std::nullopt;
	}

	[[nodiscard]] std::optional<std::string> perkSecondaryText(const RuntimeActivationLookContext::Snapshot& context)
	{
		if (!isNpcSecondaryAction(context))
		{
			return std::nullopt;
		}
		RuntimeActivityWatch::WorkScope activity{ "HUDRollover secondary perk lookup" };
		if (auto text = RuntimePerkActivateChoiceTranslations::LookupHudSecondaryTarget(context.baseEditor); text && !text->empty())
		{
			return text;
		}
		if (auto text = RuntimePerkActivateChoiceTranslations::LookupHudSecondaryTarget(context.refEditor); text && !text->empty())
		{
			return text;
		}
		return RuntimePerkActivateChoiceTranslations::RoboticsExpertHackText();
	}

	[[nodiscard]] std::string_view slotName(std::size_t offset) noexcept
	{
		return offset == kPrimaryActionSlot ? "+8" : "+10";
	}

	[[nodiscard]] std::string skipStage(std::string_view prefix, PatchSlotStatus status)
	{
		switch (status)
		{
		case PatchSlotStatus::kApplied:
			return std::format("{}-replace", prefix);
		case PatchSlotStatus::kEmptyText:
			return std::format("skip-{}-empty-text", prefix);
		case PatchSlotStatus::kNoReadableEntry:
			return std::format("skip-{}-no-readable-entry", prefix);
		case PatchSlotStatus::kNotWritable:
			return std::format("skip-{}-not-writable", prefix);
		case PatchSlotStatus::kNoReplacement:
			return std::format("skip-{}-no-replacement", prefix);
		}
		return std::string{ prefix };
	}

	void tracePatch(
		std::string_view stage,
		const RuntimeActivationLookContext::Snapshot& context,
		std::string_view slot,
		std::size_t destLen)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed) ||
			g_traceLines.fetch_add(1, std::memory_order_relaxed) >= 192)
		{
			return;
		}

		REX::INFO(
			"{} hud-rollover action patch stage={} ref={:08X} refType={} base={:08X} baseLocal={:06X} baseType={} slot={} destLen={}",
			Plugin::NAME,
			stage,
			context.ref,
			context.refType,
			context.base,
			context.baseLocal,
			context.baseType,
			slot,
			destLen);
	}

	[[nodiscard]] PatchSlotStatus patchSlot(
		void* parameters,
		std::size_t offset,
		const std::string& text,
		std::vector<Patch>& patches)
	{
		if (text.empty())
		{
			return PatchSlotStatus::kEmptyText;
		}
		if (!hasStringEntryAt(parameters, offset))
		{
			return PatchSlotStatus::kNoReadableEntry;
		}

		RuntimeActivityWatch::WorkScope activity{ "HUDRollover patch action slot" };
		auto* field = reinterpret_cast<const void**>(reinterpret_cast<std::byte*>(parameters) + offset);
		if (!RuntimeMemorySafety::IsWritableMemory(field, sizeof(const void*)))
		{
			return PatchSlotStatus::kNotWritable;
		}

		const auto* replacement = cachedFixedStringEntry(text);
		if (!replacement)
		{
			return PatchSlotStatus::kNoReplacement;
		}

		patches.push_back(Patch{ .field = field, .original = *field });
		*field = replacement;
		return PatchSlotStatus::kApplied;
	}
}

	void ConfigureTrace(bool enabled) noexcept
	{
		g_traceEnabled.store(enabled, std::memory_order_release);
		g_traceLines.store(0, std::memory_order_relaxed);
	}

	void traceSlotResult(
		std::string_view label,
		PatchSlotStatus status,
		const RuntimeActivationLookContext::Snapshot& context,
		std::size_t offset,
		std::size_t textLen)
	{
		const auto stage = skipStage(label, status);
		tracePatch(stage, context, slotName(offset), textLen);
	}

	std::vector<Patch> TryApply(void* parameters, const RuntimeActivationLookContext::Snapshot& context)
	{
		std::vector<Patch> patches;
		if (!parameters)
		{
			tracePatch("skip-null-parameters", context, "", 0);
			return patches;
		}

		RuntimeActivityWatch::WorkScope activity{ "HUDRollover action patch" };
		ensureTranslations();
		const auto text = activationText(context);
		if (!text)
		{
			tracePatch("skip-no-action-text", context, "", 0);
		}
		else
		{
			traceSlotResult("primary", patchSlot(parameters, kPrimaryActionSlot, *text, patches), context, kPrimaryActionSlot, text->size());
		}

		if (auto secondary = perkSecondaryText(context))
		{
			traceSlotResult(
				"secondary-perk",
				patchSlot(parameters, kSecondaryActionSlot, *secondary, patches),
				context,
				kSecondaryActionSlot,
				secondary->size());
		}
		return patches;
	}

	void Restore(const std::vector<Patch>& patches)
	{
		for (auto it = patches.rbegin(); it != patches.rend(); ++it)
		{
			if (it->field && RuntimeMemorySafety::IsWritableMemory(it->field, sizeof(const void*)))
			{
				*it->field = it->original;
			}
		}
	}
}
