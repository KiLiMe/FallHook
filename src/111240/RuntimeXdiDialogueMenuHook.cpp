// AI CONTEXT: Wraps XDI DialogueMenu GetDialogueOptions for 1.11.240 source-free option text replacement.
// Depends on F4SE Scaleform callbacks, RuntimeXdiDialogueOptionTranslations, RuntimeHookWatch, and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.11.240 DialogueMenu.swf only; XDI is optional and detected at runtime.
// Version-specific logic: installs 1.11.240 GFx Value::ObjectInterface hooks using provided Address Library IDs.
// Source-free policy: uses TESTopicInfo form identity and INFO RNAM/NAM1 maps; never matches visible/source text.
#include "PCH.h"

#include "111240/RuntimeXdiDialogueMenuHook.h"

#include "RuntimeApplySettings.h"
#include "RuntimeHookWatch.h"
#include "RuntimePrologueHook.h"
#include "111240/RuntimeXdiDialogueInfoResolver.h"
#include "111240/RuntimeXdiDialogueOptionResolver.h"
#include "111240/RuntimeXdiScaleformAccess.h"

#include "F4SE/API.h"
#include "F4SE/Interfaces.h"
#include "RE/T/TESTopicInfo.h"
#include "Scaleform/G/GFx_FunctionHandler.h"
#include "Scaleform/G/GFx_Movie.h"
#include "Scaleform/G/GFx_Value.h"

#include <array>
#include <atomic>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

namespace Runtime111240
{
namespace
{
	namespace SFA = RuntimeXdiScaleformAccess;
	using Value = Scaleform::GFx::Value;
	using ObjectInterface = Scaleform::GFx::Value::ObjectInterface;
	using GetArraySizeFunc = SFA::GetArraySizeFunc;
	using GetElementFunc = SFA::GetElementFunc;
	using GetMemberFunc = SFA::GetMemberFunc;
	using InvokeFunc = SFA::InvokeFunc;
	using ObjectReleaseFunc = SFA::ObjectReleaseFunc;
	using SetMemberFunc = SFA::SetMemberFunc;

	constexpr REL::ID kGetArraySizeID{ 2285791 };
	constexpr REL::ID kGetElementID{ 2285881 };
	constexpr REL::ID kGetMemberID{ 4494126 };
	constexpr REL::ID kInvokeID{ 2286101 };
	constexpr REL::ID kObjectReleaseID{ 2286229 };
	constexpr REL::ID kSetMemberID{ 2286589 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::uint32_t kTraceLimit{ 192 };
	constexpr std::array<std::uint32_t, 4> kVanillaOrder{ 3, 0, 1, 2 };
	constexpr char kDialogueMenuPath[] = "Interface/DialogueMenu.swf";
	constexpr char kCodeObjPath[] = "root.Menu_mc.BGSCodeObj";
	constexpr char kGetOptionsName[] = "GetDialogueOptions";
	constexpr char kOriginalName[] = "__FallHook_XDI_OriginalGetDialogueOptions";
	constexpr char kWrapperName[] = "__FallHook_XDI_WrapperGetDialogueOptions";
	constexpr char kMarkerName[] = "__FallHook_XDI_Target";

	GetArraySizeFunc* g_getArraySize{ nullptr };
	GetElementFunc* g_getElement{ nullptr };
	GetMemberFunc* g_getMember{ nullptr };
	InvokeFunc* g_invoke{ nullptr };
	ObjectReleaseFunc* g_objectRelease{ nullptr };
	SetMemberFunc* g_setMember{ nullptr };
	RuntimeHookWatch::Stats g_setMemberWatch;
	RuntimeHookWatch::Stats g_wrapperWatch;
	std::atomic_bool g_installed{ false };
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };
	thread_local bool g_internalSetMember{ false };

	struct ScopedInternalSetMember
	{
		ScopedInternalSetMember() { g_internalSetMember = true; }
		~ScopedInternalSetMember() { g_internalSetMember = false; }
	};

	void trace(
		std::string_view stage,
		std::uint32_t index,
		const RE::TESTopicInfo* info,
		std::size_t textLen,
		std::uint32_t option = 0xFFFFFFFFu,
		std::uint32_t responseIndex = 0xFFFFFFFFu,
		std::uint32_t topic = 0,
		std::uint32_t quest = 0)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed) ||
			g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} xdi-dialogue trace stage={} index={} optionID={} info={:08X} responseIndex={} topic={:08X} quest={:08X} textLen={}",
			Plugin::NAME, stage, index, option, info ? info->formID : 0,
			responseIndex, topic, quest, textLen);
	}

	[[nodiscard]] bool isDialogueMenu(Scaleform::GFx::Movie* movie)
	{
		Value path;
		const bool result = movie &&
							movie->GetVariable(&path, "root.loaderInfo.url") &&
							path.IsString() &&
							std::strcmp(path.GetString(), kDialogueMenuPath) == 0;
		SFA::ReleaseValue(path, g_objectRelease);
		return result;
	}

	[[nodiscard]] std::optional<std::uint32_t> optionID(const Value& option)
	{
		Value value;
		if (!SFA::GetMember(g_getMember, option, "optionID", value))
		{
			return std::nullopt;
		}
		std::optional<std::uint32_t> result;
		if (value.IsUInt())
			result = value.GetUInt();
		else if (value.IsInt() && value.GetInt() >= 0)
			result = static_cast<std::uint32_t>(value.GetInt());
		SFA::ReleaseValue(value, g_objectRelease);
		return result;
	}

	[[nodiscard]] bool isVanillaOptionLayout(const Value& result, std::uint32_t count)
	{
		if (count == 0 || count > kVanillaOrder.size())
			return false;
		std::size_t orderIndex = 0;
		for (std::uint32_t i = 0; i < count; ++i)
		{
			Value option;
			if (!SFA::GetElement(g_getElement, result, i, option) || !option.IsObject())
			{
				SFA::ReleaseValue(option, g_objectRelease);
				return false;
			}
			const auto id = optionID(option);
			SFA::ReleaseValue(option, g_objectRelease);
			if (!id) return false;
			while (orderIndex < kVanillaOrder.size() && kVanillaOrder[orderIndex] != *id)
				++orderIndex;
			if (orderIndex == kVanillaOrder.size())
				return false;
			++orderIndex;
		}
		return true;
	}

	void traceMissOption(std::uint32_t index, std::uint32_t optionID, const RuntimeXdiDialogueOptionResolver::ResolvedOption& resolved)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed) ||
			g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
			return;
		REX::INFO(
			"{} xdi-dialogue trace stage=miss-option index={} optionID={} candidate0={:08X} candidate1={:08X}",
			Plugin::NAME, index, optionID, resolved.candidate0, resolved.candidate1);
	}

	void patchDialogueOptions(Value& result)
	{
		if (!result.IsArray()) return;
		const auto count = SFA::ArraySize(g_getArraySize, result);
		const bool vanillaLayout = isVanillaOptionLayout(result, count);
		const auto activeInfos = vanillaLayout ? RuntimeXdiDialogueInfoResolver::InfoList{} :
												 RuntimeXdiDialogueInfoResolver::CollectActiveInfos();
		for (std::uint32_t i = 0; i < count; ++i)
		{
			Value option;
			if (!SFA::GetElement(g_getElement, result, i, option) || !option.IsObject())
			{
				SFA::ReleaseValue(option, g_objectRelease);
				continue;
			}
			const auto id = optionID(option);
			const auto optionValue = id.value_or(0xFFFFFFFFu);
			RuntimeXdiDialogueOptionResolver::ResolvedOption resolved;
			if (id && !vanillaLayout)
			{
				resolved = RuntimeXdiDialogueOptionResolver::ResolveInfo(
					RuntimeXdiDialogueInfoResolver::ResolveActiveInfo(activeInfos, *id));
				if (RuntimeXdiDialogueOptionResolver::HasText(resolved))
					trace("hit-active-option", i, resolved.info, resolved.text.prompt.size() + resolved.text.response.size(), optionValue, 0);
				else
				{
					trace("miss-active-option", i, resolved.info, 0, optionValue, 0);
					resolved = RuntimeXdiDialogueOptionResolver::ResolveCapturedPrompt(*id);
					if (RuntimeXdiDialogueOptionResolver::HasText(resolved))
						trace("hit-context-option", i, resolved.info, resolved.text.prompt.size() + resolved.text.response.size(), optionValue, 0);
				}
			}
			else if (id)
			{
				resolved = RuntimeXdiDialogueOptionResolver::ResolveCapturedPrompt(*id);
				if (RuntimeXdiDialogueOptionResolver::HasText(resolved))
					trace("hit-context-option", i, resolved.info, resolved.text.prompt.size() + resolved.text.response.size(), optionValue, 0);
			}
			if (!RuntimeXdiDialogueOptionResolver::HasText(resolved))
			{
				traceMissOption(i, optionValue, resolved);
				SFA::ReleaseValue(option, g_objectRelease);
				continue;
			}
			if (resolved.text.hasPrompt && !resolved.text.prompt.empty())
			{
				(void)SFA::SetMember(g_setMember, option, "prompt", Value{ resolved.text.prompt.c_str() });
				trace("hit-option-prompt", i, resolved.info, resolved.text.prompt.size(), optionValue, 0);
			}
			if (resolved.text.hasResponse && !resolved.text.response.empty())
			{
				(void)SFA::SetMember(g_setMember, option, "response", Value{ resolved.text.response.c_str() });
				trace("hit-option-response", i, resolved.info, resolved.text.response.size(), optionValue, 0);
			}
			SFA::ReleaseValue(option, g_objectRelease);
		}
	}

	class XdiGetDialogueOptionsHandler final : public Scaleform::GFx::FunctionHandler
	{
	public:
		static void* operator new(std::size_t size) { return ::operator new(size); }
		static void operator delete(void* ptr) noexcept { ::operator delete(ptr); }
		static void operator delete(void* ptr, std::size_t) noexcept { ::operator delete(ptr); }

		void Call(const Params& params) override
		{
			RuntimeHookWatch::ScopedCall watch{ g_wrapperWatch, "XDI GetDialogueOptions wrapper" };
			if (!params.self || !params.retVal ||
				!SFA::Invoke(g_invoke, *params.self, kOriginalName, params.retVal, params.args, params.argCount))
			{
				if (params.retVal)
					SFA::ReleaseValue(*params.retVal, g_objectRelease);
				return;
			}
			patchDialogueOptions(*params.retVal);
		}
	};

	[[nodiscard]] bool hasMarker(ObjectInterface* self, void* data, bool isDisplayObject)
	{
		Value marker;
		const bool result = self &&
							g_getMember(self, data, kMarkerName, &marker, isDisplayObject) &&
							marker.IsBoolean() && marker.GetBoolean();
		SFA::ReleaseValue(marker, g_objectRelease);
		return result;
	}

	void lateWrap(ObjectInterface* self, void* data, const Value& original, bool isDisplayObject)
	{
		if (!hasMarker(self, data, isDisplayObject))
			return;
		Value wrapper;
		if (!g_getMember(self, data, kWrapperName, &wrapper, isDisplayObject) || wrapper.IsUndefined())
		{
			SFA::ReleaseValue(wrapper, g_objectRelease);
			return;
		}
		RuntimeHookWatch::ScopedCall watch{ g_setMemberWatch, "Scaleform Value::ObjectInterface::SetMember XDI wrap" };
		g_setMember(self, data, kOriginalName, original, isDisplayObject);
		g_setMember(self, data, kGetOptionsName, wrapper, isDisplayObject);
		trace("wrap-late", 0, nullptr, 0);
		SFA::ReleaseValue(wrapper, g_objectRelease);
	}

	bool setMemberThunk(ObjectInterface* self, void* data, const char* name, const Value& value, bool isDisplayObject)
	{
		const auto result = g_setMember(self, data, name, value, isDisplayObject);
		if (result && !g_internalSetMember && name && std::strcmp(name, kGetOptionsName) == 0)
			lateWrap(self, data, value, isDisplayObject);
		return result;
	}

	void wrapNow(Value& codeObj, Scaleform::GFx::Movie* movie)
	{
		Value existingOriginal;
		if (SFA::GetMember(g_getMember, codeObj, kOriginalName, existingOriginal) && !existingOriginal.IsUndefined())
		{
			SFA::ReleaseValue(existingOriginal, g_objectRelease);
			return;
		}
		SFA::ReleaseValue(existingOriginal, g_objectRelease);

		Value wrapper;
		movie->CreateFunction(&wrapper, new XdiGetDialogueOptionsHandler());

		ScopedInternalSetMember guard;
		(void)SFA::SetMember(g_setMember, codeObj, kMarkerName, Value{ true });
		(void)SFA::SetMember(g_setMember, codeObj, kWrapperName, wrapper);

		Value original;
		if (SFA::GetMember(g_getMember, codeObj, kGetOptionsName, original) && !original.IsUndefined())
		{
			(void)SFA::SetMember(g_setMember, codeObj, kOriginalName, original);
			(void)SFA::SetMember(g_setMember, codeObj, kGetOptionsName, wrapper);
			trace("wrap-installed", 0, nullptr, 0);
		}
		else
		{
			trace("wrap-armed", 0, nullptr, 0);
		}
		SFA::ReleaseValue(original, g_objectRelease);
		SFA::ReleaseValue(wrapper, g_objectRelease);
	}

	bool F4SEAPI registerScaleform(Scaleform::GFx::Movie* movie, Scaleform::GFx::Value*)
	{
		if (!isDialogueMenu(movie))
			return true;
		Value codeObj;
		if (!movie->GetVariable(&codeObj, kCodeObjPath) || !codeObj.IsObject())
		{
			trace("missing-codeobj", 0, nullptr, 0);
			SFA::ReleaseValue(codeObj, g_objectRelease);
			return true;
		}
		wrapNow(codeObj, movie);
		SFA::ReleaseValue(codeObj, g_objectRelease);
		return true;
	}

	bool installSetMemberHook()
	{
		REL::Relocation<std::uintptr_t> target{ kSetMemberID };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(), reinterpret_cast<std::uintptr_t>(setMemberThunk), kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped Scaleform Value::ObjectInterface::SetMember XDI hook; unsupported prologue bytes: {}",
				Plugin::NAME, result.prologueBytes);
			return false;
		}
		g_setMember = reinterpret_cast<SetMemberFunc*>(result.original);
		g_getArraySize = REL::Relocation<GetArraySizeFunc*>{ kGetArraySizeID }.get();
		g_getElement = REL::Relocation<GetElementFunc*>{ kGetElementID }.get();
		g_getMember = REL::Relocation<GetMemberFunc*>{ kGetMemberID }.get();
		g_invoke = REL::Relocation<InvokeFunc*>{ kInvokeID }.get();
		g_objectRelease = REL::Relocation<ObjectReleaseFunc*>{ kObjectReleaseID }.get();
		REX::INFO("{} installed Scaleform Value::ObjectInterface::SetMember XDI hook at {:X} using Address Library ID {}.",
			Plugin::NAME, target.address(), kSetMemberID.id());
		return true;
	}
}

namespace RuntimeXdiDialogueMenuHook
{
	void Install()
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
			return;
		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		if (!installSetMemberHook()) return;
		const auto* scaleform = F4SE::GetScaleformInterface();
		if (!scaleform || !scaleform->Register("FallHookXDI", registerScaleform))
		{
			REX::ERROR("{} could not register XDI DialogueMenu Scaleform callback.", Plugin::NAME);
			return;
		}
		REX::INFO("{} registered XDI DialogueMenu source-free option wrapper.", Plugin::NAME);
	}
}

} // namespace Runtime111240
