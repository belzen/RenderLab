#pragma once

class PropertyDef;

class IViewModel
{
public:
	static IViewModel* Create(uint64 typeId, void* pTypeData);

	virtual ~IViewModel() {};

	virtual const PropertyDef** GetProperties() = 0;
};