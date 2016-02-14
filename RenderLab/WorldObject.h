#pragma once

class Model;
class Renderer;

class WorldObject
{
public:
	WorldObject(Model* pModel, Vec3 pos, Quaternion orientation, Vec3 scale);

	inline void SetModel(Model* pModel) { m_pModel = pModel; }
	inline const Model* GetModel() const { return m_pModel; }

	inline const Vec3 GetPosition() const { return m_position; }
	inline void SetPosition(Vec3 pos) { m_position = pos; }

	inline const Quaternion GetOrientation() const { return m_orientation; }

	inline const Vec3 GetScale() const { return m_scale; }
	inline void SetScale(Vec3 scale) { m_scale = scale; }

	inline const Matrix44 GetTransform() const;

	void QueueDraw(Renderer* pRenderer) const;

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