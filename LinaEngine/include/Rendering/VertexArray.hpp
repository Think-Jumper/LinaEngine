/*
Author: Inan Evin
www.inanevin.com

Copyright 2018 Inan Evin

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, 
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions 
and limitations under the License.

Class: VertexArray
Timestamp: 4/26/2019 12:30:15 AM

*/

#pragma once

#ifndef VertexArray_HPP
#define VertexArray_HPP

#include "Rendering/IndexedModel.hpp"

namespace LinaEngine::Graphics
{
	class VertexArray
	{
	public:

		VertexArray() {};
			
		FORCEINLINE ~VertexArray()
		{
			m_EngineBoundID = renderDevice->ReleaseVertexArray(m_EngineBoundID);
		}

		FORCEINLINE void Construct(PAMRenderDevice& deviceIn, const IndexedModel& model, BufferUsage bufferUsage)
		{
			renderDevice = &deviceIn;
			m_EngineBoundID = model.CreateVertexArray(deviceIn, bufferUsage);
			m_IndexCount = model.GetIndexCount();
		}

		FORCEINLINE void UpdateBuffer(uint32 bufferIndex, const void* data, uintptr dataSize)
		{
			return renderDevice->UpdateVertexArrayBuffer(m_EngineBoundID, bufferIndex, data, dataSize);
		}

		FORCEINLINE uint32 GetID() { return m_EngineBoundID; }
		inline uint32 GetIndexCount() { return m_IndexCount;  }

	private:

		PAMRenderDevice* renderDevice = nullptr;
		uint32 m_EngineBoundID = 0;
		uint32 m_IndexCount = 0;

		NULL_COPY_AND_ASSIGN(VertexArray);
	};

}


#endif