#pragma once

class PropertyDef;

class IViewModel
{
public:
	template<typename ObjectT>
	static IViewModel* Create(ObjectT* pObject);
	static IViewModel* Create(const std::type_info& typeId, void* pTypeData);

	virtual ~IViewModel() {};

	virtual const char* GetTypeName() = 0;
	virtual const PropertyDef** GetProperties() = 0;
};

template<typename ObjectT>
IViewModel* IViewModel::Create(ObjectT* pObject)
{
	return Create(typeid(*pObject), pObject);
}