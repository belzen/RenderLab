#pragma once

#include "Types.h"
#include "Widgets/Widget.h"

class PropertyDef
{
public:
	PropertyDef(const char* name, const char* desc);
	virtual ~PropertyDef() {}
	
	const char* GetName() const;
	const char* GetDescription() const;

private:
	const char* m_name;
	const char* m_description;
};

class BeginGroupPropertyDef : public PropertyDef
{
public:
	BeginGroupPropertyDef(const char* name);
};

class EndGroupPropertyDef : public PropertyDef
{
public:
	EndGroupPropertyDef();
};

class IntegerPropertyDef : public PropertyDef
{
public:
	IntegerPropertyDef(const char* name, const char* desc, int minVal, int maxVal);

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

class IntChoicePropertyDef : public PropertyDef
{
public:
	typedef int (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(int newValue, void* pSource);

	IntChoicePropertyDef(const char* name, const char* desc, const NameValuePair* aOptions, int numOptions, GetValueFunc getValueCallback, ChangedFunc changedCallback);

	ChangedFunc GetChangedCallback() const;
	const NameValuePair* GetOptions() const;
	int GetNumOptions() const;

	int GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
	const NameValuePair* m_aOptions;
	int m_numOptions;
};

class Vector3PropertyDef : public PropertyDef
{
public:
	typedef Vec3 (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(const Vec3& newVal, void* pSource);
	typedef bool (*ElemChangedFunc)(const float newVal, void* pSource);

	Vector3PropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback,
		ElemChangedFunc xChangedCallback, ElemChangedFunc yChangedCallback, ElemChangedFunc zChangedCallback);

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

class RotationPropertyDef : public PropertyDef
{
public:
	typedef Rotation (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(const Rotation& newVal, void* pSource);
	typedef bool (*ElemChangedFunc)(const float newVal, void* pSource);

	RotationPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback,
		ElemChangedFunc pitchChangedCallback, ElemChangedFunc yawChangedCallback, ElemChangedFunc rollChangedCallback);

	ChangedFunc GetChangedCallback() const;
	ElemChangedFunc GetPitchChangedCallback() const;
	ElemChangedFunc GetYawChangedCallback() const;
	ElemChangedFunc GetRollChangedCallback() const;

	Rotation GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
	ElemChangedFunc m_pitchChangedCallback;
	ElemChangedFunc m_yawChangedCallback;
	ElemChangedFunc m_rollChangedCallback;
};

class BooleanPropertyDef : public PropertyDef
{
public:
	typedef bool (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(bool newValue, void* pSource);

	BooleanPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback);

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

	ChangedFunc GetChangedCallback() const;

	std::string GetValue(void* pSource) const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
};

class AssetPropertyDef : public PropertyDef
{
public:
	typedef std::string (*GetValueFunc)(void* pSource);
	typedef bool (*ChangedFunc)(const std::string& newVal, void* pSource);

	AssetPropertyDef(const char* name, const char* desc, WidgetDragDataType assetType, GetValueFunc getValueCallback, ChangedFunc changedCallback);

	ChangedFunc GetChangedCallback() const;

	std::string GetValue(void* pSource) const;

	WidgetDragDataType GetAssetType() const;

private:
	GetValueFunc m_getValueCallback;
	ChangedFunc m_changedCallback;
	WidgetDragDataType m_assetType; // TODO: Replace this with an actual asset identifier type
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

//////////////////////////////////////////////////////////////////////////
// EndGroupPropertyDef inlines
inline EndGroupPropertyDef::EndGroupPropertyDef()
	: PropertyDef(nullptr, nullptr)
{

}

//////////////////////////////////////////////////////////////////////////
// IntegerPropertyDef inlines
inline IntegerPropertyDef::IntegerPropertyDef(const char* name, const char* desc, int minVal, int maxVal)
	: PropertyDef(name, desc)
	, m_minVal(minVal)
	, m_maxVal(maxVal)
{

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
// IntChoicePropertyDef inlines
inline IntChoicePropertyDef::IntChoicePropertyDef(const char* name, const char* desc, const NameValuePair* aOptions, int numOptions,
	GetValueFunc getValueCallback, ChangedFunc changedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
	, m_aOptions(aOptions)
	, m_numOptions(numOptions)
{

}

inline IntChoicePropertyDef::ChangedFunc IntChoicePropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline const NameValuePair* IntChoicePropertyDef::GetOptions() const
{
	return m_aOptions;
}

inline int IntChoicePropertyDef::GetNumOptions() const
{
	return m_numOptions;
}

inline int IntChoicePropertyDef::GetValue(void* pSource) const
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
// RotationPropertyDef inlines
inline RotationPropertyDef::RotationPropertyDef(const char* name, const char* desc, GetValueFunc getValueCallback, ChangedFunc changedCallback,
	ElemChangedFunc pitchChangedCallback, ElemChangedFunc yawChangedCallback, ElemChangedFunc rollChangedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
	, m_pitchChangedCallback(pitchChangedCallback)
	, m_yawChangedCallback(yawChangedCallback)
	, m_rollChangedCallback(rollChangedCallback)
{

}

inline RotationPropertyDef::ChangedFunc RotationPropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline RotationPropertyDef::ElemChangedFunc RotationPropertyDef::GetPitchChangedCallback() const
{
	return m_pitchChangedCallback;
}

inline RotationPropertyDef::ElemChangedFunc RotationPropertyDef::GetYawChangedCallback() const
{
	return m_yawChangedCallback;
}

inline RotationPropertyDef::ElemChangedFunc RotationPropertyDef::GetRollChangedCallback() const
{
	return m_rollChangedCallback;
}

inline Rotation RotationPropertyDef::GetValue(void* pSource) const
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

inline TextPropertyDef::ChangedFunc TextPropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline std::string TextPropertyDef::GetValue(void* pSource) const
{
	return m_getValueCallback(pSource);
}


//////////////////////////////////////////////////////////////////////////
// AssetPropertyDef inlines
inline AssetPropertyDef::AssetPropertyDef(const char* name, const char* desc, WidgetDragDataType assetType, GetValueFunc getValueCallback, ChangedFunc changedCallback)
	: PropertyDef(name, desc)
	, m_getValueCallback(getValueCallback)
	, m_changedCallback(changedCallback)
	, m_assetType(assetType)
{

}

inline AssetPropertyDef::ChangedFunc AssetPropertyDef::GetChangedCallback() const
{
	return m_changedCallback;
}

inline std::string AssetPropertyDef::GetValue(void* pSource) const
{
	return m_getValueCallback(pSource);
}

inline WidgetDragDataType AssetPropertyDef::GetAssetType() const
{
	return m_assetType;
}
