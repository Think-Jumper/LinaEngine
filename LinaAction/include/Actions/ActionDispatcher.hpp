/*
Author: Inan Evin
www.inanevin.com

Copyright 2018 Inan Evin

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions
and limitations under the License.

Class: ActionDispatcher
Timestamp: 4/10/2019 1:26:00 PM

*/

#pragma once

#ifndef ActionDispatcher_HPP
#define ActionDispatcher_HPP

#include "Action.hpp"
#include "Utility/Log.hpp"
#include "Core/SizeDefinitions.hpp"
#include <map>
#include <vector>

namespace LinaEngine::Action 
{
	class ActionDispatcher
	{

	public:

		virtual ~ActionDispatcher();

	protected:

		ActionDispatcher();

		template<typename T>
		FORCEINLINE void DispatchAction(ActionType at, const T& data)
		{
			try {

				// Get the handler array corresponding to the action type.
				std::vector<ActionHandlerBase*>& arr = m_actionHandlerMap.at(at);

				// Iterate through the array of handlers w/ the same action type and execute to check conditions (if exists).
				std::vector<ActionHandlerBase*>::iterator it;
				for (it = arr.begin(); it != arr.end(); it++)
				{
					ActionHandler<T>* handler = (static_cast<ActionHandler<T>*>(*it));
					handler->ControlExecute(data);
				}
			}
			catch (const std::out_of_range& e)
			{
				const char* exp = e.what();
				LINA_CORE_ERR("Out of Range Exception while subscribing handler! {0}", exp);
			}
		}

	private:

		friend class ActionSubscriber;
		void SubscribeHandler(ActionHandlerBase* ptr);
		void UnsubscribeHandler(ActionHandlerBase* handler);

	private:

		std::map<uint32, std::vector<ActionHandlerBase*>> m_actionHandlerMap;

	};
}


#endif