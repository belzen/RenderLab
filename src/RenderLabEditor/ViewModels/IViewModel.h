#pragma once

class PropertyDef;

class IViewModel
{
public:
	template<typename ObjectT>
	static IViewModel* Create(ObjectT* pObject);
	static IViewModel* Create(uint64 typeId, void* pTypeData);

	virtual ~IViewModel() {};

	virtual const PropertyDef** GetProperties() = 0;
};

template<typename ObjectT>
IViewModel* IViewModel::Create(ObjectT* pObject)
{
	return Create(typeid(*pObject).hash_code(), pObject);
}