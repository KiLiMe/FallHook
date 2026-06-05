// AI CONTEXT: Applies the user-approved post-data HUDRollover action replacement exception.
// Depends on activation/PERK destination maps, activation look context, catalog provider, and memory safety helpers.
// Runtime scope is Fallout 4 1.10.163 HUDRollover trap/furniture/robot action slots only.
// Version-specific logic: patches ShowRolloverParameters +0x08 and +0x10 after trace verification.
// Source-free policy: never compares visible action text; approval permits no Source matching.
#include "PCH.h"

#include "110163/RuntimeHudRolloverActionPatch.h"

#include "110163/RuntimeActivationTextTranslations.h"
#include "110163/RuntimeCatalogProvider.h"
#include "RuntimeActivityWatch.h"
#include "RuntimeMemorySafety.h"
#include "110163/RuntimePerkActivateChoiceTranslations.h"

#include "RE/B/BSFixedString.h"
#include "RE/B/BSStringPool.h"
#include "RE/E/ENUM_FORM_ID.h"
#include "RE/T/TESForm.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

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
			if (auto text = RuntimeActivationTextTranslations::Lookup(base); text && !text->empty())
			{
				return text;
			}
		}
		if (auto* ref = formByID(context.ref))
		{
			if (auto text = RuntimeActivationTextTranslations::Lookup(ref); text && !text->empty())
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
			"{} hud-rollover trace stage={} ref={:08X} refType={} base={:08X} baseLocal={:06X} baseType={} slot={} destLen={}",
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

	[[nodiscard]] bool patchSlot(
		void* parameters,
		std::size_t offset,
		const std::string& text,
		std::vector<RuntimeHudRolloverActionPatch::Patch>& patches)
	{
		if (text.empty() || !hasStringEntryAt(parameters, offset))
		{
			return false;
		}

		RuntimeActivityWatch::WorkScope activity{ "HUDRollover patch action slot" };
		auto* field = reinterpret_cast<const void**>(reinterpret_cast<std::byte*>(parameters) + offset);
		if (!RuntimeMemorySafety::IsWritableMemory(field, sizeof(const void*)))
		{
			return false;
		}

		const auto* replacement = cachedFixedStringEntry(text);
		if (!replacement)
		{
			return false;
		}

		patches.push_back(RuntimeHudRolloverActionPatch::Patch{ .field = field, .original = *field });
		*field = replacement;
		return true;
	}
}

namespace RuntimeHudRolloverActionPatch
{
	void ConfigureTrace(bool enabled) noexcept
	{
		g_traceEnabled.store(enabled, std::memory_order_release);
		g_traceLines.store(0, std::memory_order_relaxed);
	}

	std::vector<Patch> TryApply(
		void* parameters,
		const RuntimeActivationLookContext::Snapshot& context)
	{
		std::vector<Patch> patches;
		if (!parameters)
		{
			return patches;
		}

		RuntimeActivityWatch::WorkScope activity{ "HUDRollover action patch" };
		ensureTranslations();
		if (auto text = activationText(context))
		{
			if (patchSlot(parameters, kPrimaryActionSlot, *text, patches))
			{
				tracePatch("primary-replace", context, "+8", text->size());
			}
		}
		if (auto text = perkSecondaryText(context))
		{
			if (patchSlot(parameters, kSecondaryActionSlot, *text, patches))
			{
				tracePatch("secondary-hack-replace", context, "+10", text->size());
			}
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
