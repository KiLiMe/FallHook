// AI CONTEXT: Scaleform GFx::Value raw access helpers shared by the 1.11.191 XDI DialogueMenu hook.
// Depends on Scaleform GFx_Value.h ObjectInterface layout and CommonLibF4 REX enum helpers.
// Runtime scope is Fallout 4 1.11.191 XDI DialogueMenu.swf Value introspection only.
// Version-specific logic: reads Scaleform::GFx::Value internals via ValueAccess union; assumes 1.11.191 layout.
// Source-free policy: this module handles raw Value/object access only; no translation identity or text matching.
#pragma once

#include <cstdint>
#include <cstddef>

namespace Scaleform::GFx
{
	class Value;
}

namespace Runtime111191::RuntimeXdiScaleformAccess
{
	using Value = Scaleform::GFx::Value;
	using ObjectInterface = Scaleform::GFx::Value::ObjectInterface;
	using GetArraySizeFunc = std::uint32_t(ObjectInterface*, void*);
	using GetElementFunc = bool(ObjectInterface*, void*, std::uint32_t, Value*);
	using GetMemberFunc = bool(ObjectInterface*, void*, const char*, Value*, bool);
	using InvokeFunc = bool(ObjectInterface*, void*, Value*, const char*, const Value*, std::size_t, bool);
	using ObjectReleaseFunc = void(ObjectInterface*, Value*, void*);
	using SetMemberFunc = bool(ObjectInterface*, void*, const char*, const Value&, bool);

	struct ValueAccess
	{
		ObjectInterface* objectInterface;
		REX::TEnumSet<Value::ValueType, std::int32_t> type;
		Value::ValueUnion value;
		std::size_t dataAux;
	};
	static_assert(sizeof(ValueAccess) == sizeof(Value));

	[[nodiscard]] inline ValueAccess& Access(Value& value) noexcept
	{
		return reinterpret_cast<ValueAccess&>(value);
	}

	[[nodiscard]] inline const ValueAccess& Access(const Value& value) noexcept
	{
		return reinterpret_cast<const ValueAccess&>(value);
	}

	[[nodiscard]] inline bool IsManaged(const Value& value) noexcept
	{
		return (Access(value).type.underlying() & static_cast<std::int32_t>(Value::ValueType::kManagedBit)) != 0;
	}

	inline void ReleaseValue(Value& value, ObjectReleaseFunc* objectRelease)
	{
		auto& raw = Access(value);
		if (IsManaged(value) && raw.objectInterface && raw.value.data && objectRelease)
		{
			objectRelease(raw.objectInterface, &value, raw.value.data);
		}
		raw.objectInterface = nullptr;
		raw.type = Value::ValueType::kUndefined;
		raw.value.data = nullptr;
		raw.dataAux = 0;
	}

	[[nodiscard]] inline bool ValueObject(const Value& value, ObjectInterface*& objectInterface, void*& data, bool& isDisplayObject)
	{
		if (!value.IsObject())
		{
			return false;
		}
		const auto& raw = Access(value);
		objectInterface = raw.objectInterface;
		data = raw.value.data;
		isDisplayObject = value.IsDisplayObject();
		return objectInterface && data;
	}

	[[nodiscard]] inline bool GetMember(GetMemberFunc* getMemberFn, const Value& object, const char* name, Value& out)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return getMemberFn && ValueObject(object, objectInterface, data, isDisplayObject) &&
			   getMemberFn(objectInterface, data, name, &out, isDisplayObject);
	}

	[[nodiscard]] inline bool SetMember(SetMemberFunc* setMemberFn, const Value& object, const char* name, const Value& value)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return setMemberFn && ValueObject(object, objectInterface, data, isDisplayObject) &&
			   setMemberFn(objectInterface, data, name, value, isDisplayObject);
	}

	[[nodiscard]] inline bool Invoke(InvokeFunc* invokeFn, const Value& object, const char* name, Value* result, const Value* args, std::size_t argCount)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return invokeFn && ValueObject(object, objectInterface, data, isDisplayObject) &&
			   invokeFn(objectInterface, data, result, name, args, argCount, isDisplayObject);
	}

	[[nodiscard]] inline std::uint32_t ArraySize(GetArraySizeFunc* getArraySizeFn, const Value& object)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return (getArraySizeFn && ValueObject(object, objectInterface, data, isDisplayObject)) ? getArraySizeFn(objectInterface, data) : 0;
	}

	[[nodiscard]] inline bool GetElement(GetElementFunc* getElementFn, const Value& object, std::uint32_t index, Value& out)
	{
		ObjectInterface* objectInterface{ nullptr };
		void* data{ nullptr };
		bool isDisplayObject{ false };
		return getElementFn && ValueObject(object, objectInterface, data, isDisplayObject) &&
			   getElementFn(objectInterface, data, index, &out);
	}
}
