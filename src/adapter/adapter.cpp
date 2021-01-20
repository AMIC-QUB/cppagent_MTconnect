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

#define __STDC_LIMIT_MACROS 1
#include "adapter/adapter.hpp"

#include "device_model/device.hpp"

#include <dlib/logger.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

using namespace std;
using namespace std::literals;

namespace mtconnect
{
  namespace adapter
  {
    static dlib::logger g_logger("input.adapter");

    // Adapter public methods
    Adapter::Adapter(string device, const string &server, const unsigned int port,
                     std::chrono::seconds legacyTimeout)
      : Connector(server, port, legacyTimeout),
        m_agent(nullptr),
        m_device(nullptr),
        m_deviceName(std::move(device)),
        m_running(true),
        m_autoAvailable(false),
        m_reconnectInterval{10000ms}
    {
      stringstream url;
      url << "shdr://" << server << ':' << port;
      m_url = url.str();

      stringstream identity;
      identity << '_' << server << '_' << port;
      m_identity = identity.str();
    }

    Adapter::~Adapter()
    {
      if (m_running)
        stop();
    }

    void Adapter::stop()
    {
      // Will stop threaded object gracefully Adapter::thread()
      m_running = false;
      close();
      wait();
    }

    void Adapter::setAgent(Agent &agent)
    {
      m_agent = &agent;
      m_device = m_agent->getDeviceByName(m_deviceName);
      if (m_device)
      {
        m_device->addAdapter(this);
        m_allDevices.emplace_back(m_device);
      }
    }

    void Adapter::addDevice(string &device)
    {
      auto dev = m_agent->getDeviceByName(device);
      if (dev)
      {
        m_allDevices.emplace_back(dev);
        dev->addAdapter(this);
      }
    }

    void Adapter::processData(const string &data)
    {
      if (m_terminator)
      {
        if (data == *m_terminator)
        {
          if (m_handler && m_handler->m_processData)
            m_handler->m_processData(m_body.str(), m_context);
          m_terminator.reset();
          m_body.str("");
        }
        else
        {
          m_body << endl << data;
        }

        return;
      }

      size_t multi;
      if ((multi = data.find("__multiline__"sv)) != string::npos)
      {
        m_body.str("");
        m_body << data.substr(0, multi);
        m_terminator = data.substr(multi);
        return;
      }

      if (m_handler && m_handler->m_processData)
        m_handler->m_processData(data, m_context);
    }

    // Adapter private methods
    void Adapter::thread()
    {
      while (m_running)
      {
        try
        {
          // Start the connection to the socket
          connect();

          // make sure we're closed...
          close();
        }
        catch (std::invalid_argument &err)
        {
          g_logger << LERROR << "Adapter for " << m_deviceName
                   << "'s thread threw an argument error, stopping adapter: " << err.what();
          stop();
        }
        catch (std::exception &err)
        {
          g_logger << LERROR << "Adapter for " << m_deviceName
                   << "'s thread threw an exceotion, stopping adapter: " << err.what();
          stop();
        }
        catch (...)
        {
          g_logger << LERROR << "Thread for adapter " << m_deviceName
                   << "'s thread threw an unhandled exception, stopping adapter";
          stop();
        }

        if (!m_running)
          break;

        // Try to reconnect every 10 seconds
        g_logger << LINFO << "Will try to reconnect in " << m_reconnectInterval.count()
                 << " milliseconds";
        this_thread::sleep_for(m_reconnectInterval);
      }
      g_logger << LINFO << "Adapter thread stopped";
    }

  }  // namespace adapter
}  // namespace mtconnect
