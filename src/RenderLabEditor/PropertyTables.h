#pragma once

#include <typeinfo>
#include "Types.h"

class PropertyDef
{
public:
	PropertyDef(const char* name, const char* desc);
	virtual ~PropertyDef() {}
	
	const char* GetName() const;
	const char* GetDescription() const;

	virtual uint64 GetTypeId() const = 0;

private:
	const char* m_name;
	const char* m_description;
};

class BeginGroupPropertyDef : public PropertyDef
{
public:
	BeginGroupPropertyDef(const char* name);

	static const uint64 kTypeId;
	uint64 GetTypeId() const;
};

class EndGroupPropertyDef : public PropertyDef
{
public:
	EndGroupPropertyDef();

	static const uint64 kTypeId;
	uint64 GetTypeId() const;
};

class IntegerPropertyDef : public PropertyDef
{
public:
	IntegerPropertyDef(const char* name, const char* desc, int minVal, int maxVal);

	static const uint64 kTypeId;
	uint64 GetTypeId() const;

private:
	int m_minVal;
	int m_maxVal;
};

class FloatPropertyDef : public PropertyDef
{
public:
	typedef float (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(float newVal, void* pSource);

	FloatPropertyDef(const char* name, const char* desc, float minValue, float maxValue, float step, GetValueFunc getValueCallback, ChangedFunc changedCallback);

	static const uint64 kTypeId;
	uint64 GetTypeId() const;

	ChangedFunc GetChangedCallback() const;
	float GetMinValue() const;
	float GetMaxValue() const;
	float GetStep() const;

	float GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
	float m_minVal;
	float m_maxVal;
	float m_step;
};

class Vector3PropertyDef : public PropertyDef
{
public:
	typedef Vec3 (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(const Vec3& newVal, void* pSource);
	typedef bool (*ElemChangedFunc)(const float newVal, void* pSource);

	Vector3PropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback,
		ElemChangedFunc xChangedCallback, ElemChangedFunc yChangedCallback, ElemChangedFunc zChangedCallback);

	static const uint64 kTypeId;
	uint64 GetTypeId() const;

	ChangedFunc GetChangedCallback() const;
	ElemChangedFunc GetXChangedCallback() const;
	ElemChangedFunc GetYChangedCallback() const;
	ElemChangedFunc GetZChangedCallback() const;

	Vec3 GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
	ElemChangedFunc m_xChangedCallback;
	ElemChangedFunc m_yChangedCallback;
	ElemChangedFunc m_zChangedCallback;
};

class BooleanPropertyDef : public PropertyDef
{
public:
	typedef bool (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(bool newValue, void* pSource);

	BooleanPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback);

	static const uint64 kTypeId;
	uint64 GetTypeId() const;

	ChangedFunc GetChangedCallback() const;

	bool GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
};

class TextPropertyDef : public PropertyDef
{
public:
	typedef std::string (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(const std::string& newVal, void* pSource);

	TextPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback);

	static const uint64 kTypeId;
	uint64 GetTypeId() const;

	ChangedFunc GetChangedCallback() const;

	std::string GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
};

class ModelPropertyDef : public PropertyDef
{
public:
	typedef std::string (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(const std::string& newVal, void* pSource);

	ModelPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback);

	static const uint64 kTypeId;
	uint64 GetTypeId() const;

	ChangedFunc GetChangedCallback() const;

	std::string GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
};


//////////////////////////////////////////////////////////////////////////
// PropertyDef inlines
inline PropertyDef::PropertyDef(const char* name, const char* desc)
	: m_name(name)
	, m_description(desc)
{

}

inline const char* PropertyDef::GetName() const
{
	return m_name;
}

inline const char* PropertyDef::GetDescription() const
{
	return m_description;
}

//////////////////////////////////////////////////////////////////////////
// BeginGroupPropertyDef inlines
inline BeginGroupPropertyDef::BeginGroupPropertyDef(const char* name)
	: PropertyDef(name, nullptr)
{

}

inline uint64 BeginGroupPropertyDef::GetTypeId() const
{
	return kTypeId;
}

//////////////////////////////////////////////////////////////////////////
// EndGroupPropertyDef inlines
inline EndGroupPropertyDef::EndGroupPropertyDef()
	: PropertyDef(nullptr, nullptr)
{

}

inline uint64 EndGroupPropertyDef::GetTypeId() const
{
	return kTypeId;
}

//////////////////////////////////////////////////////////////////////////
// IntegerPropertyDef inlines
inline IntegerPropertyDef::IntegerPropertyDef(const char* name, const char* desc, int minVal, int maxVal)
	: PropertyDef(name, desc)
	, m_minVal(minVal)
	, m_maxVal(maxVal)
{

}

inline uint64 IntegerPropertyDef::GetTypeId() const
{
	return kTypeId;
}

//////////////////////////////////////////////////////////////////////////
// FloatPropertyDef inlines
inline FloatPropertyDef::FloatPropertyDef(const char* name, const char* desc, float minVal, float maxVal, float step, GetValueFunc getValueCallback, ChangedFunc changedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
	, m_minVal(minVal)
	, m_maxVal(maxVal)
	, m_step(step)
{

}

inline uint64 FloatPropertyDef::GetTypeId() const
{
	return kTypeId;
}

inline FloatPropertyDef::ChangedFunc FloatPropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline float FloatPropertyDef::GetMinValue() const
{
	return m_minVal;
}

inline float FloatPropertyDef::GetMaxValue() const
{
	return m_maxVal;
}

inline float FloatPropertyDef::GetStep() const
{
	return m_step;
}

inline float FloatPropertyDef::GetValue(void* pSource) const
{
	return m_getValueCallback(pSource);
}

//////////////////////////////////////////////////////////////////////////
// Vector3PropertyDef inlines
inline Vector3PropertyDef::Vector3PropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback,
	ElemChangedFunc xChangedCallback, ElemChangedFunc yChangedCallback, ElemChangedFunc zChangedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
	, m_xChangedCallback(xChangedCallback)
	, m_yChangedCallback(yChangedCallback)
	, m_zChangedCallback(zChangedCallback)
{

}

inline uint64 Vector3PropertyDef::GetTypeId() const
{
	return kTypeId;
}

inline Vector3PropertyDef::ChangedFunc Vector3PropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline Vector3PropertyDef::ElemChangedFunc Vector3PropertyDef::GetXChangedCallback() const
{
	return m_xChangedCallback;
}

inline Vector3PropertyDef::ElemChangedFunc Vector3PropertyDef::GetYChangedCallback() const
{
	return m_yChangedCallback;
}

inline Vector3PropertyDef::ElemChangedFunc Vector3PropertyDef::GetZChangedCallback() const
{
	return m_zChangedCallback;
}

inline Vec3 Vector3PropertyDef::GetValue(void* pSource) const
{
	return m_getValueCallback(pSource);
}

//////////////////////////////////////////////////////////////////////////
// BooleanPropertyDef inlines
inline BooleanPropertyDef::BooleanPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
{

}

inline uint64 BooleanPropertyDef::GetTypeId() const
{
	return kTypeId;
}

inline BooleanPropertyDef::ChangedFunc BooleanPropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline bool BooleanPropertyDef::GetValue(void* pSource) const
{
	return m_getValueCallback(pSource);
}

//////////////////////////////////////////////////////////////////////////
// TextPropertyDef inlines
inline TextPropertyDef::TextPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
{

}

inline uint64 TextPropertyDef::GetTypeId() const
{
	return kTypeId;
}

inline TextPropertyDef::ChangedFunc TextPropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline std::string TextPropertyDef::GetValue(void* pSource) const
{
	return m_getValueCallback(pSource);
}


//////////////////////////////////////////////////////////////////////////
// ModelPropertyDef inlines
inline ModelPropertyDef::ModelPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
{

}

inline uint64 ModelPropertyDef::GetTypeId() const
{
	return kTypeId;
}

inline ModelPropertyDef::ChangedFunc ModelPropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline std::string ModelPropertyDef::GetValue(void* pSource) const
{
	return m_getValueCallback(pSource);
}
