/*
Author: Inan Evin
www.inanevin.com

Copyright 2018 Inan Evin

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions
and limitations under the License.

Class: RenderEngine
Timestamp: 4/27/2019 11:18:07 PM

*/

#include "Rendering/RenderEngine.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/RenderConstants.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/ArrayBitmap.hpp"
#include "ECS/Systems/CameraSystem.hpp"
#include "ECS/Components/CameraComponent.hpp"
#include "ECS/ECS.hpp"
#include "Utility/Math/Color.hpp"
#include "Utility/UtilityFunctions.hpp"
#include "PackageManager/OpenGL/GLRenderDevice.hpp"
#include "Core/Layer.hpp"


namespace LinaEngine::Graphics
{

	constexpr size_t UNIFORMBUFFER_VIEWDATA_SIZE = (sizeof(Matrix) * 3) + (sizeof(Vector4)) + (sizeof(float) * 2);
	constexpr int UNIFORMBUFFER_VIEWDATA_BINDPOINT = 0;
	constexpr auto UNIFORMBUFFER_VIEWDATA_NAME = "ViewData";

	constexpr size_t UNIFORMBUFFER_LIGHTDATA_SIZE = (sizeof(int) * 2) + sizeof(Vector4) + sizeof(Vector4);
	constexpr int UNIFORMBUFFER_LIGHTDATA_BINDPOINT = 1;
	constexpr auto UNIFORMBUFFER_LIGHTDATA_NAME = "LightData";

	constexpr size_t UNIFORMBUFFER_DEBUGDATA_SIZE = (sizeof(bool) * 1);
	constexpr int UNIFORMBUFFER_DEBUGDATA_BINDPOINT = 2;
	constexpr auto UNIFORMBUFFER_DEBUGDATA_NAME = "DebugData";

	RenderEngine::RenderEngine()
	{
		LINA_CORE_TRACE("[Constructor] -> RenderEngine ({0})", typeid(*this).name());
	}

	RenderEngine::~RenderEngine()
	{
		// Delete textures.
		for (std::map<int, Texture*>::iterator it = m_loadedTextures.begin(); it != m_loadedTextures.end(); it++)
			delete it->second;

		// Dump the remaining memory.
		DumpMemory();

		// Release Vertex Array Objects
		m_skyboxVAO = m_renderDevice.ReleaseVertexArray(m_skyboxVAO);
		m_screenQuadVAO = m_renderDevice.ReleaseVertexArray(m_screenQuadVAO);
		m_hdriCubeVAO = m_renderDevice.ReleaseVertexArray(m_hdriCubeVAO);
		m_lineVAO = m_renderDevice.ReleaseVertexArray(m_lineVAO);

		LINA_CORE_TRACE("[Destructor] -> RenderEngine ({0})", typeid(*this).name());
	}

	void RenderEngine::Initialize(LinaEngine::ECS::ECSRegistry& ecsReg, Window& appWindow)
	{
		// Set references.
		m_appWindow = &appWindow;

		// Flip loaded images.
		ArrayBitmap::SetImageFlip(true);

		// Setup draw parameters.
		SetupDrawParameters();

		// Initialize the render device.
		m_renderDevice.Initialize(m_appWindow->GetWidth(), m_appWindow->GetHeight(), m_defaultDrawParams);

		// Construct the uniform buffer for global matrices.
		m_globalDataBuffer.Construct(m_renderDevice, UNIFORMBUFFER_VIEWDATA_SIZE, BufferUsage::USAGE_DYNAMIC_DRAW, NULL);
		m_globalDataBuffer.Bind(UNIFORMBUFFER_VIEWDATA_BINDPOINT);

		// Construct the uniform buffer for lights.
		m_globalLightBuffer.Construct(m_renderDevice, UNIFORMBUFFER_LIGHTDATA_SIZE, BufferUsage::USAGE_DYNAMIC_DRAW, NULL);
		m_globalLightBuffer.Bind(UNIFORMBUFFER_LIGHTDATA_BINDPOINT);

		// Construct the uniform buffer for debugging.
		m_globalDebugBuffer.Construct(m_renderDevice, UNIFORMBUFFER_DEBUGDATA_SIZE, BufferUsage::USAGE_DYNAMIC_DRAW, NULL);
		m_globalDebugBuffer.Bind(UNIFORMBUFFER_DEBUGDATA_BINDPOINT);

		// Initialize the engine shaders.
		ConstructEngineShaders();

		// Initialize engine materials
		ConstructEngineMaterials();

		// Initialize engine vertex arrays.
		ConstructEnginePrimitives();

		// Initialize built-in vertex array objects.
		m_skyboxVAO = m_renderDevice.CreateSkyboxVertexArray();
		m_hdriCubeVAO = m_renderDevice.CreateHDRICubeVertexArray();
		m_screenQuadVAO = m_renderDevice.CreateScreenQuadVertexArray();
		m_lineVAO = m_renderDevice.CreateLineVertexArray();

		// Construct render targets
		ConstructRenderTargets();

		// Create default textures.
		m_defaultTexture.ConstructEmpty(m_renderDevice);
		m_defaultCubemapTexture.ConstructRTCubemapTexture(m_renderDevice, m_viewportSize, SamplerParameters());

		// Initialize ECS Camera System.
		m_cameraSystem.Construct(ecsReg);
		m_cameraSystem.SetAspectRatio((float)m_viewportSize.x / (float)m_viewportSize.y);

		// Initialize ECS Mesh Renderer System
		m_meshRendererSystem.Construct(ecsReg, *this, m_renderDevice);
		m_spriteRendererSystem.Construct(ecsReg, *this, m_renderDevice);

		// Initialize ECS Lighting system.
		m_lightingSystem.Construct(ecsReg, m_renderDevice, *this);

		// Add the ECS systems into the pipeline.
		m_renderingPipeline.AddSystem(m_cameraSystem);
		m_renderingPipeline.AddSystem(m_meshRendererSystem);
		m_renderingPipeline.AddSystem(m_spriteRendererSystem);
		m_renderingPipeline.AddSystem(m_lightingSystem);

		// Set debug values.
		m_debugData.visualizeDepth = false;

	}

	void RenderEngine::Render()
	{
		// DrawShadows();

		Draw();

		if (!m_firstFrameDrawn)
		{
			ValidateEngineShaders();
			m_firstFrameDrawn = true;
		}

		//DrawOperationsDefault();

	}

	void RenderEngine::TickAndSwap(float frameTime)
	{
		// Draw GUI Layers
		for (Layer* layer : m_guiLayerStack)
			layer->OnTick(frameTime);

		// Update window.
		m_appWindow->Tick();
	}

	void RenderEngine::SetViewportDisplay(Vector2 pos, Vector2 size)
	{
		m_renderDevice.SetViewport(pos, size);
		m_viewportPos = pos;
		m_viewportSize = size;

		m_cameraSystem.SetAspectRatio((float)m_viewportSize.x / (float)m_viewportSize.y);

		// Resize render buffers & frame buffer textures
		m_renderDevice.ResizeRTTexture(m_primaryRTTexture0.GetID(), m_viewportSize, m_primaryRTParams.textureParams.internalPixelFormat, m_primaryRTParams.textureParams.pixelFormat);
		m_renderDevice.ResizeRTTexture(m_primaryRTTexture1.GetID(), m_viewportSize, m_primaryRTParams.textureParams.internalPixelFormat, m_primaryRTParams.textureParams.pixelFormat);
		//m_renderDevice.ResizeRTTexture(m_OutlineRTTexture.GetID(), windowSize, primaryRTParams.textureParams.internalPixelFormat, primaryRTParams.textureParams.pixelFormat);
		m_renderDevice.ResizeRTTexture(m_pingPongRTTexture1.GetID(), m_viewportSize, m_pingPongRTParams.textureParams.internalPixelFormat, m_pingPongRTParams.textureParams.pixelFormat);
		m_renderDevice.ResizeRTTexture(m_pingPongRTTexture1.GetID(), m_viewportSize, m_pingPongRTParams.textureParams.internalPixelFormat, m_pingPongRTParams.textureParams.pixelFormat);
		m_renderDevice.ResizeRenderBuffer(m_primaryRenderTarget.GetID(), m_primaryRenderBuffer.GetID(), m_viewportSize, RenderBufferStorage::STORAGE_DEPTH);
	}


	Material& RenderEngine::CreateMaterial(int id, Shaders shader)
	{
		if (!MaterialExists(id))
		{
			// Create material & set it's shader.
			SetMaterialShader(m_loadedMaterials[id], shader);
			m_loadedMaterials[id].m_MaterialID = id;
			return m_loadedMaterials[id];
		}
		else
		{
			// Abort if material exists.
			LINA_CORE_WARN("Material with the id {0} already exists, returning that...", id);
			return m_loadedMaterials[id];
		}
	}

	Texture& RenderEngine::CreateTexture2D(const std::string& filePath, SamplerParameters samplerParams, bool compress, bool useDefaultFormats)
	{
		// Create pixel data.
		ArrayBitmap* textureBitmap = new ArrayBitmap();

		int nrComponents = textureBitmap->Load(filePath);
		if (nrComponents == -1)
		{
			LINA_CORE_WARN("Texture with the path {0} doesn't exist, returning empty texture", filePath);
			delete textureBitmap;
			return m_defaultTexture;
		}

		if (useDefaultFormats)
		{
			if (nrComponents == 1)
				samplerParams.textureParams.internalPixelFormat = samplerParams.textureParams.pixelFormat = PixelFormat::FORMAT_R;
			if (nrComponents == 2)
				samplerParams.textureParams.internalPixelFormat = samplerParams.textureParams.pixelFormat = PixelFormat::FORMAT_RG;
			else if (nrComponents == 3)
				samplerParams.textureParams.internalPixelFormat = samplerParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;
			else if (nrComponents == 4)
				samplerParams.textureParams.internalPixelFormat = samplerParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGBA;

		}

		// Create texture & construct.
		Texture* texture = new Texture();
		texture->Construct(m_renderDevice, *textureBitmap, samplerParams, compress, filePath);
		m_loadedTextures[texture->GetID()] = texture;

		// Delete pixel data.
		delete textureBitmap;

		// Return
		return *m_loadedTextures[texture->GetID()];
	}

	Texture& RenderEngine::CreateTextureHDRI(const std::string filePath)
	{
		// Create pixel data.
		int w, h, nrComponents;
		float* data = ArrayBitmap::LoadImmediateHDRI(filePath.c_str(), w, h, nrComponents);

		if (!data)
		{
			LINA_CORE_WARN("Texture with the path {0} doesn't exist, returning empty texture", filePath);
			return m_defaultTexture;
		}

		// Create texture & construct.
		SamplerParameters samplerParams;
		samplerParams.textureParams.wrapR = samplerParams.textureParams.wrapS = samplerParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_EDGE;
		samplerParams.textureParams.minFilter = samplerParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		samplerParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGB16F;
		samplerParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;

		Texture* texture = new Texture();
		texture->ConstructHDRI(m_renderDevice, samplerParams, Vector2(w, h), data, filePath);
		m_loadedTextures[texture->GetID()] = texture;

		// Return
		return *m_loadedTextures[texture->GetID()];
	}

	Mesh& RenderEngine::CreateMesh(int id, const std::string& filePath, MeshParameters meshParams)
	{

		if (!MeshExists(id))
		{

			// Create object data & feed it from model.
			Mesh& mesh = m_loadedMeshes[id];
			mesh.SetParameters(meshParams);
			ModelLoader::LoadModel(filePath, mesh.GetIndexedModels(), mesh.GetMaterialIndices(), mesh.GetMaterialSpecs(), meshParams);

			if (mesh.GetIndexedModels().size() == 0)
			{
				LINA_CORE_WARN("Indexed model array is empty! The model with the name: {0} could not be found or model scene does not contain any mesh! Returning plane quad...", filePath);
				UnloadMeshResource(id);
				return GetPrimitive(Primitives::PLANE);
			}

			// Create vertex array for each mesh.
			for (uint32 i = 0; i < mesh.GetIndexedModels().size(); i++)
			{
				VertexArray* vertexArray = new VertexArray();
				vertexArray->Construct(m_renderDevice, mesh.GetIndexedModels()[i], BufferUsage::USAGE_STATIC_COPY);
				mesh.GetVertexArrays().push_back(vertexArray);
			}

			// Set id
			mesh.m_MeshID = id;
			mesh.m_path = filePath;

			// Return
			return m_loadedMeshes[id];
		}
		else
		{
			// Mesh with this name already exists!
			LINA_CORE_WARN("Mesh with the name {0} already exists, returning that...", filePath);
			return m_loadedMeshes[id];
		}

	}

	Mesh& RenderEngine::CreatePrimitive(Primitives primitive, const std::string& path)
	{
		if (!MeshExists(primitive))
		{

			// Create object data & feed it from model.
			Mesh& mesh = m_loadedMeshes[primitive];

			ModelLoader::LoadModel(path, mesh.GetIndexedModels(), mesh.GetMaterialIndices(), mesh.GetMaterialSpecs(), MeshParameters());

			if (mesh.GetIndexedModels().size() == 0)
			{
				LINA_CORE_WARN("Indexed model array is empty! Primitive {0} could not be loaded, returning empty mesh", primitive);
				return Mesh();
			}

			// Create vertex array for each mesh.
			for (uint32 i = 0; i < mesh.GetIndexedModels().size(); i++)
			{
				VertexArray* vertexArray = new VertexArray();
				vertexArray->Construct(m_renderDevice, mesh.GetIndexedModels()[i], BufferUsage::USAGE_STATIC_COPY);
				mesh.GetVertexArrays().push_back(vertexArray);
			}

			// Set ID
			mesh.m_MeshID = primitive;

			// Return
			return m_loadedMeshes[primitive];
		}
		else
		{
			// Mesh with this name already exists!
			LINA_CORE_WARN("Primitive with the ID{0} already exists, returning that...", primitive);
			return m_loadedMeshes[primitive];
		}
	}

	Shader& RenderEngine::CreateShader(Shaders shader, const std::string& path, bool usesGeometryShader)
	{
		// Create shader
		if (!ShaderExists(shader))
		{
			std::string shaderText;
			Utility::LoadTextFileWithIncludes(shaderText, path, "#include");
			return m_loadedShaders[shader].Construct(m_renderDevice, shaderText, usesGeometryShader);
		}
		else
		{
			// Shader with this name already exists!
			LINA_CORE_WARN("Shader with the id {0} already exists, returning that...", shader);
			return m_loadedShaders[shader];
		}
	}

	Material& RenderEngine::GetMaterial(int id)
	{
		if (!MaterialExists(id))
		{
			// Mesh not found.
			LINA_CORE_WARN("Material with the id {0} was not found, returning default material...", id);
			return m_defaultUnlit;
		}

		return m_loadedMaterials[id];
	}

	Texture& RenderEngine::GetTexture(int id)
	{
		if (!TextureExists(id))
		{
			// Mesh not found.
			LINA_CORE_WARN("Texture with the id {0} was not found, returning un-constructed texture...", id);
			return Texture();
		}

		return *m_loadedTextures[id];
	}

	Texture& RenderEngine::GetTexture(const std::string& path)
	{
		std::map<int, Texture*>::iterator it = std::find_if(m_loadedTextures.begin(), m_loadedTextures.end(), [path]
		(std::pair<int, Texture*> const& item) -> bool { return item.second->GetPath().compare(path) == 0; });

		if (it == m_loadedTextures.end())
		{
			// Mesh not found.
			LINA_CORE_WARN("Texture with the path {0} was not found, returning un-constructed texture...", path);
			return Texture();
		}

		return *it->second;
	}

	Mesh& RenderEngine::GetMesh(int id)
	{
		if (!MeshExists(id))
		{
			// Mesh not found.
			LINA_CORE_WARN("Mesh with the id {0} was not found, returning un-constructed mesh...", id);
			return Mesh();
		}

		return m_loadedMeshes[id];
	}

	Mesh& RenderEngine::GetMesh(const std::string& path)
	{
		std::map<int, Mesh>::iterator it = std::find_if(m_loadedMeshes.begin(), m_loadedMeshes.end(), [path]
		(std::pair<int, Mesh> const& item) -> bool { return item.second.GetPath().compare(path) == 0; });

		if (it == m_loadedMeshes.end())
		{
			// Mesh not found.
			LINA_CORE_WARN("Mesh with the path {0} was not found, returning un-constructed mesh...", path);
			return Mesh();
		}

		return it->second;
	}

	Shader& RenderEngine::GetShader(Shaders shader)
	{
		if (!ShaderExists(shader))
		{
			// Shader not found.
			LINA_CORE_WARN("Shader with the ID {0} was not found, returning standardUnlit Shader", shader);
			return GetShader(Shaders::STANDARD_UNLIT);
		}

		return m_loadedShaders[shader];
	}

	Mesh& RenderEngine::GetPrimitive(Primitives primitive)
	{
		if (!MeshExists(primitive))
		{
			// VA not found.
			LINA_CORE_WARN("Primitive with the ID {0} was not found, returning plane...", primitive);
			return GetPrimitive(Primitives::PLANE);
		}
		else
			return m_loadedMeshes[primitive];
	}

	Material& RenderEngine::SetMaterialShader(Material& material, Shaders shader)
	{

		// If no shader found, fall back to standardLit
		if (m_loadedShaders.find(shader) == m_loadedShaders.end()) {
			LINA_CORE_WARN("Shader with engine ID {0} was not found. Setting material's shader to standardUnlit.", shader);
			material.shaderID = m_loadedShaders[Shaders::STANDARD_UNLIT].GetID();
		}
		else
			material.shaderID = m_loadedShaders[shader].GetID();

		// Clear all shader related material data.
		material.sampler2Ds.clear();
		material.colors.clear();
		material.floats.clear();
		material.ints.clear();
		material.vector3s.clear();
		material.vector2s.clear();
		material.matrices.clear();
		material.vector4s.clear();
		material.m_ShaderType = shader;

		if (shader == Shaders::STANDARD_UNLIT)
		{
			material.colors[MAT_OBJECTCOLORPROPERTY] = Color::White;
			material.sampler2Ds[MAT_TEXTURE2D_DIFFUSE] = { 0 };
			material.sampler2Ds[MAT_TEXTURE2D_SHADOWMAP] = { 1 };
			material.ints[MAT_SURFACETYPE] = 0;
			material.isShadowMapped = true;
			m_shadowMappedMaterials.emplace(&material);

		}
		else if (shader == Shaders::SKYBOX_SINGLECOLOR)
		{
			material.colors[MAT_COLOR] = Color::Gray;
		}
		else if (shader == Shaders::SKYBOX_GRADIENT)
		{
			material.colors[MAT_STARTCOLOR] = Color::Black;
			material.colors[MAT_ENDCOLOR] = Color::White;
		}
		else if (shader == Shaders::SKYBOX_PROCEDURAL)
		{
			material.colors[MAT_STARTCOLOR] = Color::Black;
			material.colors[MAT_ENDCOLOR] = Color::White;
			material.vector3s[MAT_SUNDIRECTION] = Vector3(0, -1, 0);
		}
		else if (shader == Shaders::SKYBOX_CUBEMAP)
		{
			material.sampler2Ds[MAT_MAP_ENVIRONMENT] = { 0 };
		}
		else if (shader == Shaders::SKYBOX_HDRI)
		{
			material.sampler2Ds[MAT_MAP_ENVIRONMENT] = { 0 };
		}

		else if (shader == Shaders::SCREEN_QUAD_FINAL)
		{

			material.sampler2Ds[MAT_MAP_SCREEN] = { 0 };
			material.sampler2Ds[MAT_MAP_BLOOM] = { 1 };
			material.sampler2Ds[MAT_MAP_OUTLINE] = { 2 };
			material.floats[MAT_EXPOSURE] = 1.0f;
			material.floats[MAT_FXAAREDUCEMIN] = 1.0f / 128.0f;
			material.floats[MAT_FXAAREDUCEMUL] = 1.0f / 8.0f;
			material.floats[MAT_FXAASPANMAX] = 8.0f;
			material.booleans[MAT_BLOOMENABLED] = false;
			material.booleans[MAT_FXAAENABLED] = false;
			material.vector3s[MAT_INVERSESCREENMAPSIZE] = Vector3();
		}
		else if (shader == Shaders::SCREEN_QUAD_BLUR)
		{
			material.sampler2Ds[MAT_MAP_SCREEN] = { 0 };
			material.booleans[MAT_ISHORIZONTAL] = false;
		}
		else if (shader == Shaders::SCREEN_QUAD_OUTLINE)
		{
			material.sampler2Ds[MAT_MAP_SCREEN] = { 0 };
		}
		else if (shader == Shaders::SCREEN_SHADOWMAP)
		{

		}
		else if (shader == Shaders::PBR_LIT)
		{
			material.sampler2Ds[MAT_TEXTURE2D_ALBEDOMAP] = { 0 };
			material.sampler2Ds[MAT_TEXTURE2D_NORMALMAP] = { 1 };
			material.sampler2Ds[MAT_TEXTURE2D_ROUGHNESSMAP] = { 2 };
			material.sampler2Ds[MAT_TEXTURE2D_METALLICMAP] = { 3 };
			material.sampler2Ds[MAT_TEXTURE2D_AOMAP] = { 4 };
			material.sampler2Ds[MAT_TEXTURE2D_BRDFLUTMAP] = { 5 };
			material.sampler2Ds[MAT_TEXTURE2D_SHADOWMAP] = { 6 };
			material.sampler2Ds[MAT_TEXTURE2D_IRRADIANCEMAP] = { 7, nullptr, TextureBindMode::BINDTEXTURE_CUBEMAP, false };
			material.sampler2Ds[MAT_TEXTURE2D_PREFILTERMAP] = { 8,nullptr, TextureBindMode::BINDTEXTURE_CUBEMAP, false };
			material.floats[MAT_METALLICMULTIPLIER] = 1.0f;
			material.floats[MAT_ROUGHNESSMULTIPLIER] = 1.0f;
			material.ints[MAT_WORKFLOW] = 0;
			material.vector2s[MAT_TILING] = Vector2::One;
			material.receivesLighting = true;
			material.isShadowMapped = true;
			material.usesHDRI = true;

			m_shadowMappedMaterials.emplace(&material);
		}
		else if (shader == Shaders::EQUIRECTANGULAR_HDRI)
		{
			material.sampler2Ds[MAT_MAP_EQUIRECTANGULAR] = { 0 };
			material.matrices[UF_MATRIX_VIEW] = Matrix();
			material.matrices[UF_MATRIX_PROJECTION] = Matrix();
		}
		else if (shader == Shaders::DEBUG_LINE)
		{
			material.colors[MAT_COLOR] = Color::White;
		}
		else if (shader == Shaders::RENDERER2D_SPRITE)
		{
			material.colors[MAT_OBJECTCOLORPROPERTY] = Color::White;
			material.sampler2Ds[MAT_TEXTURE2D_DIFFUSE] = { 0 };
		}


		return material;
	}

	void RenderEngine::UnloadTextureResource(int id)
	{
		if (!TextureExists(id))
		{
			LINA_CORE_WARN("Texture not found! Aborting... ");
			return;
		}

		delete m_loadedTextures[id];
		m_loadedTextures.erase(id);
	}

	void RenderEngine::UnloadMeshResource(int id)
	{
		if (!MeshExists(id))
		{
			LINA_CORE_WARN("Mesh not found! Aborting... ");
			return;
		}

		m_loadedMeshes.erase(id);
	}

	void RenderEngine::UnloadMaterialResource(int id)
	{
		if (!MaterialExists(id))
		{
			LINA_CORE_WARN("Material not found! Aborting... ");
			return;
		}

		// If its in the internal list, remove first.
		if (m_shadowMappedMaterials.find(&m_loadedMaterials[id]) != m_shadowMappedMaterials.end())
			m_shadowMappedMaterials.erase(&m_loadedMaterials[id]);

		m_loadedMaterials.erase(id);
	}

	bool RenderEngine::MaterialExists(int id)
	{
		return !(m_loadedMaterials.find(id) == m_loadedMaterials.end());
	}

	bool RenderEngine::TextureExists(int id)
	{
		return !(m_loadedTextures.find(id) == m_loadedTextures.end());
	}

	bool RenderEngine::MeshExists(int id)
	{
		return !(m_loadedMeshes.find(id) == m_loadedMeshes.end());
	}

	bool RenderEngine::ShaderExists(Shaders shader)
	{
		return !(m_loadedShaders.find(shader) == m_loadedShaders.end());
	}

	void RenderEngine::ConstructEngineShaders()
	{
		// Unlit.
		Shader& unlit = CreateShader(Shaders::STANDARD_UNLIT, "resources/engine/shaders/Unlit/Unlit.glsl");
		unlit.BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		unlit.BindBlockToBuffer(UNIFORMBUFFER_DEBUGDATA_BINDPOINT, UNIFORMBUFFER_DEBUGDATA_NAME);

		// PBR Lit
		Shader& pbrLit = CreateShader(Shaders::PBR_LIT, "resources/engine/shaders/PBR/PBRLit.glsl", false);
		pbrLit.BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		pbrLit.BindBlockToBuffer(UNIFORMBUFFER_LIGHTDATA_BINDPOINT, UNIFORMBUFFER_LIGHTDATA_NAME);
		pbrLit.BindBlockToBuffer(UNIFORMBUFFER_DEBUGDATA_BINDPOINT, UNIFORMBUFFER_DEBUGDATA_NAME);

		// Skies
		CreateShader(Shaders::SKYBOX_SINGLECOLOR, "resources/engine/shaders/Skybox/SkyboxColor.glsl");
		CreateShader(Shaders::SKYBOX_GRADIENT, "resources/engine/shaders/Skybox/SkyboxGradient.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		CreateShader(Shaders::SKYBOX_CUBEMAP, "resources/engine/shaders/Skybox/SkyboxCubemap.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		CreateShader(Shaders::SKYBOX_PROCEDURAL, "resources/engine/shaders/Skybox/SkyboxProcedural.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		CreateShader(Shaders::SKYBOX_HDRI, "resources/engine/shaders/Skybox/SkyboxHDRI.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);


		// Equirectangular cube & irradiance for HDRI skbox
		CreateShader(Shaders::EQUIRECTANGULAR_HDRI, "resources/engine/shaders/HDRI/HDRIEquirectangular.glsl");
		CreateShader(Shaders::IRRADIANCE_HDRI, "resources/engine/shaders/HDRI/HDRIIrradiance.glsl");
		CreateShader(Shaders::PREFILTER_HDRI, "resources/engine/shaders/HDRI/HDRIPrefilter.glsl");
		CreateShader(Shaders::BRDF_HDRI, "resources/engine/shaders/HDRI/HDRIBRDF.glsl");


		// Screen Quad Shaders
		CreateShader(Shaders::SCREEN_QUAD_FINAL, "resources/engine/shaders/ScreenQuads/SQFinal.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		CreateShader(Shaders::SCREEN_QUAD_BLUR, "resources/engine/shaders/ScreenQuads/SQBlur.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		CreateShader(Shaders::SCREEN_QUAD_OUTLINE, "resources/engine/shaders/ScreenQuads/SQOutline.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
		CreateShader(Shaders::SCREEN_SHADOWMAP, "resources/engine/shaders/ScreenQuads/SQShadowMap.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);

		// Line
		CreateShader(Shaders::DEBUG_LINE, "resources/engine/shaders/Misc/DebugLine.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);

		// 2D
		CreateShader(Shaders::RENDERER2D_SPRITE, "resources/engine/shaders/2D/Sprite.glsl").BindBlockToBuffer(UNIFORMBUFFER_VIEWDATA_BINDPOINT, UNIFORMBUFFER_VIEWDATA_NAME);
	}

	bool RenderEngine::ValidateEngineShaders()
	{
		int validation = 0;
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::STANDARD_UNLIT).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::PBR_LIT).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SKYBOX_SINGLECOLOR).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SKYBOX_GRADIENT).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SKYBOX_CUBEMAP).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SKYBOX_PROCEDURAL).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SKYBOX_HDRI).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::EQUIRECTANGULAR_HDRI).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::IRRADIANCE_HDRI).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::PREFILTER_HDRI).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::BRDF_HDRI).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SCREEN_QUAD_FINAL).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SCREEN_QUAD_BLUR).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SCREEN_QUAD_OUTLINE).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::SCREEN_SHADOWMAP).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::DEBUG_LINE).GetID());
		validation += m_renderDevice.ValidateShaderProgram(GetShader(Shaders::RENDERER2D_SPRITE).GetID());

		return !validation;
	}

	void RenderEngine::ConstructEngineMaterials()
	{
		SetMaterialShader(m_screenQuadFinalMaterial, Shaders::SCREEN_QUAD_FINAL);
		SetMaterialShader(m_screenQuadBlurMaterial, Shaders::SCREEN_QUAD_BLUR);
		SetMaterialShader(m_screenQuadOutlineMaterial, Shaders::SCREEN_QUAD_OUTLINE);
		SetMaterialShader(m_hdriMaterial, Shaders::EQUIRECTANGULAR_HDRI);
		SetMaterialShader(m_debugDrawMaterial, Shaders::DEBUG_LINE);
		SetMaterialShader(m_shadowMapMaterial, Shaders::SCREEN_SHADOWMAP);
		SetMaterialShader(m_defaultSkyboxMaterial, Shaders::SKYBOX_SINGLECOLOR);
		SetMaterialShader(m_defaultUnlit, Shaders::STANDARD_UNLIT);
	}

	void RenderEngine::ConstructEnginePrimitives()
	{
		// Primitives
		CreateMesh(Primitives::CUBE, "resources/engine/meshes/primitives/cube.obj");
		CreateMesh(Primitives::CYLINDER, "resources/engine/meshes/primitives/cylinder.obj");
		CreateMesh(Primitives::PLANE, "resources/engine/meshes/primitives/plane.obj");
		CreateMesh(Primitives::SPHERE, "resources/engine/meshes/primitives/sphere.obj");
		CreateMesh(Primitives::ICOSPHERE, "resources/engine/meshes/primitives/icosphere.obj");
		CreateMesh(Primitives::CONE, "resources/engine/meshes/primitives/cone.obj");
	}

	void RenderEngine::ConstructRenderTargets()
	{

		// Main
		m_mainRTParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;
		m_mainRTParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGBA16F;
		m_mainRTParams.textureParams.minFilter = m_mainRTParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		m_mainRTParams.textureParams.wrapS = m_mainRTParams.textureParams.wrapT = SamplerWrapMode::WRAP_REPEAT;

		// Primary
		m_primaryRTParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;
		m_primaryRTParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGB16F;
		m_primaryRTParams.textureParams.minFilter = m_primaryRTParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		m_primaryRTParams.textureParams.wrapS = m_primaryRTParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_EDGE;


		// Ping pong
		m_pingPongRTParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;
		m_pingPongRTParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGB16F;
		m_pingPongRTParams.textureParams.minFilter = m_pingPongRTParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		m_pingPongRTParams.textureParams.wrapS = m_pingPongRTParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_EDGE;

		// Shadows depth.
		m_shadowsRTParams.textureParams.pixelFormat = PixelFormat::FORMAT_DEPTH;
		m_shadowsRTParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_DEPTH16;
		m_shadowsRTParams.textureParams.minFilter = m_shadowsRTParams.textureParams.magFilter = SamplerFilter::FILTER_NEAREST;
		m_shadowsRTParams.textureParams.wrapS = m_shadowsRTParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_BORDER;

		// Initialize primary RT textures
		m_primaryRTTexture0.ConstructRTTexture(m_renderDevice, m_viewportSize, m_primaryRTParams, false);
		m_primaryRTTexture1.ConstructRTTexture(m_renderDevice, m_viewportSize, m_primaryRTParams, false);

		// Initialize ping pong rt texture
		m_pingPongRTTexture1.ConstructRTTexture(m_renderDevice, m_viewportSize, m_pingPongRTParams, false);
		m_pingPongRTTexture2.ConstructRTTexture(m_renderDevice, m_viewportSize, m_pingPongRTParams, false);

		// Initialize outilne RT texture
		//m_OutlineRTTexture.ConstructRTTexture(m_renderDevice, screenSize, primaryRTParams, false);

		// Shadow map RT texture
		m_shadowMapRTTexture.ConstructRTTexture(m_renderDevice, m_shadowMapResolution, m_shadowsRTParams, true);

		// Initialize primary render buffer
		m_primaryRenderBuffer.Construct(m_renderDevice, RenderBufferStorage::STORAGE_DEPTH, m_viewportSize);

		// Initialize hdri render buffer
		m_hdriCaptureRenderBuffer.Construct(m_renderDevice, RenderBufferStorage::STORAGE_DEPTH_COMP24, m_hdriResolution);

		// Initialize primary render target.
		m_primaryRenderTarget.Construct(m_renderDevice, m_primaryRTTexture0, m_viewportSize, TextureBindMode::BINDTEXTURE_TEXTURE2D, FrameBufferAttachment::ATTACHMENT_COLOR, FrameBufferAttachment::ATTACHMENT_DEPTH, m_primaryRenderBuffer.GetID());

		// Bind the extre texture to primary render target, also tell open gl that we are running mrts.
		m_renderDevice.BindTextureToRenderTarget(m_primaryRenderTarget.GetID(), m_primaryRTTexture1.GetID(), TextureBindMode::BINDTEXTURE_TEXTURE2D, FrameBufferAttachment::ATTACHMENT_COLOR, 1);
		uint32 attachments[2] = { FrameBufferAttachment::ATTACHMENT_COLOR , (FrameBufferAttachment::ATTACHMENT_COLOR + (uint32)1) };
		m_renderDevice.MultipleDrawBuffersCommand(m_primaryRenderTarget.GetID(), 2, attachments);

		// Initialize ping pong render targets
		m_pingPongRenderTarget1.Construct(m_renderDevice, m_pingPongRTTexture1, m_viewportSize, TextureBindMode::BINDTEXTURE_TEXTURE2D, FrameBufferAttachment::ATTACHMENT_COLOR);
		m_pingPongRenderTarget2.Construct(m_renderDevice, m_pingPongRTTexture2, m_viewportSize, TextureBindMode::BINDTEXTURE_TEXTURE2D, FrameBufferAttachment::ATTACHMENT_COLOR);

		// Initialize outline render target
		//m_OutlineRenderTarget.Construct(m_renderDevice, m_OutlineRTTexture, m_viewportSize.x, m_viewportSize.y, TextureBindMode::BINDTEXTURE_TEXTURE2D, FrameBufferAttachment::ATTACHMENT_COLOR);

		// Initialize HDRI render target
		m_hdriCaptureRenderTarget.Construct(m_renderDevice, m_hdriResolution, FrameBufferAttachment::ATTACHMENT_DEPTH, m_hdriCaptureRenderBuffer.GetID());

		// Initialize depth map for shadows
		m_shadowMapTarget.Construct(m_renderDevice, m_shadowMapRTTexture, m_shadowMapResolution, TextureBindMode::BINDTEXTURE_TEXTURE2D, FrameBufferAttachment::ATTACHMENT_DEPTH, true);

	}

	void RenderEngine::SetupDrawParameters()
	{
		// Set default drawing parameters.
		m_defaultDrawParams.useScissorTest = false;
		m_defaultDrawParams.useDepthTest = true;
		m_defaultDrawParams.useStencilTest = true;
		m_defaultDrawParams.primitiveType = PrimitiveType::PRIMITIVE_TRIANGLES;
		m_defaultDrawParams.faceCulling = FaceCulling::FACE_CULL_BACK;
		m_defaultDrawParams.sourceBlend = BlendFunc::BLEND_FUNC_SRC_ALPHA;
		m_defaultDrawParams.destBlend = BlendFunc::BLEND_FUNC_ONE_MINUS_SRC_ALPHA;
		m_defaultDrawParams.shouldWriteDepth = true;
		m_defaultDrawParams.depthFunc = DrawFunc::DRAW_FUNC_LESS;
		m_defaultDrawParams.stencilFunc = DrawFunc::DRAW_FUNC_ALWAYS;
		m_defaultDrawParams.stencilComparisonVal = 1;
		m_defaultDrawParams.stencilTestMask = 0xFF;
		m_defaultDrawParams.stencilWriteMask = 0x00;
		m_defaultDrawParams.stencilFail = StencilOp::STENCIL_KEEP;
		m_defaultDrawParams.stencilPass = StencilOp::STENCIL_REPLACE;
		m_defaultDrawParams.stencilPassButDepthFail = StencilOp::STENCIL_KEEP;
		m_defaultDrawParams.scissorStartX = 0;
		m_defaultDrawParams.scissorStartY = 0;
		m_defaultDrawParams.scissorWidth = 0;
		m_defaultDrawParams.scissorHeight = 0;


		// Set render to fbo target draw parameters.	
		m_fullscreenQuadDP.useScissorTest = false;
		m_fullscreenQuadDP.useDepthTest = false;
		m_fullscreenQuadDP.useStencilTest = true;
		m_fullscreenQuadDP.primitiveType = PrimitiveType::PRIMITIVE_TRIANGLES;
		m_fullscreenQuadDP.faceCulling = FaceCulling::FACE_CULL_NONE;
		m_fullscreenQuadDP.sourceBlend = BlendFunc::BLEND_FUNC_NONE;
		m_fullscreenQuadDP.destBlend = BlendFunc::BLEND_FUNC_NONE;
		m_fullscreenQuadDP.shouldWriteDepth = true;
		m_fullscreenQuadDP.depthFunc = DrawFunc::DRAW_FUNC_LESS;
		m_fullscreenQuadDP.stencilFunc = DrawFunc::DRAW_FUNC_ALWAYS;
		m_fullscreenQuadDP.stencilComparisonVal = 1;
		m_fullscreenQuadDP.stencilTestMask = 0xFF;
		m_fullscreenQuadDP.stencilWriteMask = 0xFF;
		m_fullscreenQuadDP.stencilFail = StencilOp::STENCIL_KEEP;
		m_fullscreenQuadDP.stencilPass = StencilOp::STENCIL_REPLACE;
		m_fullscreenQuadDP.stencilPassButDepthFail = StencilOp::STENCIL_KEEP;
		m_fullscreenQuadDP.scissorStartX = 0;
		m_fullscreenQuadDP.scissorStartY = 0;
		m_fullscreenQuadDP.scissorWidth = 0;
		m_fullscreenQuadDP.scissorHeight = 0;

		// Set skybox draw params.	
		m_skyboxDrawParams.useScissorTest = false;
		m_skyboxDrawParams.useDepthTest = true;
		m_skyboxDrawParams.useStencilTest = true;
		m_skyboxDrawParams.primitiveType = PrimitiveType::PRIMITIVE_TRIANGLES;
		m_skyboxDrawParams.faceCulling = FaceCulling::FACE_CULL_BACK;
		m_skyboxDrawParams.sourceBlend = BlendFunc::BLEND_FUNC_SRC_ALPHA;
		m_skyboxDrawParams.destBlend = BlendFunc::BLEND_FUNC_ONE_MINUS_SRC_ALPHA;
		m_skyboxDrawParams.shouldWriteDepth = true;
		m_skyboxDrawParams.depthFunc = DrawFunc::DRAW_FUNC_LEQUAL;
		m_skyboxDrawParams.stencilFunc = DrawFunc::DRAW_FUNC_ALWAYS;
		m_skyboxDrawParams.stencilComparisonVal = 0;
		m_skyboxDrawParams.stencilTestMask = 0xFF;
		m_skyboxDrawParams.stencilWriteMask = 0xFF;
		m_skyboxDrawParams.stencilFail = StencilOp::STENCIL_KEEP;
		m_skyboxDrawParams.stencilPass = StencilOp::STENCIL_REPLACE;
		m_skyboxDrawParams.stencilPassButDepthFail = StencilOp::STENCIL_KEEP;
		m_skyboxDrawParams.scissorStartX = 0;
		m_skyboxDrawParams.scissorStartY = 0;
		m_skyboxDrawParams.scissorWidth = 0;
		m_skyboxDrawParams.scissorHeight = 0;


		// Set depth map drawing parameters.
		m_shadowMapDrawParams.useScissorTest = false;
		m_shadowMapDrawParams.useDepthTest = true;
		m_shadowMapDrawParams.useStencilTest = false;
		m_shadowMapDrawParams.primitiveType = PrimitiveType::PRIMITIVE_TRIANGLES;
		m_shadowMapDrawParams.faceCulling = FaceCulling::FACE_CULL_NONE;
		m_shadowMapDrawParams.sourceBlend = BlendFunc::BLEND_FUNC_NONE;
		m_shadowMapDrawParams.destBlend = BlendFunc::BLEND_FUNC_NONE;
		m_shadowMapDrawParams.shouldWriteDepth = true;
		m_shadowMapDrawParams.depthFunc = DrawFunc::DRAW_FUNC_LESS;
		m_shadowMapDrawParams.stencilFunc = DrawFunc::DRAW_FUNC_ALWAYS;
		m_shadowMapDrawParams.stencilComparisonVal = 1;
		m_shadowMapDrawParams.stencilTestMask = 0xFF;
		m_shadowMapDrawParams.stencilWriteMask = 0xFF;
		m_shadowMapDrawParams.stencilFail = StencilOp::STENCIL_KEEP;
		m_shadowMapDrawParams.stencilPass = StencilOp::STENCIL_REPLACE;
		m_shadowMapDrawParams.stencilPassButDepthFail = StencilOp::STENCIL_KEEP;
		m_shadowMapDrawParams.scissorStartX = 0;
		m_shadowMapDrawParams.scissorStartY = 0;
		m_shadowMapDrawParams.scissorWidth = 0;
		m_shadowMapDrawParams.scissorHeight = 0;
	}

	void RenderEngine::DumpMemory()
	{
		// Clear dumps.
		m_loadedMeshes.clear();
		m_loadedTextures.clear();
		m_loadedMaterials.clear();
	}

	void RenderEngine::DrawShadows()
	{
		// Clear color.
		m_renderDevice.Clear(true, true, false, m_cameraSystem.GetCurrentClearColor(), 0xFF);

		// Update pipeline.
		m_renderingPipeline.UpdateSystems(0.0f);

		// Update uniform buffers on GPU
		UpdateUniformBuffers();

		// Set depth frame 
		m_renderDevice.SetFBO(m_shadowMapTarget.GetID());
		m_renderDevice.SetViewport(Vector2::Zero, m_shadowMapResolution);

		// Clear color.
		m_renderDevice.Clear(false, true, false, m_cameraSystem.GetCurrentClearColor(), 0xFF);

		// Draw scene
		DrawSceneObjects(m_shadowMapDrawParams, &m_shadowMapMaterial, false);

		// Add the shadow texture
		for (std::set<Material*>::iterator it = m_shadowMappedMaterials.begin(); it != m_shadowMappedMaterials.end(); ++it)
			(*it)->SetTexture(MAT_TEXTURE2D_SHADOWMAP, &m_shadowMapRTTexture);

	}

	void RenderEngine::Draw()
	{
		// Set render target
		m_renderDevice.SetFBO(m_primaryRenderTarget.GetID());
		m_renderDevice.SetViewport(Vector2::Zero, m_viewportSize);

		// Clear color.
		m_renderDevice.Clear(true, true, true, m_cameraSystem.GetCurrentClearColor(), 0xFF);

		// Update pipeline.
		m_renderingPipeline.UpdateSystems(0.0f);

		// Update uniform buffers on GPU
		UpdateUniformBuffers();

		// Draw scene
		DrawSceneObjects(m_defaultDrawParams);

		bool horizontal = true;

		if (m_screenQuadFinalMaterial.booleans[MAT_BLOOMENABLED])
		{
			// Write to the pingpong buffers to apply 2 pass gaussian blur.
			bool firstIteration = true;
			unsigned int amount = 4;
			for (unsigned int i = 0; i < amount; i++)
			{
				// Select FBO
				m_renderDevice.SetFBO(horizontal ? m_pingPongRenderTarget1.GetID() : m_pingPongRenderTarget2.GetID());

				// Setup material & use.
				m_screenQuadBlurMaterial.SetBool(MAT_ISHORIZONTAL, horizontal);
				if (firstIteration)
					m_screenQuadBlurMaterial.SetTexture(MAT_MAP_SCREEN, &m_primaryRTTexture1);
				else
				{
					if (horizontal)
						m_screenQuadBlurMaterial.SetTexture(MAT_MAP_SCREEN, &m_pingPongRTTexture2);
					else
						m_screenQuadBlurMaterial.SetTexture(MAT_MAP_SCREEN, &m_pingPongRTTexture1);
				}

				// Update shader data & draw.
				UpdateShaderData(&m_screenQuadBlurMaterial);
				m_renderDevice.Draw(m_screenQuadVAO, m_fullscreenQuadDP, 0, 6, true);
				horizontal = !horizontal;
				if (firstIteration) firstIteration = false;
			}
		}

		// Back to default buffer
		m_renderDevice.SetFBO(0);
		m_renderDevice.SetViewport(m_viewportPos, m_viewportSize);

		// Clear color bit.
		m_renderDevice.Clear(true, true, false, Color::White, 0xFF);

		// Set frame buffer texture on the material.
		m_screenQuadFinalMaterial.SetTexture(MAT_MAP_SCREEN, &m_primaryRTTexture0, TextureBindMode::BINDTEXTURE_TEXTURE2D);

		if (m_screenQuadFinalMaterial.booleans[MAT_BLOOMENABLED])
			m_screenQuadFinalMaterial.SetTexture(MAT_MAP_BLOOM, horizontal ? &m_pingPongRTTexture1 : &m_pingPongRTTexture2, TextureBindMode::BINDTEXTURE_TEXTURE2D);

		// m_ScreenQuadFinalMaterial.SetTexture(MAT_MAP_OUTLINE, &m_OutlineRTTexture, TextureBindMode::BINDTEXTURE_TEXTURE2D);

		Vector2 inverseMapSize = 1.0f / m_primaryRTTexture0.GetSize();
		m_screenQuadFinalMaterial.SetVector3(MAT_INVERSESCREENMAPSIZE, Vector3(inverseMapSize.x, inverseMapSize.y, 0.0));

		// update shader w/ material data.
		UpdateShaderData(&m_screenQuadFinalMaterial);

		// Draw full screen quad.
		m_renderDevice.Draw(m_screenQuadVAO, m_fullscreenQuadDP, 0, 6, true);
	}

	void RenderEngine::DrawLine(Vector3 p1, Vector3 p2, Color col, float width)
	{
		m_renderDevice.SetShader(m_debugDrawMaterial.shaderID);
		m_renderDevice.UpdateShaderUniformColor(m_debugDrawMaterial.shaderID, MAT_COLOR, col);
		m_renderDevice.DrawLine(m_debugDrawMaterial.shaderID, Matrix::Identity(), p1, p2, width);
	}

	void RenderEngine::SetDrawParameters(const DrawParams& params)
	{
		m_renderDevice.SetDrawParameters(params);
	}

	void RenderEngine::DrawOperationsDefault()
	{
		m_renderDevice.SetFBO(0);

		// Clear color.
		m_renderDevice.Clear(true, true, true, m_cameraSystem.GetCurrentClearColor(), 0xFF);

		// Update pipeline.
		m_renderingPipeline.UpdateSystems(0.0f);

		// Update uniform buffers on GPU
		UpdateUniformBuffers();

		// Draw scene
		DrawSceneObjects(m_defaultDrawParams, nullptr, true);
	}

	void RenderEngine::DrawSkybox()
	{
		if (m_skyboxMaterial != nullptr)
		{
			UpdateShaderData(m_skyboxMaterial);
			m_renderDevice.Draw(m_skyboxVAO, m_skyboxDrawParams, 1, 36, true);
		}
		else
		{
			UpdateShaderData(&m_defaultSkyboxMaterial);
			m_renderDevice.Draw(m_skyboxVAO, m_skyboxDrawParams, 1, 36, true);
		}
	}

	void RenderEngine::DrawSceneObjects(DrawParams& drawParams, Material* overrideMaterial, bool drawSkybox)
	{
		m_meshRendererSystem.FlushOpaque(drawParams, overrideMaterial, true);
		m_meshRendererSystem.FlushTransparent(drawParams, overrideMaterial, true);
		m_spriteRendererSystem.Flush(drawParams, overrideMaterial, true);

		// Draw skybox.
		if (drawSkybox)
			DrawSkybox();

		// Post scene draw callback.
		if (m_postSceneDrawCallback)
			m_postSceneDrawCallback();

	}

	void RenderEngine::UpdateUniformBuffers()
	{
		Vector3 cameraLocation = m_cameraSystem.GetCameraLocation();
		Vector4 viewPos = Vector4(cameraLocation.x, cameraLocation.y, cameraLocation.z, 1.0f);

		// Update global matrix buffer
		uintptr currentGlobalDataOffset = 0;
		m_globalDataBuffer.Update(&m_cameraSystem.GetProjectionMatrix()[0][0], currentGlobalDataOffset, sizeof(Matrix));
		currentGlobalDataOffset += sizeof(Matrix);

		m_globalDataBuffer.Update(&m_cameraSystem.GetViewMatrix()[0][0], currentGlobalDataOffset, sizeof(Matrix));
		currentGlobalDataOffset += sizeof(Matrix);

		m_globalDataBuffer.Update(&m_cameraSystem.GetLightMatrix(m_lightingSystem.GetDirLight())[0][0], currentGlobalDataOffset, sizeof(Matrix));
		currentGlobalDataOffset += sizeof(Matrix);

		m_globalDataBuffer.Update(&viewPos, currentGlobalDataOffset, sizeof(Vector4));
		currentGlobalDataOffset += sizeof(Vector4);

		ECS::CameraComponent* cameraComponent = m_cameraSystem.GetCurrentCameraComponent();

		if (cameraComponent != nullptr)
		{
			// Update only if changed.
			if (m_bufferValueRecord.zNear != cameraComponent->zNear)
			{
				m_bufferValueRecord.zNear = cameraComponent->zNear;
				m_globalDataBuffer.Update(&cameraComponent->zNear, currentGlobalDataOffset, sizeof(float));
			}
			currentGlobalDataOffset += sizeof(float);


			// Update only if changed.
			if (m_bufferValueRecord.zFar != cameraComponent->zFar)
			{
				m_bufferValueRecord.zFar = cameraComponent->zFar;
				m_globalDataBuffer.Update(&cameraComponent->zNear, currentGlobalDataOffset, sizeof(float));
			}
			currentGlobalDataOffset += sizeof(float);
		}

		// Update lights buffer.
		Color ambient = m_lightingSystem.GetAmbientColor();
		Vector4 ambientColor = Vector4(ambient.r, ambient.g, ambient.b, 1.0f);
		m_globalLightBuffer.Update(&m_currentPointLightCount, 0, sizeof(int));
		m_globalLightBuffer.Update(&m_currentSpotLightCount, sizeof(int), sizeof(int));
		m_globalLightBuffer.Update(&ambientColor, sizeof(int) * 2, sizeof(float) * 4);
		m_globalLightBuffer.Update(&m_cameraSystem.GetCameraLocation(), (sizeof(int) * 2) + (sizeof(float) * 4), sizeof(float) * 4);

		// Update debug fufer.
		m_globalDebugBuffer.Update(&m_debugData.visualizeDepth, 0, sizeof(bool));
	}

	void RenderEngine::UpdateShaderData(Material* data)
	{

		m_renderDevice.SetShader(data->GetShaderID());

		for (auto const& d : (*data).floats)
			m_renderDevice.UpdateShaderUniformFloat(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).booleans)
			m_renderDevice.UpdateShaderUniformInt(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).colors)
			m_renderDevice.UpdateShaderUniformColor(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).ints)
			m_renderDevice.UpdateShaderUniformInt(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).vector2s)
			m_renderDevice.UpdateShaderUniformVector2(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).vector3s)
			m_renderDevice.UpdateShaderUniformVector3(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).vector4s)
			m_renderDevice.UpdateShaderUniformVector4F(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).matrices)
			m_renderDevice.UpdateShaderUniformMatrix(data->shaderID, d.first, d.second);

		for (auto const& d : (*data).sampler2Ds)
		{
			// Set whether the texture is active or not.
			bool isActive = (d.second.isActive && !d.second.boundTexture->GetIsEmpty()) ? true : false;
			m_renderDevice.UpdateShaderUniformInt(data->shaderID, d.first + MAT_EXTENSION_ISACTIVE, isActive);

			// Set the texture to corresponding active unit.
			m_renderDevice.UpdateShaderUniformInt(data->shaderID, d.first + MAT_EXTENSION_TEXTURE2D, d.second.unit);

			// Set texture
			if (d.second.isActive)
				m_renderDevice.SetTexture(d.second.boundTexture->GetID(), d.second.boundTexture->GetSamplerID(), d.second.unit, d.second.bindMode, true);
			else
			{

				if (d.second.bindMode == TextureBindMode::BINDTEXTURE_TEXTURE2D)
					m_renderDevice.SetTexture(m_defaultTexture.GetID(), m_defaultTexture.GetSamplerID(), d.second.unit, BINDTEXTURE_TEXTURE2D);
				else
					m_renderDevice.SetTexture(m_defaultCubemapTexture.GetID(), m_defaultCubemapTexture.GetSamplerID(), d.second.unit, BINDTEXTURE_CUBEMAP);
			}
		}


		if (data->receivesLighting)
			m_lightingSystem.SetLightingShaderData(data->GetShaderID());

	}

	void RenderEngine::CaptureCalculateHDRI(Texture& hdriTexture)
	{
		// Create projection & view matrices for capturing HDRI data.
		Matrix captureProjection = Matrix::PerspectiveRH(90.0f, 1.0f, 0.1f, 10.0f);
		Matrix captureViews[] =
		{
			Matrix::InitLookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			Matrix::InitLookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			Matrix::InitLookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			Matrix::InitLookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			Matrix::InitLookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			Matrix::InitLookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		// Calculate HDRI, Irradiance, Prefilter and BRDF
		CalculateHDRICubemap(hdriTexture, captureProjection, captureViews);
		CalculateHDRIIrradiance(captureProjection, captureViews);
		CalculateHDRIPrefilter(captureProjection, captureViews);
		CalculateHDRIBRDF(captureProjection, captureViews);
		m_renderDevice.SetFBO(0);
		m_renderDevice.SetViewport(m_viewportPos, m_viewportSize);

		// Set flag
		m_hdriDataCaptured = true;
	}

	void RenderEngine::CalculateHDRICubemap(Texture& hdriTexture, glm::mat4& captureProjection, glm::mat4 views[6])
	{
		// Generate sampler.
		SamplerParameters samplerParams;
		samplerParams.textureParams.wrapR = samplerParams.textureParams.wrapS = samplerParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_EDGE;
		samplerParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		samplerParams.textureParams.minFilter = SamplerFilter::FILTER_LINEAR_MIPMAP_LINEAR;
		samplerParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGB16F;
		samplerParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;

		// Set resolution.
		m_hdriResolution = Vector2(512, 512);

		// Construct Cubemap texture.
		m_hdriCubemap.ConstructRTCubemapTexture(m_renderDevice, m_hdriResolution, samplerParams);

		// Setup shader data.
		uint32 equirectangularShader = GetShader(Shaders::EQUIRECTANGULAR_HDRI).GetID();
		m_renderDevice.SetShader(equirectangularShader);
		m_renderDevice.UpdateShaderUniformInt(equirectangularShader, MAT_MAP_EQUIRECTANGULAR + std::string(MAT_EXTENSION_TEXTURE2D), 0);
		m_renderDevice.UpdateShaderUniformInt(equirectangularShader, MAT_MAP_EQUIRECTANGULAR + std::string(MAT_EXTENSION_ISACTIVE), 1);
		m_renderDevice.UpdateShaderUniformMatrix(equirectangularShader, UF_MATRIX_PROJECTION, captureProjection);
		m_renderDevice.SetTexture(hdriTexture.GetID(), hdriTexture.GetSamplerID(), 0);
		m_renderDevice.SetFBO(m_hdriCaptureRenderTarget.GetID());
		m_renderDevice.SetViewport(Vector2::Zero, m_hdriResolution);

		// Draw the cubemap.
		for (uint32 i = 0; i < 6; ++i)
		{
			m_renderDevice.UpdateShaderUniformMatrix(equirectangularShader, UF_MATRIX_VIEW, views[i]);
			m_renderDevice.BindTextureToRenderTarget(m_hdriCaptureRenderTarget.GetID(), m_hdriCubemap.GetID(), TextureBindMode::BINDTEXTURE_CUBEMAP_POSITIVE_X, FrameBufferAttachment::ATTACHMENT_COLOR, 0, i, 0, false);
			m_renderDevice.SetFBO(m_hdriCaptureRenderTarget.GetID());
			m_renderDevice.Clear(true, true, true, m_cameraSystem.GetCurrentClearColor(), 0xFF);
			m_renderDevice.Draw(m_hdriCubeVAO, m_defaultDrawParams, 0, 36, true);
		}

		// Generate mipmaps & check errors.
		m_renderDevice.GenerateTextureMipmaps(m_hdriCubemap.GetID(), TextureBindMode::BINDTEXTURE_CUBEMAP);
		m_renderDevice.IsRenderTargetComplete(m_hdriCaptureRenderTarget.GetID());
	}

	void RenderEngine::CalculateHDRIIrradiance(Matrix& captureProjection, Matrix views[6])
	{
		// Generate sampler.
		SamplerParameters irradianceParams;
		irradianceParams.textureParams.wrapR = irradianceParams.textureParams.wrapS = irradianceParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_EDGE;
		irradianceParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		irradianceParams.textureParams.minFilter = SamplerFilter::FILTER_LINEAR_MIPMAP_LINEAR;
		irradianceParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGB16F;
		irradianceParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;

		// Set resolution
		Vector2 irradianceMapResolsution = Vector2(32, 32);

		// Create irradiance texture & scale render buffer according to the resolution.
		m_hdriIrradianceMap.ConstructRTCubemapTexture(m_renderDevice, irradianceMapResolsution, irradianceParams);
		m_renderDevice.SetFBO(m_hdriCaptureRenderTarget.GetID());
		m_renderDevice.ResizeRenderBuffer(m_hdriCaptureRenderTarget.GetID(), m_hdriCaptureRenderBuffer.GetID(), irradianceMapResolsution, RenderBufferStorage::STORAGE_DEPTH_COMP24);

		// Create & setup shader info.
		uint32 irradianceShader = GetShader(Shaders::IRRADIANCE_HDRI).GetID();
		m_renderDevice.SetShader(irradianceShader);
		m_renderDevice.UpdateShaderUniformInt(irradianceShader, MAT_MAP_ENVIRONMENT + std::string(MAT_EXTENSION_TEXTURE2D), 0);
		m_renderDevice.UpdateShaderUniformInt(irradianceShader, MAT_MAP_ENVIRONMENT + std::string(MAT_EXTENSION_ISACTIVE), 1);
		m_renderDevice.UpdateShaderUniformMatrix(irradianceShader, UF_MATRIX_PROJECTION, captureProjection);
		m_renderDevice.SetTexture(m_hdriCubemap.GetID(), m_hdriCubemap.GetSamplerID(), 0, TextureBindMode::BINDTEXTURE_CUBEMAP);
		m_renderDevice.SetViewport(Vector2::Zero, irradianceMapResolsution);

		// Draw cubemap.
		for (uint32 i = 0; i < 6; ++i)
		{
			m_renderDevice.UpdateShaderUniformMatrix(irradianceShader, UF_MATRIX_VIEW, views[i]);
			m_renderDevice.BindTextureToRenderTarget(m_hdriCaptureRenderTarget.GetID(), m_hdriIrradianceMap.GetID(), TextureBindMode::BINDTEXTURE_CUBEMAP_POSITIVE_X, FrameBufferAttachment::ATTACHMENT_COLOR, 0, i, 0, false, false);
			m_renderDevice.Clear(true, true, true, m_cameraSystem.GetCurrentClearColor(), 0xFF);
			m_renderDevice.Draw(m_hdriCubeVAO, m_defaultDrawParams, 0, 36, true);
		}
	}

	void RenderEngine::CalculateHDRIPrefilter(Matrix& captureProjection, Matrix views[6])
	{
		// Generate sampler.
		SamplerParameters prefilterParams;
		prefilterParams.textureParams.generateMipMaps = true;
		prefilterParams.textureParams.wrapR = prefilterParams.textureParams.wrapS = prefilterParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_EDGE;
		prefilterParams.textureParams.minFilter = SamplerFilter::FILTER_LINEAR_MIPMAP_LINEAR;
		prefilterParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		prefilterParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGB16F;
		prefilterParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;

		// Set resolution
		Vector2 prefilterResolution = Vector2(128, 128);

		// Construct prefilter texture.
		m_hdriPrefilterMap.ConstructRTCubemapTexture(m_renderDevice, prefilterResolution, prefilterParams);

		// Setup shader data.
		uint32 prefilterShader = GetShader(Shaders::PREFILTER_HDRI).GetID();
		m_renderDevice.SetShader(prefilterShader);
		m_renderDevice.UpdateShaderUniformInt(prefilterShader, MAT_MAP_ENVIRONMENT + std::string(MAT_EXTENSION_TEXTURE2D), 0);
		m_renderDevice.UpdateShaderUniformInt(prefilterShader, MAT_MAP_ENVIRONMENT + std::string(MAT_EXTENSION_ISACTIVE), 1);
		m_renderDevice.UpdateShaderUniformFloat(prefilterShader, MAT_ENVIRONMENTRESOLUTION, 512.0f);
		m_renderDevice.UpdateShaderUniformMatrix(prefilterShader, UF_MATRIX_PROJECTION, captureProjection);
		m_renderDevice.SetTexture(m_hdriCubemap.GetID(), m_hdriCubemap.GetSamplerID(), 0, TextureBindMode::BINDTEXTURE_CUBEMAP);

		// Setup mip levels & switch fbo.
		uint32 maxMipLevels = 5;
		m_renderDevice.SetFBO(m_hdriCaptureRenderTarget.GetID());

		for (uint32 mip = 0; mip < maxMipLevels; ++mip)
		{
			// reisze framebuffer according to mip-level size.
			unsigned int mipWidth = 128 * std::pow(0.5, mip);
			unsigned int mipHeight = 128 * std::pow(0.5, mip);
			m_renderDevice.ResizeRenderBuffer(m_hdriCaptureRenderTarget.GetID(), m_hdriCaptureRenderBuffer.GetID(), Vector2(mipWidth, mipHeight), RenderBufferStorage::STORAGE_DEPTH_COMP24);
			m_renderDevice.SetViewport(Vector2::Zero, Vector2(mipWidth, mipHeight));

			// Draw prefiltered map
			float roughness = (float)mip / (float)(maxMipLevels - 1);
			m_renderDevice.UpdateShaderUniformFloat(prefilterShader, MAT_ROUGHNESSMULTIPLIER, roughness);
			for (unsigned int i = 0; i < 6; ++i)
			{
				m_renderDevice.UpdateShaderUniformMatrix(prefilterShader, UF_MATRIX_VIEW, views[i]);
				m_renderDevice.BindTextureToRenderTarget(m_hdriCaptureRenderTarget.GetID(), m_hdriPrefilterMap.GetID(), TextureBindMode::BINDTEXTURE_CUBEMAP_POSITIVE_X, FrameBufferAttachment::ATTACHMENT_COLOR, 0, i, mip, false, false);
				m_renderDevice.Clear(true, true, true, m_cameraSystem.GetCurrentClearColor(), 0xFF);
				m_renderDevice.Draw(m_hdriCubeVAO, m_defaultDrawParams, 0, 36, true);
			}
		}
	}

	void RenderEngine::CalculateHDRIBRDF(Matrix& captureProjection, Matrix views[6])
	{
		// Generate sampler.
		SamplerParameters samplerParams;
		samplerParams.textureParams.wrapR = samplerParams.textureParams.wrapS = samplerParams.textureParams.wrapT = SamplerWrapMode::WRAP_CLAMP_EDGE;
		samplerParams.textureParams.magFilter = SamplerFilter::FILTER_LINEAR;
		samplerParams.textureParams.minFilter = SamplerFilter::FILTER_LINEAR;
		samplerParams.textureParams.internalPixelFormat = PixelFormat::FORMAT_RGB16F;
		samplerParams.textureParams.pixelFormat = PixelFormat::FORMAT_RGB;

		// Set resolution.
		Vector2 brdfLutSize = Vector2(512, 512);

		// Create BRDF texture.
		m_HDRILutMap.ConstructHDRI(m_renderDevice, samplerParams, brdfLutSize, NULL);

		// Scale render buffer according to the resolution & bind lut map to frame buffer.
		m_renderDevice.ResizeRenderBuffer(m_hdriCaptureRenderTarget.GetID(), m_hdriCaptureRenderBuffer.GetID(), brdfLutSize, RenderBufferStorage::STORAGE_DEPTH_COMP24);
		m_renderDevice.BindTextureToRenderTarget(m_hdriCaptureRenderTarget.GetID(), m_HDRILutMap.GetID(), TextureBindMode::BINDTEXTURE_TEXTURE2D, FrameBufferAttachment::ATTACHMENT_COLOR, 0, 0, 0, true, false);

		// Setup shader.
		uint32 brdfShader = GetShader(Shaders::BRDF_HDRI).GetID();
		m_renderDevice.SetShader(brdfShader);

		// Switch framebuffer & draw.
		m_renderDevice.SetFBO(m_hdriCaptureRenderTarget.GetID());
		m_renderDevice.SetViewport(Vector2::Zero, brdfLutSize);
		m_renderDevice.Clear(true, true, true, m_cameraSystem.GetCurrentClearColor(), 0xFF);
		m_renderDevice.Draw(m_screenQuadVAO, m_fullscreenQuadDP, 0, 6, true);
	}

	void RenderEngine::SetHDRIData(Material* mat)
	{
		if (mat == nullptr)
		{
			LINA_CORE_WARN("Material set HDRI data for is null, returning...");
			return;
		}

		if (!mat->usesHDRI)
		{
			LINA_CORE_WARN("This material's shader does not use HDRI calculations, returning...");
			return;
		}

		if (!m_hdriDataCaptured)
		{
			LINA_CORE_WARN("HDRI data is not captured, please capture it first then set the material's data.");
			return;
		}

		mat->SetTexture(MAT_TEXTURE2D_IRRADIANCEMAP, &m_hdriIrradianceMap, TextureBindMode::BINDTEXTURE_CUBEMAP);
		mat->SetTexture(MAT_TEXTURE2D_BRDFLUTMAP, &m_HDRILutMap, TextureBindMode::BINDTEXTURE_TEXTURE2D);
		mat->SetTexture(MAT_TEXTURE2D_PREFILTERMAP, &m_hdriPrefilterMap, TextureBindMode::BINDTEXTURE_CUBEMAP);
	}

	void RenderEngine::RemoveHDRIData(Material* mat)
	{
		if (mat == nullptr)
		{
			LINA_CORE_WARN("Material set HDRI data for is null, returning...");
			return;
		}

		if (!mat->usesHDRI)
		{
			LINA_CORE_WARN("This material's shader does not use HDRI calculations, returning...");
			return;
		}

		mat->RemoveTexture(MAT_TEXTURE2D_IRRADIANCEMAP);
		mat->RemoveTexture(MAT_TEXTURE2D_BRDFLUTMAP);
		mat->RemoveTexture(MAT_TEXTURE2D_PREFILTERMAP);

	}

	void RenderEngine::PushLayer(Layer* layer)
	{
		m_guiLayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void RenderEngine::PushOverlay(Layer* layer)
	{
		m_guiLayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void* RenderEngine::GetFinalImage()
	{
		return (void*)m_primaryRTTexture0.GetID();
	}

	void* RenderEngine::GetShadowMapImage()
	{
		return (void*)m_shadowMapRTTexture.GetID();
	}

}