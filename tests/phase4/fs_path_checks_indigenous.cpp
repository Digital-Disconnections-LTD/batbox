#include "fs_operations/IndigenousPathChecks.hpp"
#include "filesystem/FileOps.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main(){
  namespace fs=std::filesystem;
  const auto root=fs::canonical(fs::temp_directory_path())/
    ("batbox-path-checks-"+std::to_string(::getpid()));
  fs::create_directories(root/"target");
  {std::ofstream file(root/"target"/"file.txt");file<<"ok";}
  fs::create_symlink(root/"target"/"file.txt",root/"link.txt");
  fs::create_symlink(root/"target"/"new.txt",root/"dangling.txt");
  fs::create_directory_symlink(root/"target",root/"parent");
  const auto view=batbox::fs_operations::indigenousFileView();
  const auto path=batbox::fs_operations::indigenousPathImports(
    [root]{return root.string();},[root]{return root.string();});
  const auto link=(root/"link.txt").string();
  const auto canonical=(root/"target"/"file.txt").string();
  const auto resolved=batbox::filesystem::safeResolvePath(view,link);
  const auto direct=batbox::filesystem::safeResolvePath(link);
  assert(resolved.resolvedPath==canonical&&resolved.isSymlink&&resolved.isCanonical);
  assert(direct.resolvedPath==resolved.resolvedPath);
  const auto linked=batbox::fs_operations::getPathsForPermissionCheck(link,view,path);
  assert(linked.size()==2&&linked[0]==link&&linked[1]==canonical);
  const auto dangling=(root/"dangling.txt").string();
  const auto destination=(root/"target"/"new.txt").string();
  const auto pending=batbox::fs_operations::getPathsForPermissionCheck(dangling,view,path);
  assert(std::find(pending.begin(),pending.end(),destination)!=pending.end());
  const auto throughParent=(root/"parent"/"new.txt").string();
  const auto parentPaths=batbox::fs_operations::getPathsForPermissionCheck(
    throughParent,view,path);
  assert(std::find(parentPaths.begin(),parentPaths.end(),destination)!=parentPaths.end());
  const auto fifo=(root/"pipe").string();
  assert(::mkfifo(fifo.c_str(),0600)==0);
  const auto special=batbox::filesystem::safeResolvePath(view,fifo);
  assert(special.resolvedPath==fifo&&!special.isCanonical);
  fs::remove_all(root);
}
