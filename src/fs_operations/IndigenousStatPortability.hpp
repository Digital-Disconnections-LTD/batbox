#pragma once
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <chrono>
#include <filesystem>
#include <system_error>

namespace batbox::fs_operations {
inline Json portableStat(const std::string& utf8,bool link){
  const auto path=runtime::pathFromUtf8(utf8);
  const auto status=link?std::filesystem::symlink_status(path):
    std::filesystem::status(path);
  if(status.type()==std::filesystem::file_type::not_found)
    throw std::system_error(std::make_error_code(
      std::errc::no_such_file_or_directory),"stat");
  const bool file=status.type()==std::filesystem::file_type::regular;
  const bool directory=status.type()==std::filesystem::file_type::directory;
  const bool symlink=status.type()==std::filesystem::file_type::symlink;
  const int mode=(static_cast<int>(status.permissions())&0777)|
    (file?0100000:directory?0040000:symlink?0120000:0);
  std::uintmax_t size=0;
  if(file)size=std::filesystem::file_size(path);
  else if(symlink)size=std::filesystem::read_symlink(path).u8string().size();
  const auto modified=std::filesystem::last_write_time(path);
  const auto current=std::filesystem::file_time_type::clock::now();
  const auto now=std::chrono::system_clock::now();
  const double milliseconds=static_cast<double>(std::chrono::duration_cast<
    std::chrono::milliseconds>((modified-current+now).time_since_epoch()).count());
  return Json::object({{"size",size},{"mode",mode},{"mtimeMs",milliseconds},
    {"isFile",file},{"isDirectory",directory},{"isSymbolicLink",symlink},
    {"isFIFO",false},{"isSocket",false},{"isCharacterDevice",false},
    {"isBlockDevice",false}});
}
} // namespace batbox::fs_operations
