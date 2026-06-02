// AI CONTEXT: Maintains immutable-string lookup maps for the QUST:CNAM journal text hook.
// Depends on RuntimeFormResolver and TranslationCatalog records built from XML/plugin identity.
// Runtime scope is Fallout 4 1.10.163 QUST:CNAM quest journal stage/item identity maps only.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: maps by runtime quest FormID plus stage item index; Source text is ignored.
#include "PCH.h"

#include "110163/RuntimeFormResolver.h"
#include "110163/RuntimeQuestLogTranslations.h"

#include <array>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace
{
	struct QuestJournalKey
	{
		std::uint32_t questFormID{ 0 };
		std::uint32_t value{ 0 };

		[[nodiscard]] bool operator==(const QuestJournalKey&) const = default;
	};

	struct QuestJournalKeyHash
	{
		[[nodiscard]] std::size_t operator()(const QuestJournalKey& key) const noexcept
		{
			const auto combined = (static_cast<std::uint64_t>(key.questFormID) << 32) | key.value;
			return std::hash<std::uint64_t>{}(combined);
		}
	};

	struct QuestJournalStageItemKey
	{
		std::uint32_t questFormID{ 0 };
		std::uint16_t stageIndex{ 0 };
		std::uint8_t itemIndex{ 0 };

		[[nodiscard]] bool operator==(const QuestJournalStageItemKey&) const = default;
	};

	struct QuestJournalStageItemKeyHash
	{
		[[nodiscard]] std::size_t operator()(const QuestJournalStageItemKey& key) const noexcept
		{
			const auto combined =
				(static_cast<std::uint64_t>(key.questFormID) << 24) |
				(static_cast<std::uint64_t>(key.stageIndex) << 8) |
				key.itemIndex;
			return std::hash<std::uint64_t>{}(combined);
		}
	};

	using QuestJournalMap = std::unordered_map<QuestJournalKey, const std::string*, QuestJournalKeyHash>;
	using QuestJournalDirectMap = std::unordered_map<
		QuestJournalStageItemKey,
		RuntimeQuestLogTranslations::LookupResult,
		QuestJournalStageItemKeyHash>;

	struct QuestJournalSnapshot
	{
		std::deque<std::string> textStorage;
		QuestJournalMap byIndex;
		mutable std::shared_mutex directLock;
		mutable QuestJournalDirectMap byStageItem;
	};

	std::mutex g_rebuildLock;
	std::atomic<std::shared_ptr<const QuestJournalSnapshot>> g_snapshot;

	bool hasText(std::string_view text)
	{
		for (const auto ch : text)
		{
			if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
			{
				return true;
			}
		}
		return false;
	}

	const std::string* storeText(QuestJournalSnapshot& snapshot, std::string text)
	{
		snapshot.textStorage.push_back(std::move(text));
		return std::addressof(snapshot.textStorage.back());
	}

	RE::TESForm* resolveQuestForm(const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			return RuntimeFormResolver::ResolveRawForm(*record.data.formID, record.pluginName);
		}
		return RuntimeFormResolver::ResolveEditorForm(record.data);
	}

	RuntimeQuestLogTranslations::LookupResult lookupByCandidatesInSnapshot(
		const QuestJournalSnapshot& snapshot,
		std::uint32_t runtimeQuestFormID,
		const std::uint32_t* indexes,
		std::size_t count)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			const auto it = snapshot.byIndex.find(QuestJournalKey{ runtimeQuestFormID, indexes[i] });
			if (it != snapshot.byIndex.end() && it->second)
			{
				return RuntimeQuestLogTranslations::LookupResult{ .text = it->second->c_str(), .index = indexes[i] };
			}
		}
		return {};
	}
}

namespace RuntimeQuestLogTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog)
	{
		std::scoped_lock lock{ g_rebuildLock };
		auto snapshot = std::make_shared<QuestJournalSnapshot>();
		snapshot->byIndex.reserve(catalog.records.size());
		snapshot->byStageItem.reserve(512);

		for (const auto& record : catalog.records)
		{
			const auto& data = record.data;
			if (data.translationType != TranslationType::kRuntimeLegacy || record.recordSignature != "QUST CNAM")
			{
				continue;
			}
			if (!hasText(data.replacerText))
			{
				continue;
			}

			auto* form = resolveQuestForm(record);
			if (!form)
			{
				continue;
			}
			if (!data.index)
			{
				continue;
			}

			const auto* text = storeText(*snapshot, data.replacerText);
			const auto runtimeQuestFormID = static_cast<std::uint32_t>(form->formID);
			snapshot->byIndex.insert_or_assign(QuestJournalKey{ runtimeQuestFormID, *data.index }, text);
		}

		g_snapshot.store(std::static_pointer_cast<const QuestJournalSnapshot>(snapshot), std::memory_order_release);
	}

	LookupResult LookupByCandidates(
		std::uint32_t runtimeQuestFormID,
		const std::uint32_t* indexes,
		std::size_t count)
	{
		if (runtimeQuestFormID == 0 || !indexes || count == 0)
		{
			return {};
		}
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot)
		{
			return {};
		}
		return lookupByCandidatesInSnapshot(*snapshot, runtimeQuestFormID, indexes, count);
	}

	LookupResult LookupByStageItem(
		std::uint32_t runtimeQuestFormID,
		std::uint16_t stageIndex,
		std::uint8_t itemIndex)
	{
		if (runtimeQuestFormID == 0)
		{
			return {};
		}
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot)
		{
			return {};
		}

		const QuestJournalStageItemKey key{ runtimeQuestFormID, stageIndex, itemIndex };
		{
			std::shared_lock lock{ snapshot->directLock };
			if (const auto found = snapshot->byStageItem.find(key); found != snapshot->byStageItem.end())
			{
				return found->second;
			}
		}

		const std::array<std::uint32_t, 3> candidates{
			static_cast<std::uint32_t>(stageIndex) + itemIndex,
			static_cast<std::uint32_t>(itemIndex),
			static_cast<std::uint32_t>(stageIndex)
		};
		const auto result = lookupByCandidatesInSnapshot(*snapshot, runtimeQuestFormID, candidates.data(), candidates.size());
		{
			std::unique_lock lock{ snapshot->directLock };
			const auto [it, inserted] = snapshot->byStageItem.try_emplace(key, result);
			return inserted ? result : it->second;
		}
	}
}
