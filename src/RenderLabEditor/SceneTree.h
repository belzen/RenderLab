#pragma once

#include "WindowBase.h"

class Scene;

class SceneTree : public WindowBase
{
public:
	void SetScene(Scene* pScene);


private:
	Scene* m_pScene;
};