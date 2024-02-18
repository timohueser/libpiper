#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <filesystem>
#include <string>

class FileManager {
public:
  FileManager() = default;
  static std::filesystem::path getDataSharePath();

private:
  static std::filesystem::path getLocalSharePath();
  static std::filesystem::path getSystemSpecificSharePath();
};

#endif // FILEMANAGER_H
