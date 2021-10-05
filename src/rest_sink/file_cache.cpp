//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/algorithm/string.hpp>

#include "file_cache.hpp"

#include "cached_file.hpp"
#include "logging.hpp"

using namespace std;

namespace mtconnect {
  namespace rest_sink {
    FileCache::FileCache(size_t max)
      : m_mimeTypes({{{".xsl", "text/xsl"},
                      {".xml", "text/xml"},
                      {".json", "application/json"},
                      {".js", "text/javascript"},
                      {".obj", "model/obj"},
                      {".stl", "model/stl"},
                      {".css", "text/css"},
                      {".xsd", "text/xml"},
                      {".jpg", "image/jpeg"},
                      {".jpeg", "image/jpeg"},
                      {".png", "image/png"},
                      {".html", "text/html"},
                      {".ico", "image/x-icon"}}}),
        m_maxCachedFileSize(max)
    {
      NAMED_SCOPE("file_cache");
    }

    namespace fs = std::filesystem;

    XmlNamespaceList FileCache::registerFiles(const string &uri, const string &pathName,
                                              const string &version)
    {
      return registerDirectory(uri, pathName, version);
    }

    // Register a file
    XmlNamespaceList FileCache::registerDirectory(const string &uri, const string &pathName,
                                                  const string &version)
    {
      XmlNamespaceList namespaces;

      try
      {
        fs::path path(pathName);

        if (!fs::exists(path))
        {
          LOG(warning) << "The following path " << pathName
                       << " cannot be found, full path: " << path;
        }
        else if (!fs::is_directory(path))
        {
          auto ns = registerFile(uri, path, version);
          if (ns)
            namespaces.emplace_back(*ns);
        }
        else
        {
          fs::path baseUri(uri, fs::path::format::generic_format);

          for (auto &file : fs::directory_iterator(path))
          {
            string name = (file.path().filename()).string();
            fs::path uri = baseUri / name;

            auto ns = registerFile(uri.generic_string(), (file.path()).string(), version);
            if (ns)
              namespaces.emplace_back(*ns);
          }
        }
      }
      catch (fs::filesystem_error e)
      {
        LOG(warning) << "The following path " << pathName << " cannot be accessed: " << e.what();
      }

      return namespaces;
    }

    std::optional<XmlNamespace> FileCache::registerFile(const std::string &uri,
                                                        const fs::path &path,
                                                        const std::string &version)
    {
      optional<XmlNamespace> ns;

      if (!fs::exists(path))
      {
        LOG(warning) << "The following path " << path
                     << " cannot be found, full path: " << fs::absolute(path);
        return nullopt;
      }
      else if (!fs::is_regular_file(path))
      {
        LOG(warning) << "The following path " << path
                     << " is not a regular file: " << fs::absolute(path);
        return nullopt;
      }

      // Make sure the uri is using /
      string gen;
      gen.resize(uri.size());
      replace_copy(uri.begin(), uri.end(), gen.begin(), L'\\', L'/');

      m_fileMap.emplace(gen, fs::absolute(path));
      string name = path.filename().string();

      // Check if the file name maps to a standard MTConnect schema file.
      if (!name.find("MTConnect") && name.substr(name.length() - 4u, 4u) == ".xsd" &&
          version == name.substr(name.length() - 7u, 3u))
      {
        string version = name.substr(name.length() - 7u, 3u);

        if (name.substr(9u, 5u) == "Error")
        {
          string urn = "urn:mtconnect.org:MTConnectError:" + version;
          ns = make_optional<XmlNamespace>(urn, uri);
        }
        else if (name.substr(9u, 7u) == "Devices")
        {
          string urn = "urn:mtconnect.org:MTConnectDevices:" + version;
          ns = make_optional<XmlNamespace>(urn, uri);
        }
        else if (name.substr(9u, 6u) == "Assets")
        {
          string urn = "urn:mtconnect.org:MTConnectAssets:" + version;
          ns = make_optional<XmlNamespace>(urn, uri);
        }
        else if (name.substr(9u, 7u) == "Streams")
        {
          string urn = "urn:mtconnect.org:MTConnectStreams:" + version;
          ns = make_optional<XmlNamespace>(urn, uri);
        }
      }

      return ns;
    }
    
    CachedFilePtr FileCache::findFileInDirectories(const std::string &name)
    {
      namespace fs = std::filesystem;
      
      for (const auto &dir : m_directories)
      {
        if (boost::starts_with(name, dir.first))
        {
          auto fileName = boost::erase_first_copy(name, dir.first);
          fs::path path = dir.second / fileName;
          if (fs::exists(path))
          {
            auto ext = path.extension().string();
            auto size = fs::file_size(path);
            auto file = make_shared<CachedFile>(path, getMimeType(ext),
                                           size <= m_maxCachedFileSize,
                                           size);
            m_fileCache.emplace(name, file);
            return file;
          }
          else
          {
            LOG(warning) << "Cannot find file: " << path;
          }        
        }
      }
      
      return nullptr;
    }

    CachedFilePtr FileCache::getFile(const std::string &name)
    {
      try
      {
        auto file = m_fileCache.find(name);
        if (file != m_fileCache.end())
        {
          return file->second;
        }
        else
        {
          auto path = m_fileMap.find(name);
          if (path != m_fileMap.end())
          {
            auto ext = path->second.extension().string();
            auto file = make_shared<CachedFile>(path->second, getMimeType(ext));
            m_fileCache.emplace(name, file);
            return file;
          }
          else
          {
            return findFileInDirectories(name);
          }
        }
      }
      catch (fs::filesystem_error e)
      {
        LOG(warning) << "Cannot open file " << name << ": " << e.what();
      }

      return nullptr;
    }
    
    void FileCache::addDirectory(const std::string &uri, const std::string &pathName)
    {
      fs::path path(pathName);
      if (fs::exists(path))
      {
        m_directories.emplace(uri, fs::canonical(path));
      }
      else
      {
        LOG(warning) << "Cannot find path " << pathName << " for " << uri;

      }
    }
  }  // namespace rest_sink
}  // namespace mtconnect
