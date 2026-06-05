// AI CONTEXT: Maps constructed dialogue string-pool identity to resolved subtitle destination text.
// Depends on RuntimeApplySettings and CommonLibF4 TESTopicInfo/BSFixedString storage.
// Runtime scope is Fallout 4 1.11.191 response construction followed by subtitle display.
// Version-specific logic: keys by 1.11.191 INFO form ID, speaker form ID, and pooled raw response pointer.
// Source-free policy: raw text content is never read or matched; pending handoff uses resolved dialogue identity.
#include "PCH.h"

#include "111191/RuntimeDialogueSubtitleContext.h"

#include "RuntimeApplySettings.h"

#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESTopicInfo.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace Runtime111191
{
namespace
{
	constexpr std::uint32_t kTraceLimit{ 512 };

	struct ContextKey
	{
		std::uint32_t infoFormID{ 0 };
		std::uint32_t speakerFormID{ 0 };
		const char* rawPointer{ nullptr };

		[[nodiscard]] bool operator==(const ContextKey&) const = default;
	};

	struct ContextKeyHash
	{
		[[nodiscard]] std::size_t operator()(const ContextKey& key) const noexcept
		{
			const auto pointer = reinterpret_cast<std::uintptr_t>(key.rawPointer);
			const auto mixed = pointer ^
				(static_cast<std::uintptr_t>(key.infoFormID) << 17) ^
				(static_cast<std::uintptr_t>(key.speakerFormID) << 7);
			return std::hash<std::uintptr_t>{}(mixed);
		}
	};

	struct PendingKey
	{
		std::uint32_t infoFormID{ 0 };
		std::uint32_t speakerFormID{ 0 };

		[[nodiscard]] bool operator==(const PendingKey&) const = default;
	};

	struct PendingKeyHash
	{
		[[nodiscard]] std::size_t operator()(const PendingKey& key) const noexcept
		{
			const auto mixed = static_cast<std::uintptr_t>(key.infoFormID) ^
				(static_cast<std::uintptr_t>(key.speakerFormID) << 13);
			return std::hash<std::uintptr_t>{}(mixed);
		}
	};

	struct ContextEntry
	{
		RE::BSFixedStringCS text;
		std::uint32_t ordinal{ 0xFFFFFFFFu };
		std::uint32_t responseID{ 0 };
		bool ambiguous{ false };
	};

	struct PendingEntry
	{
		const char* rawPointer{ nullptr };
		ContextEntry entry;
	};

	std::shared_mutex g_contextLock;
	std::unordered_map<ContextKey, ContextEntry, ContextKeyHash> g_contexts;
	std::unordered_map<PendingKey, std::deque<PendingEntry>, PendingKeyHash> g_pending;
	std::atomic_uint32_t g_traceLines{ 0 };

	[[nodiscard]] bool traceEnabled()
	{
		static const bool enabled = RuntimeApplySettings::Load().TraceEnabled();
		return enabled;
	}

	void trace(
		std::string_view stage,
		std::uint32_t infoFormID,
		std::uint32_t speakerFormID,
		const char* rawPointer,
		std::uint32_t ordinal,
		std::uint32_t responseID,
		std::size_t textLen)
	{
		if (!traceEnabled() || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} dialogue-subtitle-context trace stage={} info={:08X} speaker={:08X} rawPtr={} ordinal={} responseID={} textLen={}",
			Plugin::NAME,
			stage,
			infoFormID,
			speakerFormID,
			static_cast<const void*>(rawPointer),
			ordinal,
			responseID,
			textLen);
	}

	[[nodiscard]] bool sameText(const RE::BSFixedStringCS& left, const RE::BSFixedStringCS& right)
	{
		return std::string_view{ left.c_str(), left.length() } ==
			std::string_view{ right.c_str(), right.length() };
	}

	[[nodiscard]] std::uint32_t speakerFormID(const RE::TESObjectREFR* speaker) noexcept
	{
		return speaker ? speaker->formID : 0;
	}

	void rememberPending(const ContextKey& key, const ContextEntry& entry)
	{
		const PendingKey pendingKey{ key.infoFormID, key.speakerFormID };
		auto& queue = g_pending[pendingKey];
		constexpr std::size_t kMaxPendingPerSpeakerInfo{ 16 };
		if (queue.size() >= kMaxPendingPerSpeakerInfo)
		{
			queue.pop_front();
		}
		queue.push_back(PendingEntry{ key.rawPointer, entry });
	}

	void dropPendingRaw(const ContextKey& key)
	{
		const PendingKey pendingKey{ key.infoFormID, key.speakerFormID };
		const auto found = g_pending.find(pendingKey);
		if (found == g_pending.end())
		{
			return;
		}
		auto& queue = found->second;
		std::erase_if(queue, [&](const PendingEntry& pending) {
			return pending.rawPointer == key.rawPointer;
		});
		if (queue.empty())
		{
			g_pending.erase(found);
		}
	}

	[[nodiscard]] RuntimeDialogueSubtitleContext::LookupResult consumePending(const ContextKey& key)
	{
		const PendingKey pendingKey{ key.infoFormID, key.speakerFormID };
		const auto found = g_pending.find(pendingKey);
		if (found == g_pending.end())
		{
			return {};
		}
		auto& queue = found->second;
		while (!queue.empty() && (queue.front().entry.ambiguous || queue.front().entry.text.empty()))
		{
			queue.pop_front();
		}
		if (queue.empty())
		{
			g_pending.erase(found);
			return {};
		}

		const auto entry = queue.front().entry;
		queue.pop_front();
		if (queue.empty())
		{
			g_pending.erase(found);
		}
		return { entry.text, entry.ordinal, entry.responseID, true };
	}
}

namespace RuntimeDialogueSubtitleContext
{
	void Remember(
		RE::TESTopicInfo* topicInfo,
		RE::TESObjectREFR* speaker,
		const RE::BSFixedStringCS& rawText,
		const RE::BSFixedStringCS& destination,
		std::uint32_t ordinal,
		std::uint32_t responseID)
	{
		if (!topicInfo || rawText.empty() || destination.empty())
		{
			return;
		}

		const ContextKey key{ topicInfo->formID, speakerFormID(speaker), rawText.c_str() };
		const ContextEntry entry{ destination, ordinal, responseID, false };
		std::unique_lock lock{ g_contextLock };
		auto [found, inserted] = g_contexts.try_emplace(key, entry);
		if (!inserted && !sameText(found->second.text, destination))
		{
			found->second.text = {};
			found->second.ambiguous = true;
			rememberPending(key, found->second);
			trace("ambiguous", key.infoFormID, key.speakerFormID, key.rawPointer, ordinal, responseID, 0);
			return;
		}
		if (!inserted)
		{
			found->second.ordinal = ordinal;
			found->second.responseID = responseID;
		}
		rememberPending(key, found->second);
		trace("remember", key.infoFormID, key.speakerFormID, key.rawPointer, ordinal, responseID, destination.length());
	}

	LookupResult Resolve(RE::TESTopicInfo* topicInfo, RE::TESObjectREFR* speaker, const RE::BSFixedStringCS& rawText)
	{
		if (!topicInfo || rawText.empty())
		{
			return {};
		}

		const ContextKey key{ topicInfo->formID, speakerFormID(speaker), rawText.c_str() };
		std::unique_lock lock{ g_contextLock };
		const auto found = g_contexts.find(key);
		if (found == g_contexts.end() || found->second.ambiguous || found->second.text.empty())
		{
			auto queued = consumePending(key);
			if (queued.translated && !queued.text.empty())
			{
				trace("queue-hit", key.infoFormID, key.speakerFormID, key.rawPointer, queued.ordinal, queued.responseID, queued.text.length());
				return queued;
			}
			trace("miss", key.infoFormID, key.speakerFormID, key.rawPointer, 0xFFFFFFFFu, 0, 0);
			return {};
		}
		const auto result = LookupResult{
			found->second.text,
			found->second.ordinal,
			found->second.responseID,
			true
		};
		dropPendingRaw(key);
		trace("hit", key.infoFormID, key.speakerFormID, key.rawPointer, result.ordinal, result.responseID, result.text.length());
		return result;
	}

	void Clear()
	{
		std::unique_lock lock{ g_contextLock };
		g_contexts.clear();
		g_pending.clear();
		g_traceLines.store(0, std::memory_order_relaxed);
	}
}


} // namespace Runtime111191
