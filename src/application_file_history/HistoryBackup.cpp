#include "application_file_history/HistoryInternal.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include <openssl/sha.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace batbox::application_file_history {
namespace {
std::string backupName(const std::filesystem::path& file,std::size_t version){
  const auto text=runtime::pathToUtf8(file);
  std::array<unsigned char,SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(text.data()),text.size(),
    digest.data());
  std::ostringstream out;out<<std::hex<<std::setfill('0');
  for(std::size_t index=0;index<8;++index)
    out<<std::setw(2)<<static_cast<unsigned>(digest[index]);
  out<<"@v"<<version;return out.str();
}
bool sameFile(const std::filesystem::path& left,
    const std::filesystem::path& right){
  std::ifstream a(left,std::ios::binary),b(right,std::ios::binary);
  if(!a||!b)return false;
  constexpr std::size_t size=16384;
  std::array<char,size> one{},two{};
  for(;;){
    a.read(one.data(),one.size());b.read(two.data(),two.size());
    const auto countA=a.gcount(),countB=b.gcount();
    if(countA!=countB||!std::equal(one.begin(),one.begin()+countA,two.begin()))
      return false;
    if(countA==0)return true;
  }
}
}
Json History::createBackup(const std::filesystem::path& file,
    std::size_t version,const std::string& flightId){
  const auto now=platform::isoTimestamp();
  std::error_code error;
  const auto status=std::filesystem::status(file,error);
  if(error==std::errc::no_such_file_or_directory||
      status.type()==std::filesystem::file_type::not_found)
    return {{"backupFileName",nullptr},{"version",version},
      {"backupTime",now}};
  if(error)throw std::filesystem::filesystem_error("stat file history source",
    file,error);
  const auto name=backupName(file,version);
  const auto target=backupPath(name,flightId);
  std::filesystem::create_directories(target.parent_path());
  std::filesystem::copy_file(file,target,
    std::filesystem::copy_options::overwrite_existing);
  std::filesystem::permissions(target,status.permissions(),
    std::filesystem::perm_options::replace);
  return {{"backupFileName",name},{"version",version},
    {"backupTime",now}};
}
bool History::changed(const std::filesystem::path& file,
    const std::string& backup,const std::string& flightId)const{
  const auto other=backupPath(backup,flightId);
  std::error_code leftError,rightError;
  const auto left=std::filesystem::status(file,leftError);
  const auto right=std::filesystem::status(other,rightError);
  const bool leftMissing=leftError==std::errc::no_such_file_or_directory||
    left.type()==std::filesystem::file_type::not_found;
  const bool rightMissing=rightError==std::errc::no_such_file_or_directory||
    right.type()==std::filesystem::file_type::not_found;
  if(leftError&&!leftMissing)return true;
  if(rightError&&!rightMissing)return true;
  if(leftMissing!=rightMissing)return true;
  if(leftMissing)return false;
  if(left.permissions()!=right.permissions())return true;
  std::error_code sizeError;
  const auto leftSize=std::filesystem::file_size(file,sizeError);
  if(sizeError)return true;
  const auto rightSize=std::filesystem::file_size(other,sizeError);
  if(sizeError||leftSize!=rightSize)return true;
  std::error_code timeError;
  const auto leftTime=std::filesystem::last_write_time(file,timeError);
  if(timeError)return true;
  const auto rightTime=std::filesystem::last_write_time(other,timeError);
  if(timeError)return true;
  if(leftTime<rightTime)return false;
  return !sameFile(file,other);
}
bool History::restoreBackup(const std::filesystem::path& file,
    const std::string& backup,const std::string& flightId)const{
  const auto source=backupPath(backup,flightId);
  std::error_code error;
  const auto status=std::filesystem::status(source,error);
  if(error==std::errc::no_such_file_or_directory||
      status.type()==std::filesystem::file_type::not_found)return false;
  if(error)throw std::filesystem::filesystem_error("stat file history backup",
    source,error);
  std::filesystem::create_directories(file.parent_path());
  std::filesystem::copy_file(source,file,
    std::filesystem::copy_options::overwrite_existing);
  std::filesystem::permissions(file,status.permissions(),
    std::filesystem::perm_options::replace);
  return true;
}
} // namespace batbox::application_file_history
