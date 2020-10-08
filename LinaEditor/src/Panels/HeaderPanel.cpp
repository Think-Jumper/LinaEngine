/*
Author: Inan Evin
www.inanevin.com
https://github.com/inanevin/LinaEngine

Copyright 2020~ Inan Evin

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions
and limitations under the License.

Class: HeaderPanel
Timestamp: 10/8/2020 1:39:19 PM

*/


#include "Panels/HeaderPanel.hpp"
#include "Utility/Math/Vector.hpp"
#include "Utility/Math/Math.hpp"
#include "Rendering/Texture.hpp"
#include "Rendering/RenderEngine.hpp"
#include "Rendering/Window.hpp"
#include "Core/GUILayer.hpp"
#include "Utility/UtilityFunctions.hpp"
#include "imgui.h"
#include "IconsFontAwesome5.h"

LinaEngine::Color headerBGColor = LinaEngine::Color(0, 0, 0, 1);
LinaEngine::Color headerButtonsColor = LinaEngine::Color(1, 1, 1, 1); // ImVec4(113.f / 255.f, 36.f / 255.f, 78.f / 255.f, 1);
LinaEngine::Color menuBarButtonActiveColor = LinaEngine::Color(0.5f, 0.5f, 0.5f, 1.0f);
ImVec2 resizeStartPos;
ImVec2 headerClickPos;
ImVec2 linaLogoSize = ImVec2(160, 18);
LinaEngine::Vector2 resizeStartSize;
LinaEngine::Graphics::Texture* windowLogo;
LinaEngine::Graphics::Texture* windowIcon;
bool appResizeActive;

#define RESIZE_THRESHOLD 10


namespace LinaEditor
{
	HeaderPanel::~HeaderPanel()
	{
		for (int i = 0; i < m_menuBarButtons.size(); i++)
			delete m_menuBarButtons[i];
	}


	void HeaderPanel::Setup()
	{
		m_renderEngine = m_guiLayer->GetRenderEngine();
		m_appWindow = m_guiLayer->GetAppWindow();

		// Logo texture
		windowLogo = &m_renderEngine->CreateTexture2D(LinaEngine::Utility::GetUniqueID(), "resources/textures/linaEngineText.png");
		windowIcon = &m_renderEngine->CreateTexture2D(LinaEngine::Utility::GetUniqueID(), "resources/textures/linaEngineIcon.png");

		// Add menu bar buttons.

		// File menu.
		std::vector<MenuElement*> fileItems;
		fileItems.emplace_back(new MenuItem(ICON_FA_FOLDER_PLUS " New Project", nullptr));
		fileItems.emplace_back(new MenuItem(ICON_FA_FOLDER_OPEN " Open Project", nullptr));
		fileItems.emplace_back(new MenuItem(ICON_FA_SAVE " Save Project", nullptr));
		m_menuBarButtons.push_back(new MenuButton(ICON_FA_FILE " File", "pu_file", fileItems, headerBGColor, false));

		// Edit menu.
		std::vector<MenuElement*> edit;
		m_menuBarButtons.emplace_back(new MenuButton(ICON_FA_EDIT " Edit", "pu_edit", edit, headerBGColor, true));

		// View menu.
		std::vector<MenuElement*> view;
		m_menuBarButtons.emplace_back(new MenuButton(ICON_FA_EYE " View", "pu_view", view, headerBGColor, true));

		// Levels menu.
		std::vector<MenuElement*> level;
		level.emplace_back(new MenuItem(ICON_FA_DOWNLOAD " Save Level Data", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::SaveLevelData)));
		level.emplace_back(new MenuItem(ICON_FA_UPLOAD " Load Level Data", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::LoadLevelData)));
		m_menuBarButtons.emplace_back(new MenuButton(ICON_FA_ARCHWAY " Level", "pu_level", level, headerBGColor, true));

		// Panels menu
		std::vector<MenuElement*> panels;
		panels.emplace_back(new MenuItem(ICON_FA_DOWNLOAD " Entity Panel", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::ECSPanel)));
		panels.emplace_back(new MenuItem(ICON_FA_UPLOAD " Material Panel", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::MaterialPanel)));
		panels.emplace_back(new MenuItem(ICON_FA_UPLOAD " Scene Panel", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::ScenePanel)));
		panels.emplace_back(new MenuItem(ICON_FA_UPLOAD " Resources Panel", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::PropertiesPanel)));
		panels.emplace_back(new MenuItem(ICON_FA_UPLOAD " Properties Panel", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::ResourcesPanel)));
		panels.emplace_back(new MenuItem(ICON_FA_UPLOAD " Log Panel", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::LogPanel)));
		panels.emplace_back(new MenuItem(ICON_FA_UPLOAD " ImGui Panel", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::ImGuiPanel)));
		m_menuBarButtons.emplace_back(new MenuButton(ICON_FA_COLUMNS " Level", "pu_panel", panels, headerBGColor, true));

		// Debug menu
		std::vector<MenuElement*> debug;
		debug.emplace_back(new MenuItem(ICON_FA_DOWNLOAD " Debug View Physics", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::DebugViewPhysics)));
		debug.emplace_back(new MenuItem(ICON_FA_DOWNLOAD " Debug View Shadows", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::DebugViewShadows)));
		debug.emplace_back(new MenuItem(ICON_FA_DOWNLOAD " Debug View NOrmal", std::bind(&GUILayer::MenuBarItemClicked, m_guiLayer, MenuBarItems::DebugViewNormal)));
		m_menuBarButtons.emplace_back(new MenuButton(ICON_FA_COLUMNS " Debug", "dbg_panel", debug, headerBGColor, true));
	}

	void HeaderPanel::Draw()
	{
		if (m_show)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();

			// Handle app window resize.
			bool horizontalResize = Math::Abs(ImGui::GetMousePos().x - viewport->Size.x) < RESIZE_THRESHOLD;
			bool verticalResize = Math::Abs(ImGui::GetMousePos().y - viewport->Size.y) < RESIZE_THRESHOLD;

			if (horizontalResize && !verticalResize)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
			else if (verticalResize && !horizontalResize)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			}
			else if (verticalResize && horizontalResize)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
			}

			if (horizontalResize || verticalResize || appResizeActive)
			{
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					appResizeActive = true;
					ImVec2 delta = ImVec2(ImGui::GetMousePos().x - resizeStartPos.x, ImGui::GetMousePos().y - resizeStartPos.y);

					m_appWindow->SetSize(Vector2(resizeStartSize.x + delta.x, resizeStartSize.y + delta.y));
				}
				else
				{
					resizeStartSize = m_appWindow->GetSize();
					resizeStartPos = ImGui::GetMousePos();
					appResizeActive = false;
				}
			}

			// Start drawing window.
			ImGui::SetNextWindowPos(ImVec2(viewport->GetWorkPos().x, viewport->GetWorkPos().y));
			ImGui::SetNextWindowSize(ImVec2(viewport->GetWorkSize().x, 80));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(headerBGColor.r, headerBGColor.g, headerBGColor.b, headerBGColor.a));
			ImGui::Begin("Header", NULL, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

			// Handle window movement.
			if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				headerClickPos = ImGui::GetMousePos();

				ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
				Vector2 windowPos = m_appWindow->GetPos();
				Vector2 newPos = Vector2(windowPos.x + delta.x, windowPos.y + delta.y);

				if (newPos.x < 0.0f)
					newPos.x = 0.0f;

				if (newPos.y < 0.0f)
					newPos.y = 0.0f;

				m_appWindow->SetPos(newPos);
			}

			// Icon
			ImGui::Image((void*)windowIcon->GetID(), ImVec2(16, 16), ImVec2(0, 1), ImVec2(1, 0));

			// Title
			ImGui::SameLine();
			ImGui::Text(m_title.c_str());

			// Minimize, maximize, exit buttons.
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 105);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - linaLogoSize.y / 2.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(headerBGColor.r, headerBGColor.g, headerBGColor.b, headerBGColor.a));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(headerButtonsColor.r, headerButtonsColor.g, headerButtonsColor.b, headerButtonsColor.a));

			// Minimize
			if (ImGui::Button(ICON_FA_WINDOW_MINIMIZE))
			{
				m_appWindow->Iconify();
			}

			// Maximize/Restore
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_WINDOW_MAXIMIZE))
			{
				m_appWindow->Maximize();
			}

			// Exit.
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_WINDOW_CLOSE))
			{
				m_appWindow->Close();
			}

			ImGui::PopStyleColor();
			ImGui::PopStyleColor();

			// Logo
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2 - linaLogoSize.x / 2.0f);
			ImGui::SetCursorPosY(ImGui::GetCursorPos().y + linaLogoSize.y / 2.0f + 15);
			ImGui::Image((void*)windowLogo->GetID(), linaLogoSize, ImVec2(0, 1), ImVec2(1, 0));

			// Draw bar buttons & items.
			ImGui::SetCursorPosY(35);
			for (int i = 0; i < m_menuBarButtons.size(); i++)
				m_menuBarButtons[i]->Draw();

			ImGui::End();
			ImGui::PopStyleColor();

		}
	}

}