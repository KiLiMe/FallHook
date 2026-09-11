// AI CONTEXT: Performs low-level assignment into CommonLibF4 localized string storage.
// Depends on RuntimeTextStringAssign declarations and CommonLibF4 string types.
// Runtime scope is Fallout 4 1.10.163 mutable localized string fields.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: writes destination text only; no identity or lookup matching happens here.
#include "PCH.h"

#include "110163/RuntimeTextStringAssign.h"
#include "RuntimeDebugTest.h"

namespace RuntimeTextStringAssign
{
	void AssignLocalized(RE::BGSLocalizedString& target, std::string_view text)
	{
		if (RuntimeDebugTest::g_testAll.load(std::memory_order_acquire))
		{
			target = "樊";
			return;
		}
		target = text.empty() ? kEmptyText : text;
	}

	void AssignPlainLocalized(RE::BGSLocalizedString& target, std::string_view text)
	{
		auto& raw = reinterpret_cast<RE::BSFixedStringCS&>(target);
		if (RuntimeDebugTest::g_testAll.load(std::memory_order_acquire))
		{
			raw = "樊";
			return;
		}
		raw = text.empty() ? kEmptyText : text;
	}

	std::string NonEmptyString(std::string_view text)
	{
		if (RuntimeDebugTest::g_testAll.load(std::memory_order_acquire))
		{
			return "樊";
		}
		return text.empty() ? std::string{ kEmptyText } : std::string{ text };
	}
}