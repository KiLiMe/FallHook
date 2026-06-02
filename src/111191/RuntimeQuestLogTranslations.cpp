// AI CONTEXT: Maintains optimized immutable-string lookup maps for the QUST:CNAM journal hook.
// Depends on RuntimeFormResolver and TranslationCatalog records built from XML/plugin identity.
// Runtime scope is Fallout 4 1.11.191 QUST:CNAM quest journal string/stage/item identity maps only.
// Version-specific logic: caches the 1.11.191 GetLogEntry identity tuple.
// Source-free policy: maps by runtime quest FormID plus string ID or stage item index; Source text is ignored.
#include "PCH.h"

#include "111191/RuntimeFormResolver.h"
#include "111191/RuntimeQuestLogTranslations.h"

#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace Runtime111191
{
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

	struct DirectKey
	{
		std::uint32_t questFormID{ 0 };
		std::uint32_t stringID{ 0 };
		std::uint32_t owningStage{ 0xFFFFFFFFu };
		std::uint32_t itemIndex{ 0xFFFFFFFFu };
		std::uint32_t currentStage{ 0xFFFFFFFFu };

		[[nodiscard]] bool operator==(const DirectKey&) const = default;
	};

	struct DirectKeyHash
	{
		[[nodiscard]] std::size_t operator()(const DirectKey& key) const noexcept
		{
			std::uint64_t value = key.questFormID;
			value = (value * 1315423911u) ^ key.stringID;
			value = (value * 1315423911u) ^ key.owningStage;
			value = (value * 1315423911u) ^ key.itemIndex;
			value = (value * 1315423911u) ^ key.currentStage;
			return std::hash<std::uint64_t>{}(value);
		}
	};

	using QuestJournalMap = std::unordered_map<QuestJournalKey, const std::string*, QuestJournalKeyHash>;
	using DirectMap = std::unordered_map<DirectKey, RuntimeQuestLogTranslations::LookupResult, DirectKeyHash>;

	struct QuestJournalSnapshot
	{
		std::deque<std::string> textStorage;
		QuestJournalMap byIndex;
		QuestJournalMap byStringID;
		mutable std::shared_mutex directLock;
		mutable DirectMap direct;
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

	bool addCandidate(std::array<std::uint32_t, 5>& candidates, std::size_t& count, std::uint32_t value)
	{
		for (std::size_t index = 0; index < count; ++index)
		{
			if (candidates[index] == value)
			{
				return false;
			}
		}
		if (count >= candidates.size())
		{
			return false;
		}
		candidates[count++] = value;
		return true;
	}

	RuntimeQuestLogTranslations::LookupResult lookupInSnapshot(
		const QuestJournalSnapshot& snapshot,
		std::uint32_t questID,
		std::uint32_t stringID,
		std::optional<std::uint32_t> owningStage,
		std::optional<std::uint32_t> itemIndex,
		std::uint32_t currentStage)
	{
		if (stringID != 0)
		{
			const auto found = snapshot.byStringID.find(QuestJournalKey{ questID, stringID });
			if (found != snapshot.byStringID.end() && found->second)
			{
				return { .text = found->second->c_str(), .mode = RuntimeQuestLogTranslations::LookupMode::kStringID };
			}
		}
		if (!itemIndex)
		{
			return {};
		}

		std::array<std::uint32_t, 5> candidates{};
		std::size_t count = 0;
		if (owningStage)
		{
			addCandidate(candidates, count, *owningStage + *itemIndex);
		}
		if (currentStage != 0xFFFFFFFFu)
		{
			addCandidate(candidates, count, currentStage + *itemIndex);
			addCandidate(candidates, count, currentStage);
		}
		addCandidate(candidates, count, *itemIndex);
		if (owningStage)
		{
			addCandidate(candidates, count, *owningStage);
		}

		for (std::size_t index = 0; index < count; ++index)
		{
			const auto found = snapshot.byIndex.find(QuestJournalKey{ questID, candidates[index] });
			if (found != snapshot.byIndex.end() && found->second)
			{
				return {
					.text = found->second->c_str(),
					.index = candidates[index],
					.mode = RuntimeQuestLogTranslations::LookupMode::kIndex
				};
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
		snapshot->byStringID.reserve(catalog.records.size());
		snapshot->direct.reserve(512);

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
			if (!form || (!data.index && !data.stringID))
			{
				continue;
			}

			const auto* text = storeText(*snapshot, data.replacerText);
			const auto questID = static_cast<std::uint32_t>(form->formID);
			if (data.index)
			{
				snapshot->byIndex.insert_or_assign(QuestJournalKey{ questID, *data.index }, text);
			}
			if (data.stringID)
			{
				snapshot->byStringID.insert_or_assign(QuestJournalKey{ questID, *data.stringID }, text);
			}
		}

		g_snapshot.store(std::static_pointer_cast<const QuestJournalSnapshot>(snapshot), std::memory_order_release);
	}

	LookupResult LookupByIdentity(
		std::uint32_t questID,
		std::uint32_t stringID,
		std::optional<std::uint32_t> owningStage,
		std::optional<std::uint32_t> itemIndex,
		std::uint32_t currentStage)
	{
		if (questID == 0)
		{
			return {};
		}
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot)
		{
			return {};
		}

		const DirectKey key{
			.questFormID = questID,
			.stringID = stringID,
			.owningStage = owningStage.value_or(0xFFFFFFFFu),
			.itemIndex = itemIndex.value_or(0xFFFFFFFFu),
			.currentStage = currentStage
		};
		{
			std::shared_lock lock{ snapshot->directLock };
			if (const auto found = snapshot->direct.find(key); found != snapshot->direct.end())
			{
				return found->second;
			}
		}

		const auto result = lookupInSnapshot(*snapshot, questID, stringID, owningStage, itemIndex, currentStage);
		{
			std::unique_lock lock{ snapshot->directLock };
			const auto [it, inserted] = snapshot->direct.try_emplace(key, result);
			return inserted ? result : it->second;
		}
	}
}

} // namespace Runtime111191
