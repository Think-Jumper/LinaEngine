/*
 * Copyright (C) 2019 Inan Evin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include "common.glh"
#include <uniformBuffers.glh>
#include <utility.glh>

#if defined(VS_BUILD)

Layout(0) attribute vec3 position;
Layout(1) attribute vec2 texCoord;
Layout(2) attribute vec3 normal;
Layout(4) attribute mat4 model;
Layout(8) attribute mat4 inverseTransposeModel;

out VS_OUT
{
	vec3 Normal;
	vec3 FragPos;
	vec2 TexCoords;
} vs_out;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
	vs_out.FragPos = vec3(model * vec4(position,1.0));
	vs_out.Normal = mat3(inverseTransposeModel) * normal;
    vs_out.TexCoords = texCoord;
}

#elif defined(GEO_BUILD)

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VS_OUT {
    vec3 Normal;
	vec3 FragPos;
	vec2 TexCoords;
} gs_in[];

out vec2 TexCoords; 

uniform float time;


void main() {    
    
}

#elif defined(FS_BUILD)

struct MaterialSampler2D
{
	sampler2D texture;
	int isActive;
};

struct Material
{
MaterialSampler2D diffuse;
vec3 objectColor;
int surfaceType;
vec2 tiling;
};

uniform Material material;


in VS_OUT
{
	vec3 Normal;
	vec3 FragPos;
	vec2 TexCoords;
} fs_in;

#include "lighting.glh"



out vec4 fragColor;

void main()
{
	
 
	if(visualizeDepth)
	{
		float depth = LinearizeDepth(gl_FragCoord.z, cameraFar, cameraNear) / cameraFar;		
		fragColor = vec4(vec3(depth), 1);
	}
	else
	{

		vec4 diffuseTextureColor =  material.diffuse.isActive != 0 ? texture(material.diffuse.texture, vec2(fs_in.TexCoords.x * material.tiling.x, fs_in.TexCoords.y * material.tiling.y)) : vec4(1,1,1,1);
		vec3 viewPos = vec3(cameraPosition.x, cameraPosition.y, cameraPosition.z);
		vec3 normal = normalize(fs_in.Normal);
		vec3 viewDir = normalize(viewPos - fs_in.FragPos);

		vec3 finalColor = diffuseTextureColor.rgb * material.objectColor;
		
		vec3 lighting = vec3(0.0);
		
		for(int i = 0; i < pointLightCount; i++)
			lighting += CalculatePointLight(pointLights[i], normal, fs_in.FragPos, viewDir);    
		
		finalColor *= lighting;
		finalColor =  pow(finalColor, vec3(1.0/2.2));
		
		float alpha = material.surfaceType == 0 ? 1.0 : diffuseTextureColor.a;
		fragColor = vec4(finalColor, alpha);
	
	
	}
}
#endif
