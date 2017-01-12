#include "Precompiled.h"
#include "RenderWindow.h"
#include "render\Renderer.h"
#include "RenderDoc/RenderDocUtil.h"
#include "render\RdrOffscreenTasks.h"
#include "Physics.h"
#include "WorldObject.h"
#include "ViewModels/SceneViewModel.h"
#include "UI.h"

RenderWindow* RenderWindow::Create(int x, int y, int width, int height, SceneViewModel* pSceneViewModel, const Widget* pParent)
{
	return new RenderWindow(x, y, width, height, pSceneViewModel, pParent);
}

RenderWindow::RenderWindow(int x, int y, int width, int height, SceneViewModel* pSceneViewModel, const Widget* pParent)
	: WindowBase(x, y, width, height, "Renderer", pParent)
	, m_pSceneViewModel(pSceneViewModel)
	, m_pPlacingObject(nullptr)
{
	m_renderer.Init(GetWindowHandle(), width, height, &m_inputManager);
	m_defaultInputContext.SetCamera(&m_mainCamera);
	m_inputManager.PushContext(&m_defaultInputContext);
}

void RenderWindow::Close()
{
	m_renderer.Cleanup();
	WindowBase::Close();
}

bool RenderWindow::OnResize(int newWidth, int newHeight)
{
	m_renderer.Resize(newWidth, newHeight);
	return true;
}

bool RenderWindow::OnKeyDown(int key)
{
	m_inputManager.SetKeyDown(key, true);

	if (key == KEY_DELETE)
	{
		m_pSceneViewModel->RemoveObject(m_pSceneViewModel->GetSelected());
	}

	return true;
}

bool RenderWindow::OnKeyUp(int key)
{
	m_inputManager.SetKeyDown(key, false);
	return true;
}

bool RenderWindow::OnChar(char c)
{
	m_inputManager.HandleChar(c);
	return true;
}

bool RenderWindow::OnMouseDown(int button, int mx, int my)
{
	m_inputManager.SetMouseDown(button, true, mx, my);

	Physics::RaycastResult res;
	if (button == 0 && RaycastAtCursor(mx, my, &res) && res.pActor)
	{
		WorldObject* pObject = static_cast<WorldObject*>(Physics::GetActorUserData(res.pActor));
		m_pSceneViewModel->SetSelected(pObject);
	}

	return true;
}

bool RenderWindow::OnMouseUp(int button, int mx, int my)
{
	m_inputManager.SetMouseDown(button, false, mx, my);
	return true;
}

bool RenderWindow::OnMouseMove(int mx, int my)
{
	if (m_pPlacingObject)
	{
		// Update placing object's position.
		Physics::RaycastResult res;
		if (RaycastAtCursor(mx, my, &res))
		{
			m_pPlacingObject->SetPosition(res.position);
		}
	}

	m_inputManager.SetMousePos(mx, my);
	return true;
}

bool RenderWindow::OnMouseEnter(int mx, int my)
{
	assert(!m_pPlacingObject);

	Widget* pDragWidget = UI::GetDraggedWidget();
	if (pDragWidget)
	{
		// Widget is being dragged around.  If the widget represents a placeable object,
		// then create it and add it to the scene.  OnMouseMove will handle placement.
		const WidgetDragData& rDragData = pDragWidget->GetDragData();
		if (rDragData.type == WidgetDragDataType::kModelAsset)
		{
			m_pPlacingObject = WorldObject::Create("New Object", Vec3::kOrigin, Quaternion::kIdentity, Vec3::kOne);
			m_pPlacingObject->AttachModel(ModelInstance::Create(rDragData.data.assetName, nullptr, 0));
			m_pSceneViewModel->AddObject(m_pPlacingObject);
		}
		else if (rDragData.type == WidgetDragDataType::kObjectAsset)
		{
			// todo
		}
	}

	return false;
}

bool RenderWindow::OnMouseLeave()
{
	CancelObjectPlacement();
	return false;
}

void RenderWindow::OnDragEnd(Widget* pDraggedWidget)
{
	if (m_pPlacingObject)
	{
		m_pSceneViewModel->SetSelected(m_pPlacingObject);
		m_pPlacingObject = nullptr;
	}
}

bool RenderWindow::ShouldCaptureMouse() const
{
	return true;
}

bool RenderWindow::RaycastAtCursor(int mx, int my, Physics::RaycastResult* pResult)
{
	Vec3 rayDir = m_mainCamera.CalcRayDirection(mx / (float)GetWidth(), my / (float)GetHeight());
	return Physics::Raycast(m_mainCamera.GetPosition(), rayDir, 50000.f, pResult);
}

void RenderWindow::CancelObjectPlacement()
{
	if (m_pPlacingObject)
	{
		m_pSceneViewModel->RemoveObject(m_pPlacingObject);
		m_pPlacingObject->Release();
		m_pPlacingObject = nullptr;
	}
}

void RenderWindow::EarlyUpdate()
{
	m_inputManager.Reset();
}

void RenderWindow::Update()
{
	m_inputManager.GetActiveContext()->Update(m_inputManager);
}

void RenderWindow::QueueDraw()
{
	// Apply device changes (resizing, fullscreen, etc)
	m_renderer.ApplyDeviceChanges();

	RdrOffscreenTasks::IssuePendingActions(m_renderer);

	// Primary render action
	m_renderer.BeginPrimaryAction(m_mainCamera, *m_pSceneViewModel->GetScene());
	{
		Debug::QueueDraw(m_renderer, m_mainCamera);
	}
	m_renderer.EndAction();
}

void RenderWindow::DrawFrame()
{
	m_renderer.DrawFrame();
}

void RenderWindow::PostFrameSync()
{
	m_renderer.PostFrameSync();
}

void RenderWindow::SetCameraPosition(const Vec3& position, const Vec3& pitchYawRoll)
{
	m_mainCamera.SetPosition(position);
	m_mainCamera.SetPitchYawRoll(pitchYawRoll);
}
