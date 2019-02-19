/*
Author: Inan Evin
www.inanevin.com

Copyright 2018 Inan Evin

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, 
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions 
and limitations under the License.

Class: Texture
Timestamp: 1/7/2019 1:55:47 PM

*/

#pragma once

#ifndef Texture_HPP
#define Texture_HPP

#include <glad/glad.h>

namespace LinaEngine
{
	class Texture
	{
	public:

		Texture() {};
		Texture(GLenum textureTarget);

		/* Loads the texture with a given filename. */
		bool Load(const std::string& fileName);

		/* Binds the loaded texture as a unit to the program. */
		void Bind(GLenum textureUnit);

	private:

		GLenum m_TextureTarget;
		GLuint m_TextureObj;
	};
}


#endif