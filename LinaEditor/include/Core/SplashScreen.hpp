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

Class: SplashScreen
Timestamp: 10/10/2020 3:25:27 PM

*/
#pragma once

#ifndef SplashScreen_HPP
#define SplashScreen_HPP

// Headers here.

namespace LinaEngine
{
	namespace Graphics
	{
		class Window;
		class RenderEngine;
		struct WindowProperties;
	}
}

namespace LinaEditor
{
	class SplashScreen
	{
		
	public:
		
		SplashScreen() {};
		~SplashScreen();
		
		void Draw();
		void Setup(LinaEngine::Graphics::Window* splashWindow, LinaEngine::Graphics::RenderEngine* renderEngine, const LinaEngine::Graphics::WindowProperties& props);

	private:
	
		LinaEngine::Graphics::Window* m_window = nullptr;
	};
}

#endif