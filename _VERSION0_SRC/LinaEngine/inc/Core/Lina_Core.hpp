/*
Author: Inan Evin
www.inanevin.com


MIT License

Lina Engine, Copyright (c) 2018 Inan Evin

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


*/

#pragma once

#ifndef LINA_CORE_HPP
#define LINA_CORE_HPP

#include "Rendering/Lina_RenderingEngine.hpp"
#include "Input/Lina_InputEngine.hpp"
#include "Lina_ObjectHandler.hpp"

extern const long SECOND;

class Lina_GameCore;
class Lina_EngineInstances;

class Lina_Core {

public:

    Lina_Core(Lina_GameCore&, std::string, float, float, double);
	Lina_Core(const Lina_Core& r) {};


	~Lina_Core();
	void Initialize();
	inline Lina_RenderingEngine& GetRenderingEngine() { return renderingEngine; }
	inline Lina_InputEngine& GetInputEngine() { return inputEngine; }

private:

	void StartSystems();
	void Stop();
	void Run();
	void Render();
	void CleanUp();
	bool isRunning = false;

	double m_FrameCap;

	Lina_InputEngine inputEngine;
	Lina_RenderingEngine renderingEngine;
	Lina_GameCore* game;

	// Object Specific
	Lina_ObjectHandler objectHandler;


};

#endif