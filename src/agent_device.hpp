//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#pragma once
#include "component.hpp"
#include "data_item.hpp"
#include "device.hpp"
#include "globals.hpp"

#include <map>

namespace mtconnect
{
  class Adapter;

  class AgentDevice : public Device
  {
   public:
    // Constructor that sets variables from an attribute map
    AgentDevice(const Attributes &attributes);
    ~AgentDevice() override = default;

    void addAdapter(const Adapter *adapter);

    DataItem *getConnectionStatus(const Adapter *adapter);

   protected:
    void addRequiredDataItems();

   protected:
    Component *m_adapters{nullptr};
  };
}  // namespace mtconnect
