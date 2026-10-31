#include <cstdlib>
#include "flights/ProjectDiscovery.hpp"
#include "runtime/PathSanitization.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
using namespace batbox;
template<class T>T wait(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> result;std::exception_ptr error;pending.then([&](const T* value,const auto& failure){error=failure;if(value)result.emplace(*value);});
  loop.run();if(error)std::rethrow_exception(error);assert(result);return *result;
}
int main(){
  char directory[]="/tmp/batbox-project-discovery-XXXXXX";assert(::mkdtemp(directory));const std::string root=std::filesystem::canonical(directory).string();
  event_loop::Loop loop;auto services=flights::indigenousProjectDiscoveryServices(loop);
  auto dirs=std::make_shared<flights::ProjectDirectories>([&]{return root;});
  std::filesystem::create_directories(root+"/target");std::filesystem::create_directory_symlink(root+"/target",root+"/link");
  assert(wait(loop,flights::canonicalizePath(loop,services,root+"/link"))==root+"/target");
  const auto missing=root+"/cafe\xcc\x81";
  assert(wait(loop,flights::canonicalizePath(loop,services,missing))==root+"/caf\xc3\xa9");
  const auto nul=root+std::string("\0suffix",7);assert(wait(loop,flights::canonicalizePath(loop,services,nul))==nul);
  std::filesystem::create_directories(dirs->getProjectDirPortable("/short"));
  assert(wait(loop,flights::findProjectDir(loop,services,dirs,"/short"))==dirs->getProjectDirPortable("/short"));
  const auto longPath=std::string(220,'x'),prefix=std::string(200,'x');
  std::ofstream(dirs->getProjectsDir()+"/"+prefix+"-0-file")<<"skip";
  std::filesystem::create_directory_symlink(root+"/target",dirs->getProjectsDir()+"/"+prefix+"-1-link");
  std::filesystem::create_directory(dirs->getProjectsDir()+"/"+prefix+"-b");
  std::filesystem::create_directory(dirs->getProjectsDir()+"/"+prefix+"-a");
  assert(wait(loop,flights::findProjectDir(loop,services,dirs,longPath))==dirs->getProjectsDir()+"/"+prefix+"-a");
  std::filesystem::remove_all(root);
}
