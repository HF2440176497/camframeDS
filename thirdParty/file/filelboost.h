#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

bool isDirExist(const std::string& path_name)
{
  if (boost::filesystem::exists(path_name) && boost::filesystem::is_directory(path_name))
  {
    return true;
  }
  return false;
}

bool createNewDir(const std::string& path_name)
{
  if (isDirExist(path_name))
  {
    return true;
  }
  return boost::filesystem::create_directories(path_name);
}

bool isFileExist(const std::string& file_name)
{
  if (boost::filesystem::exists(file_name) && boost::filesystem::is_regular_file(file_name))
  {
    return true;
  }
  return false;
}

bool createNewFile(const std::string& file_name)
{
  if (isFileExist(file_name))
  {
    return true;
  }
  boost::filesystem::ofstream file(file_name);
  file.close();
  return isFileExist(file_name);
}
