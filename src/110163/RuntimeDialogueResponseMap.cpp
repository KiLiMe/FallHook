// AI CONTEXT: Implements source-free INFO:NAM1 response target map lookups.
// Depends on RuntimeDialogueResponseMap and CommonLibF4 TESTopicInfo form identity.
// Runtime scope is Fallout 4 1.10.163 dialogue response resolver support.
// Version-specific logic: consumes 110163 candidate lists but contains no hook addresses.
// Source-free policy: never reads Source or visible text; only stable response identity keys are used.
#include "PCH.h"

#include "110163/RuntimeDialogueResponseMap.h"

#include "RE/T/TESTopicInfo.h"

namespace RuntimeDialogueResponseMap
{
	Target MakeTarget(std::string text)
	{
		Target target;
		target.text = std::move(text);
		target.fixedText = target.text;
		return target;
	}

	void AddUniqueSID(Snapshot& snapshot, std::uint32_t sid, const Target& target)
	{
		if (sid == 0)
		{
			return;
		}
		if (auto found = snapshot.byUniqueSID.find(sid); found != snapshot.byUniqueSID.end())
		{
			if (found->second.text != target.text)
			{
				found->second = {};
			}
			return;
		}
		snapshot.byUniqueSID.emplace(sid, target);
	}

	void AddDefaultTarget(Targets& targets, const Target& target)
	{
		if (!targets.hasDefault)
		{
			targets.defaultTarget = target;
			targets.hasDefault = true;
			return;
		}
		if (targets.defaultTarget.text != target.text)
		{
			targets.defaultAmbiguous = true;
			targets.defaultTarget = {};
		}
	}

	void AddResponseIDTarget(Targets& targets, std::uint32_t id, const Target& target)
	{
		if (id == 0)
		{
			return;
		}
		const auto [found, inserted] = targets.byResponseID.emplace(id, target);
		if (!inserted && found->second.text != target.text)
		{
			found->second = {};
		}
	}

	void AddIndexCandidate(std::array<std::uint32_t, 5>& indexes, std::size_t& count, std::uint32_t value)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			if (indexes[i] == value)
			{
				return;
			}
		}
		if (count < indexes.size())
		{
			indexes[count++] = value;
		}
	}

	std::array<std::uint32_t, 5> IndexCandidates(std::uint32_t ordinal, std::uint32_t id, std::size_t& count)
	{
		std::array<std::uint32_t, 5> indexes{};
		if (ordinal != 0xFFFFFFFFu)
		{
			AddIndexCandidate(indexes, count, ordinal);
		}
		if (id > 0)
		{
			AddIndexCandidate(indexes, count, id - 1);
			AddIndexCandidate(indexes, count, id);
		}
		if (id > 1)
		{
			AddIndexCandidate(indexes, count, id - 2);
		}
		return indexes;
	}

	const Target* LookupBySID(
		const Snapshot& snapshot,
		const RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates,
		std::uint32_t sid)
	{
		for (std::size_t i = 0; i < candidates.count; ++i)
		{
			const auto* candidate = candidates.values[i];
			const auto foundTargets = candidate ? snapshot.byInfoForm.find(candidate->formID) : snapshot.byInfoForm.end();
			if (foundTargets == snapshot.byInfoForm.end())
			{
				continue;
			}
			if (const auto found = foundTargets->second.bySID.find(sid); found != foundTargets->second.bySID.end())
			{
				return std::addressof(found->second);
			}
		}
		return nullptr;
	}

	const Target* LookupFormSID(const Snapshot& snapshot, std::uint32_t formID, std::uint32_t sid)
	{
		const auto targets = snapshot.byInfoForm.find(formID);
		if (targets == snapshot.byInfoForm.end())
		{
			return nullptr;
		}
		const auto found = targets->second.bySID.find(sid);
		return found != targets->second.bySID.end() ? std::addressof(found->second) : nullptr;
	}

	const Target* LookupInfoSID(const Snapshot& snapshot, const RE::TESTopicInfo* info, std::uint32_t sid)
	{
		return info ? LookupFormSID(snapshot, info->formID, sid) : nullptr;
	}

	const Target* LookupInfoDefault(const Snapshot& snapshot, const RE::TESTopicInfo* info)
	{
		const auto targets = info ? snapshot.byInfoForm.find(info->formID) : snapshot.byInfoForm.end();
		if (targets == snapshot.byInfoForm.end() || !targets->second.hasDefault || targets->second.defaultAmbiguous)
		{
			return nullptr;
		}
		return std::addressof(targets->second.defaultTarget);
	}

	const Target* LookupByResponseID(
		const Snapshot& snapshot,
		const RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates,
		std::uint32_t id)
	{
		const Target* result = nullptr;
		for (std::size_t i = 0; i < candidates.count; ++i)
		{
			const auto* candidate = candidates.values[i];
			const auto targets = candidate ? snapshot.byInfoForm.find(candidate->formID) : snapshot.byInfoForm.end();
			if (targets == snapshot.byInfoForm.end())
			{
				continue;
			}
			const auto found = targets->second.byResponseID.find(id);
			if (found == targets->second.byResponseID.end() || found->second.text.empty())
			{
				continue;
			}
			if (result && result->text != found->second.text)
			{
				return nullptr;
			}
			result = std::addressof(found->second);
		}
		return result;
	}

	const Target* LookupInfoIndex(const Snapshot& snapshot, const RE::TESTopicInfo* info, std::uint32_t index)
	{
		const auto targets = info ? snapshot.byInfoForm.find(info->formID) : snapshot.byInfoForm.end();
		if (targets == snapshot.byInfoForm.end())
		{
			return nullptr;
		}
		const auto found = targets->second.byIndex.find(index);
		return found != targets->second.byIndex.end() ? std::addressof(found->second) : nullptr;
	}

	const Target* LookupInfoResponseID(const Snapshot& snapshot, const RE::TESTopicInfo* info, std::uint32_t id)
	{
		const auto targets = info ? snapshot.byInfoForm.find(info->formID) : snapshot.byInfoForm.end();
		if (targets == snapshot.byInfoForm.end())
		{
			return nullptr;
		}
		const auto found = targets->second.byResponseID.find(id);
		return found != targets->second.byResponseID.end() && !found->second.text.empty() ? std::addressof(found->second) : nullptr;
	}

	const Target* LookupByIndex(
		const Snapshot& snapshot,
		const RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates,
		const std::array<std::uint32_t, 5>& indexes,
		std::size_t count)
	{
		for (std::size_t candidateIndex = 0; candidateIndex < candidates.count; ++candidateIndex)
		{
			const auto* candidate = candidates.values[candidateIndex];
			const auto targets = candidate ? snapshot.byInfoForm.find(candidate->formID) : snapshot.byInfoForm.end();
			if (targets == snapshot.byInfoForm.end())
			{
				continue;
			}
			for (std::size_t i = 0; i < count; ++i)
			{
				if (const auto found = targets->second.byIndex.find(indexes[i]); found != targets->second.byIndex.end())
				{
					return std::addressof(found->second);
				}
			}
		}
		return nullptr;
	}
}
