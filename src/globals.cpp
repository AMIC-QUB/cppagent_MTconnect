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

#include "globals.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Don't include WinSock.h when processing <windows.h>
#ifdef _WINDOWS
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>
#endif

#if 0
#ifdef _WINDOWS
#include <psapi.h>
#endif
#endif

#ifdef _WINDOWS
#define localtime_r(t, tm) localtime_s(tm, t)
#define gmtime_r(t, tm) gmtime_s(tm, t)

#define DELTA_EPOCH_IN_MICROSECS 11644473600000000ull
#endif

using namespace std;
using namespace std::chrono;

namespace mtconnect
{
  string int64ToString(uint64_t i)
  {
    ostringstream stm;
    stm << i;
    return stm.str();
  }

  string intToString(unsigned int i)
  {
    ostringstream stm;
    stm << i;
    return stm.str();
  }

  string floatToString(double f)
  {
    char s[32] = {0};
    sprintf(s, "%.7g", f);
    return (string)s;
  }

  string toUpperCase(string &text)
  {
    for (char &c : text)
      c = toupper(c);

    return text;
  }

  bool isNonNegativeInteger(const string &s)
  {
    for (const char c : s)
    {
      if (!isdigit(c))
        return false;
    }

    return true;
  }

  std::string getCurrentTime(TimeFormat format)
  {
    return getCurrentTime(system_clock::now(), format);
  }

  std::string getCurrentTime(time_point<system_clock> timePoint, TimeFormat format)
  {
    constexpr char ISO_8601_FMT[] = "%Y-%m-%dT%H:%M:%SZ";

    switch (format)
    {
      case HUM_READ:
        return date::format("%a, %d %b %Y %H:%M:%S GMT", date::floor<seconds>(timePoint));
      case GMT:
        return date::format(ISO_8601_FMT, date::floor<seconds>(timePoint));
      case GMT_UV_SEC:
        return date::format(ISO_8601_FMT, date::floor<microseconds>(timePoint));
      case LOCAL:
        auto time = system_clock::to_time_t(timePoint);
        struct tm timeinfo = {0};
        localtime_r(&time, &timeinfo);
        char timestamp[64] = {0};
        strftime(timestamp, 50u, "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
        return timestamp;
    }

    return "";
  }

  template <class timePeriod>
  uint64_t getCurrentTimeIn()
  {
    return duration_cast<timePeriod>(system_clock::now().time_since_epoch()).count();
  }

  uint64_t getCurrentTimeInMicros()
  {
    return getCurrentTimeIn<microseconds>();
  }

  uint64_t getCurrentTimeInSec()
  {
    return getCurrentTimeIn<seconds>();
  }

  string getRelativeTimeString(uint64_t aTime)
  {
    char timeBuffer[50] = {0};
    time_t seconds;
    int micros;
    struct tm *timeinfo;

    seconds = aTime / 1000000ull;
    micros = aTime % 1000000ull;

    timeinfo = gmtime(&seconds);
    strftime(timeBuffer, 50, "%Y-%m-%dT%H:%M:%S", timeinfo);
    sprintf(timeBuffer + strlen(timeBuffer), ".%06dZ", micros);

    return string(timeBuffer);
  }

  void replaceIllegalCharacters(string &data)
  {
    for (auto i = 0u; i < data.length(); i++)
    {
      char c = data[i];

      switch (c)
      {
        case '&':
          data.replace(i, 1, "&amp;");
          break;

        case '<':
          data.replace(i, 1, "&lt;");
          break;

        case '>':
          data.replace(i, 1, "&gt;");
          break;
      }
    }
  }

  uint64_t parseTimeMicro(const std::string &aTime)
  {
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    char ms[16] = {0};

    int c = sscanf(aTime.c_str(), "%d-%d-%dT%d:%d:%d%15s", &timeinfo.tm_year, &timeinfo.tm_mon,
                   &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec,
                   (char *)&ms);

    if (c < 7)
      return 0;

    ms[15] = '\0';

    timeinfo.tm_mon -= 1;
    timeinfo.tm_year -= 1900;

#ifndef _WINDOWS
    auto existingTz = getenv("TZ");
    setenv("TZ", "UTC", 1);
#endif

    uint64_t time = (mktime(&timeinfo) - timezone) * 1000000ull;

#ifndef _WINDOWS
    if (existingTz)
      setenv("TZ", existingTz, 1);
#endif

    int ms_v = 0;
    auto len = strlen(ms);

    if (len > 0u)
    {
      ms_v = strtol(ms + 1, 0, 10);

      for (int pf = 7 - len; pf > 0; pf--)
        ms_v *= 10;
    }

    return time + ms_v;
  }

  static string::size_type insertPrefix(string &aPath, string::size_type &aPos,
                                        const string aPrefix)
  {
    aPath.insert(aPos, aPrefix);
    aPos += aPrefix.length();
    aPath.insert(aPos++, ":");

    return aPos;
  }

  static bool hasNamespace(const string &aPath, string::size_type aStart)
  {
    string::size_type len = aPath.length(), pos = aStart;

    while (pos < len)
    {
      if (aPath[pos] == ':')
        return true;
      else if (!isalpha(aPath[pos]))
        return false;

      pos++;
    }

    return false;
  }

  string addNamespace(const string aPath, const string aPrefix)
  {
    if (aPrefix.empty())
      return aPath;

    string newPath = aPath;
    string::size_type pos = 0u;

    // Special case for relative pathing
    if (newPath[pos] != '/')
    {
      if (!hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    while ((pos = newPath.find('/', pos)) != string::npos && pos < newPath.length() - 1)
    {
      pos++;

      if (newPath[pos] == '/')
        pos++;

      // Make sure there are no existing prefixes
      if (newPath[pos] != '*' && newPath[pos] != '\0' && !hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    pos = 0u;

    while ((pos = newPath.find('|', pos)) != string::npos)
    {
      pos++;

      if (newPath[pos] != '/' && !hasNamespace(newPath, pos))
        insertPrefix(newPath, pos, aPrefix);
    }

    return newPath;
  }

  bool isMTConnectUrn(const char *aUrn)
  {
    return !strncmp(aUrn, "urn:mtconnect.org:MTConnect", 27u);
  }

#if 0
  long getMemorySize()
  {
    long size = 0l;

#ifdef _WINDOWS
    HANDLE myself = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS memory;
    
    if (GetProcessMemoryInfo(myself, &memory, sizeof(memory)))
      size = (long)(memory.PeakWorkingSetSize / 1024l);

#else
    struct rusage memory;
    
    if (!getrusage(RUSAGE_SELF, &memory))
      size = memory.ru_maxrss;

#endif
    
    return size;
  }
#endif
}  // namespace mtconnect
