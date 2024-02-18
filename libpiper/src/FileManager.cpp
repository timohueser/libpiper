#include "FileManager.hpp"
#include <cstdlib>
#include <iostream>

std::filesystem::path FileManager::getDataSharePath() {
  auto localPath = getLocalSharePath();
  if (std::filesystem::exists(localPath))
  {
    return localPath;
  }

  auto systemPath = getSystemSpecificSharePath();
  if (std::filesystem::exists(systemPath))
  {
    return systemPath;
  }

  // If neither path exists, print an error message.
  std::cerr << "Error: Neither local './share/' directory nor system-specific "
               "share directory exists."
            << std::endl;
  return std::filesystem::path(); // Return an empty path to indicate failure.
}

std::filesystem::path FileManager::getLocalSharePath() {
  return std::filesystem::path("./libpiper/share/");
}

std::filesystem::path FileManager::getSystemSpecificSharePath() {
#ifdef _WIN32
  auto appData = std::getenv("APPDATA");
  if (appData != nullptr)
  {
    return std::filesystem::path(appData) / "YourAppName/share";
  }
#elif defined(__APPLE__)
  auto home = std::getenv("HOME");
  if (home != nullptr)
  {
    return std::filesystem::path(home) / "Library/Application Support/YourAppName/share";
  }
#else
  auto dataHome = std::getenv("XDG_DATA_HOME");
  if (dataHome != nullptr)
  {
    return std::filesystem::path(dataHome) / "YourAppName/share";
  }
  else
  {
    auto home = std::getenv("HOME");
    if (home != nullptr)
    {
      return std::filesystem::path(home) / ".local/share/YourAppName/share";
    }
  }
#endif
  return std::filesystem::path(); // Return an empty path if environment
                                  // variables are not set.
}
