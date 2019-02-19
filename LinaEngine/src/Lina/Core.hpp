/*
Author: Inan Evin
www.inanevin.com

Copyright 2018 Inan Evin

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions
and limitations under the License.

Class: Lina_Core
Timestamp: 12/29/2018 10:43:46 PM

*/

#pragma once

#ifndef Lina_Core_HPP
#define Lina_Core_HPP


#ifdef LINA_PLATFORM_WINDOWS
#ifdef LINA_BUILD_DLL
#define LINA_API __declspec(dllexport)
#else
#define LINA_API __declspec(dllimport)
#endif
#else
#error Lina supports Windows only for now.
#endif


#ifdef LINA_ENABLE_ASSERTS

#include"stdio.h"
#include"stdlib.h"

#define LINA_CLIENT_ASSERT(x,...) { if(!(x)) { LINA_CLIENT_ERR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define LINA_CORE_ASSERT(x,...)  {	if(!(x)) { LINA_CORE_ERR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }

#define check(x) if(x == NULL) {LINA_CORE_ERR("CHECK failed!\nLine: {0}\nFunc: {1}\nFile: {2}", __LINE__,__FUNCTION__,__FILE__); __debugbreak();}
#define ensure(x) if(x == NULL) {LINA_CORE_ERR("CHECK failed!\nLine: {0}\nFunc: {1}\nFile: {2}", __LINE__,__FUNCTION__,__FILE__); __debugbreak(); return;}


#else


#define LINA_CLIENT_ASSERT(x,...)
#define LINA_CORE_ASSERT(x,...)
#define check(x)

#endif


#define BIT(x) (1 << x)
#define INVALID_VALUE 0xFFFFFFFF



#endif