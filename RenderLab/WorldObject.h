#pragma once

class Model;
class Renderer;

class WorldObject
{
public:
	WorldObject(Model* pModel, Vec3 pos, Quaternion orientation, Vec3 scale);

	inline void SetModel(Model* pModel);
	inline const Model* GetModel() const;

	inline const Vec3& GetPosition() const;
	inline void SetPosition(Vec3 pos);

	inline const Quaternion& GetOrientation() const;

	inline const Vec3& GetScale() const;
	inline void SetScale(Vec3 scale);

	inline const Matrix44 GetTransform() const;

	void QueueDraw(Renderer& rRenderer) const;

private:
	Model* m_pModel;
	Vec3 m_position;
	Vec3 m_scale;
	Quaternion m_orientation;
};

inline const Matrix44 WorldObject::GetTransform() const
{
	return Matrix44Transformation(Vec3::kOrigin, Quaternion::kIdentity, m_scale, Vec3::kOrigin, m_orientation, m_position);
}

inline void WorldObject::SetModel(Model* pModel)
{
	m_pModel = pModel; 
}

inline const Model* WorldObject::GetModel() const
{ 
	return m_pModel; 
}

inline const Vec3& WorldObject::GetPosition() const
{
	return m_position; 
}

inline void WorldObject::SetPosition(Vec3 pos)
{ 
	m_position = pos; 
}

inline const Quaternion& WorldObject::GetOrientation() const
{ 
	return m_orientation; 
}

inline const Vec3& WorldObject::GetScale() const
{ 
	return m_scale;
}

inline void WorldObject::SetScale(Vec3 scale)
{ 
	m_scale = scale; 
}