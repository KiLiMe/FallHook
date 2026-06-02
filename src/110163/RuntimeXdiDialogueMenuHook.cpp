// AI CONTEXT: Wraps XDI DialogueMenu GetDialogueOptions for 1.10.163 source-free option text replacement.
// Depends on F4SE Scaleform callbacks, RuntimeXdiDialogueOptionTranslations, RuntimeHookWatch, and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.10.163 DialogueMenu.swf only; XDI is optional and detected at runtime.
// Version-specific logic: installs a 1.10.163 GFx Value::ObjectInterface::SetMember hook at Address Library ID 1360149.
// Source-free policy: uses TESTopicInfo form identity and INFO RNAM/NAM1 maps; never matches visible/source text.
#include "PCH.h"

#include "110163/RuntimeXdiDialogueMenuHook.h"

#include "RuntimeApplySettings.h"
#include "RuntimeHookWatch.h"
#include "RuntimePrologueHook.h"
#include "110163/RuntimeXdiDialogueInfoResolver.h"
#include "110163/RuntimeXdiDialogueOptionResolver.h"

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

namespace
{
	using Value = Scaleform::GFx::Value;
	using ObjectInterface = Scaleform::GFx::Value::ObjectInterface;
	using GetArraySizeFunc = std::uint32_t(ObjectInterface*, void*);
	using GetElementFunc = bool(ObjectInterface*, void*, std::uint32_t, Value*);
	using GetMemberFunc = bool(ObjectInterface*, void*, const char*, Value*, bool);
	using InvokeFunc = bool(ObjectInterface*, void*, Value*, const char*, const Value*, std::size_t, bool);
	using ObjectReleaseFunc = void(ObjectInterface*, Value*, void*);
	using SetMemberFunc = bool(ObjectInterface*, void*, const char*, const Value&, bool);

	constexpr REL::ID kGetArraySizeID{ 254218 };
	constexpr REL::ID kGetElementID{ 827659 };
	constexpr REL::ID kGetMemberID{ 1517430 };
	constexpr REL::ID kInvokeID{ 655847 };
	constexpr REL::ID kObjectReleaseID{ 856221 };
	constexpr REL::ID kSetMemberID{ 1360149 };
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

	struct ValueAccess
	{
		ObjectInterface* objectInterface;
		REX::TEnumSet<Value::ValueType, std::int32_t> type;
		Value::ValueUnion value;
		std::size_t dataAux;
	};
	static_assert(sizeof(ValueAccess) == sizeof(Value));

	[[nodiscard]] ValueAccess& access(Value& value) noexcept
	{
		return reinterpret_cast<ValueAccess&>(value);
	}

	[[nodiscard]] const ValueAccess& access(const Value& value) noexcept
	{
		return reinterpret_cast<const ValueAccess&>(value);
	}

	[[nodiscard]] bool isManaged(const Value& value) noexcept
	{
		return (access(value).type.underlying() & static_cast<std::int32_t>(Value::ValueType::kManagedBit)) != 0;
	}

	void releaseValue(Value& value)
	{
		auto& raw = access(value);
		if (isManaged(value) && raw.objectInterface && raw.value.data && g_objectRelease)
		{
			g_objectRelease(raw.objectInterface, std::addressof(value), raw.value.data);
		}
		raw.objectInterface = nullptr;
		raw.type = Value::ValueType::kUndefined;
		raw.value.data = nullptr;
		raw.dataAux = 0;
	}

	[[nodiscard]] bool valueObject(const Value& value, ObjectInterface*& objectInterface, void*& data, bool& isDisplayObject)
	{
		if (!value.IsObject())
		{
			return false;
		}
		const auto& raw = access(value);
		objectInterface = raw.objectInterface;
		data = raw.value.data;
		isDisplayObject = value.IsDisplayObject();
		return objectInterface && data;
	}

	[[nodiscard]] bool getMember(const Value& object, const char* name, Value& out)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return valueObject(object, objectInterface, data, isDisplayObject) &&
			   g_getMember(objectInterface, data, name, std::addressof(out), isDisplayObject);
	}

	[[nodiscard]] bool setMember(const Value& object, const char* name, const Value& value)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return valueObject(object, objectInterface, data, isDisplayObject) &&
			   g_setMember(objectInterface, data, name, value, isDisplayObject);
	}

	[[nodiscard]] bool invoke(const Value& object, const char* name, Value* result, const Value* args, std::size_t argCount)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return valueObject(object, objectInterface, data, isDisplayObject) &&
			   g_invoke(objectInterface, data, result, name, args, argCount, isDisplayObject);
	}

	[[nodiscard]] std::uint32_t arraySize(const Value& object)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return valueObject(object, objectInterface, data, isDisplayObject) ? g_getArraySize(objectInterface, data) : 0;
	}

	[[nodiscard]] bool getElement(const Value& object, std::uint32_t index, Value& out)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return valueObject(object, objectInterface, data, isDisplayObject) &&
			   g_getElement(objectInterface, data, index, std::addressof(out));
	}

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
			Plugin::NAME,
			stage,
			index,
			option,
			info ? info->formID : 0,
			responseIndex,
			topic,
			quest,
			textLen);
	}

	[[nodiscard]] bool isDialogueMenu(Scaleform::GFx::Movie* movie)
	{
		Value path;
		const bool result = movie &&
							movie->GetVariable(std::addressof(path), "root.loaderInfo.url") &&
							path.IsString() &&
							std::strcmp(path.GetString(), kDialogueMenuPath) == 0;
		releaseValue(path);
		return result;
	}

	[[nodiscard]] std::optional<std::uint32_t> optionID(const Value& option)
	{
		Value value;
		if (!getMember(option, "optionID", value))
		{
			return std::nullopt;
		}
		std::optional<std::uint32_t> result;
		if (value.IsUInt())
		{
			result = value.GetUInt();
		}
		else if (value.IsInt() && value.GetInt() >= 0)
		{
			result = static_cast<std::uint32_t>(value.GetInt());
		}
		releaseValue(value);
		return result;
	}

	[[nodiscard]] bool isVanillaOptionLayout(const Value& result, std::uint32_t count)
	{
		if (count == 0 || count > kVanillaOrder.size())
		{
			return false;
		}
		std::size_t orderIndex = 0;
		for (std::uint32_t i = 0; i < count; ++i)
		{
			Value option;
			if (!getElement(result, i, option) || !option.IsObject())
			{
				releaseValue(option);
				return false;
			}
			const auto id = optionID(option);
			releaseValue(option);
			if (!id)
			{
				return false;
			}
			while (orderIndex < kVanillaOrder.size() && kVanillaOrder[orderIndex] != *id)
			{
				++orderIndex;
			}
			if (orderIndex == kVanillaOrder.size())
			{
				return false;
			}
			++orderIndex;
		}
		return true;
	}

	void traceMissOption(std::uint32_t index, std::uint32_t optionID, const RuntimeXdiDialogueOptionResolver::ResolvedOption& resolved)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed) ||
			g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} xdi-dialogue trace stage=miss-option index={} optionID={} candidate0={:08X} candidate1={:08X}",
			Plugin::NAME,
			index,
			optionID,
			resolved.candidate0,
			resolved.candidate1);
	}

	void patchDialogueOptions(Value& result)
	{
		if (!result.IsArray())
		{
			return;
		}
		const auto count = arraySize(result);
		const bool vanillaLayout = isVanillaOptionLayout(result, count);
		const auto activeInfos = vanillaLayout ? RuntimeXdiDialogueInfoResolver::InfoList{} :
												 RuntimeXdiDialogueInfoResolver::CollectActiveInfos();

		for (std::uint32_t i = 0; i < count; ++i)
		{
			Value option;
			if (!getElement(result, i, option) || !option.IsObject())
			{
				releaseValue(option);
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
				{
					trace("hit-active-option", i, resolved.info, resolved.text.prompt.size() + resolved.text.response.size(), optionValue, 0);
				}
				else
				{
					trace("miss-active-option", i, resolved.info, 0, optionValue, 0);
					resolved = RuntimeXdiDialogueOptionResolver::ResolveCapturedPrompt(*id);
					if (RuntimeXdiDialogueOptionResolver::HasText(resolved))
					{
						trace("hit-context-option", i, resolved.info, resolved.text.prompt.size() + resolved.text.response.size(), optionValue, 0);
					}
				}
			}
			else if (id)
			{
				resolved = RuntimeXdiDialogueOptionResolver::ResolveCapturedPrompt(*id);
				if (RuntimeXdiDialogueOptionResolver::HasText(resolved))
				{
					trace("hit-context-option", i, resolved.info, resolved.text.prompt.size() + resolved.text.response.size(), optionValue, 0);
				}
			}
			if (!RuntimeXdiDialogueOptionResolver::HasText(resolved))
			{
				traceMissOption(i, optionValue, resolved);
				releaseValue(option);
				continue;
			}
			if (resolved.text.hasPrompt && !resolved.text.prompt.empty())
			{
				(void)setMember(option, "prompt", Value{ resolved.text.prompt.c_str() });
				trace("hit-option-prompt", i, resolved.info, resolved.text.prompt.size(), optionValue, 0);
			}
			if (resolved.text.hasResponse && !resolved.text.response.empty())
			{
				(void)setMember(option, "response", Value{ resolved.text.response.c_str() });
				trace("hit-option-response", i, resolved.info, resolved.text.response.size(), optionValue, 0);
			}
			releaseValue(option);
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
			if (!params.self || !params.retVal || !invoke(*params.self, kOriginalName, params.retVal, params.args, params.argCount))
			{
				if (params.retVal)
				{
					releaseValue(*params.retVal);
				}
				return;
			}
			patchDialogueOptions(*params.retVal);
		}
	};

	[[nodiscard]] bool hasMarker(ObjectInterface* self, void* data, bool isDisplayObject)
	{
		Value marker;
		const bool result = self &&
							g_getMember(self, data, kMarkerName, std::addressof(marker), isDisplayObject) &&
							marker.IsBoolean() &&
							marker.GetBoolean();
		releaseValue(marker);
		return result;
	}

	void lateWrap(ObjectInterface* self, void* data, const Value& original, bool isDisplayObject)
	{
		if (!hasMarker(self, data, isDisplayObject))
		{
			return;
		}
		Value wrapper;
		if (!g_getMember(self, data, kWrapperName, std::addressof(wrapper), isDisplayObject) || wrapper.IsUndefined())
		{
			releaseValue(wrapper);
			return;
		}

		RuntimeHookWatch::ScopedCall watch{ g_setMemberWatch, "Scaleform Value::ObjectInterface::SetMember XDI wrap" };
		g_setMember(self, data, kOriginalName, original, isDisplayObject);
		g_setMember(self, data, kGetOptionsName, wrapper, isDisplayObject);
		trace("wrap-late", 0, nullptr, 0);
		releaseValue(wrapper);
	}

	bool setMemberThunk(ObjectInterface* self, void* data, const char* name, const Value& value, bool isDisplayObject)
	{
		const auto result = g_setMember(self, data, name, value, isDisplayObject);
		if (result && !g_internalSetMember && name && std::strcmp(name, kGetOptionsName) == 0)
		{
			lateWrap(self, data, value, isDisplayObject);
		}
		return result;
	}

	void wrapNow(Value& codeObj, Scaleform::GFx::Movie* movie)
	{
		Value existingOriginal;
		if (getMember(codeObj, kOriginalName, existingOriginal) && !existingOriginal.IsUndefined())
		{
			releaseValue(existingOriginal);
			return;
		}
		releaseValue(existingOriginal);

		Value wrapper;
		movie->CreateFunction(std::addressof(wrapper), new XdiGetDialogueOptionsHandler());

		ScopedInternalSetMember guard;
		(void)setMember(codeObj, kMarkerName, Value{ true });
		(void)setMember(codeObj, kWrapperName, wrapper);

		Value original;
		if (getMember(codeObj, kGetOptionsName, original) && !original.IsUndefined())
		{
			(void)setMember(codeObj, kOriginalName, original);
			(void)setMember(codeObj, kGetOptionsName, wrapper);
			trace("wrap-installed", 0, nullptr, 0);
		}
		else
		{
			trace("wrap-armed", 0, nullptr, 0);
		}
		releaseValue(original);
		releaseValue(wrapper);
	}

	bool F4SEAPI registerScaleform(Scaleform::GFx::Movie* movie, Scaleform::GFx::Value*)
	{
		if (!isDialogueMenu(movie))
		{
			return true;
		}
		Value codeObj;
		if (!movie->GetVariable(std::addressof(codeObj), kCodeObjPath) || !codeObj.IsObject())
		{
			trace("missing-codeobj", 0, nullptr, 0);
			releaseValue(codeObj);
			return true;
		}
		wrapNow(codeObj, movie);
		releaseValue(codeObj);
		return true;
	}

	void installSetMemberHook()
	{
		REL::Relocation<std::uintptr_t> target{ kSetMemberID };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(setMemberThunk),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped Scaleform Value::ObjectInterface::SetMember XDI hook; unsupported prologue bytes: {}", Plugin::NAME, result.prologueBytes);
			return;
		}

		g_setMember = reinterpret_cast<SetMemberFunc*>(result.original);
		g_getArraySize = REL::Relocation<GetArraySizeFunc*>{ kGetArraySizeID }.get();
		g_getElement = REL::Relocation<GetElementFunc*>{ kGetElementID }.get();
		g_getMember = REL::Relocation<GetMemberFunc*>{ kGetMemberID }.get();
		g_invoke = REL::Relocation<InvokeFunc*>{ kInvokeID }.get();
		g_objectRelease = REL::Relocation<ObjectReleaseFunc*>{ kObjectReleaseID }.get();
		REX::INFO("{} installed Scaleform Value::ObjectInterface::SetMember XDI hook at {:X} using Address Library ID {}.", Plugin::NAME, target.address(), kSetMemberID.id());
	}
}

namespace RuntimeXdiDialogueMenuHook
{
	void Install()
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}

		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		installSetMemberHook();

		const auto* scaleform = F4SE::GetScaleformInterface();
		if (!scaleform || !scaleform->Register("FallHookXDI", registerScaleform))
		{
			REX::ERROR("{} could not register XDI DialogueMenu Scaleform callback.", Plugin::NAME);
			return;
		}
		REX::INFO("{} registered XDI DialogueMenu source-free option wrapper.", Plugin::NAME);
	}
}
