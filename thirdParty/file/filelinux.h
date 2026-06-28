#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "glog_common.h"
bool isDirExist(const std::string& path_name)
{
  if (access(path_name.data(),0)==0)
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
  LOG(INFO)<<"createNewDir"<<path_name;
  return mkdir(path_name.data(), S_IRWXU);;
}


