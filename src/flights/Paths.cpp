#include "flights/Paths.hpp"
#include "flights/IndigenousPath.hpp"
#include "runtime/Paths.hpp"
#include "shell/PosixPath.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <system_error>
namespace batbox::flights {
using shell::joinPosixPath;
std::string Paths::getProjectsDir()const{return joinPosixPath(services.getConfigHome(),"projects");}
std::string Paths::getTranscriptPath()const {
  const auto override=services.getFlightProjectDir();
  const auto project=override ? *override : services.getProjectDir(services.getOriginalCwd());
  const auto flight=services.getFlightId();
  return joinPosixPath(project,flight+".jsonl");
}
std::string Paths::getTranscriptPathForFlight(const std::string& flight)const {
  if(flight==services.getFlightId())return getTranscriptPath();
  return joinPosixPath(services.getProjectDir(services.getOriginalCwd()),flight+".jsonl");
}
std::string Paths::getFlightProjectTranscriptPath(const std::string& flight)const {
  const auto override=services.getFlightProjectDir();
  return joinPosixPath(override ? *override : services.getProjectDir(services.getOriginalCwd()),flight+".jsonl");
}
void Paths::setBatTranscriptSubdir(const std::string& bat,const std::string& subdir){subdirectories[bat]=subdir;}
void Paths::clearBatTranscriptSubdir(const std::string& bat){subdirectories.erase(bat);}
std::string Paths::getBatTranscriptPath(const std::string& bat)const {
  const auto override=services.getFlightProjectDir();
  const auto project=override ? *override : services.getProjectDir(services.getOriginalCwd());
  auto base=joinPosixPath(project,services.getFlightId());
  base=joinPosixPath(base,"subagents");
  if(const auto at=subdirectories.find(bat);at!=subdirectories.end() && !at->second.empty())base=joinPosixPath(base,at->second);
  return joinPosixPath(base,"agent-"+bat+".jsonl");
}
std::string Paths::getBatMetadataPath(const std::string& bat)const {
  auto path=getBatTranscriptPath(bat);path.replace(path.size()-6,6,".meta.json");return path;
}
bool isFsInaccessible(int error){
#if defined(_WIN32)
  return error==ENOENT || error==EACCES || error==EPERM || error==ENOTDIR;
#else
  return error==ENOENT || error==EACCES || error==EPERM || error==ENOTDIR || error==ELOOP;
#endif
}
bool isFsInaccessible(const std::error_code& error){
  return error==std::errc::no_such_file_or_directory||
    error==std::errc::permission_denied||
    error==std::errc::not_a_directory||
    error==std::errc::too_many_symbolic_link_levels;
}
void Paths::writeBatMetadata(const std::string& bat,const Json& metadata)const {
  const auto path=getBatMetadataPath(bat);
  std::filesystem::create_directories(indigenousPath(value::dirname(path)));
  std::ofstream output(indigenousPath(path),std::ios::binary|std::ios::trunc);
  if(!output)throw std::system_error(errno,std::generic_category(),"writeAgentMetadata");
  output<<value::stringify(metadata);output.close();
  if(!output)throw std::system_error(errno,std::generic_category(),"writeAgentMetadata");
}
std::optional<Json> Paths::readBatMetadata(const std::string& bat)const {
  const auto path=getBatMetadataPath(bat);
  std::ifstream input(indigenousPath(path),std::ios::binary);
  if(!input){const auto error=errno;if(isFsInaccessible(error))return std::nullopt;throw std::system_error(error,std::generic_category(),"readAgentMetadata");}
  const std::string raw((std::istreambuf_iterator<char>(input)),{});
  if(input.bad())throw std::system_error(errno,std::generic_category(),"readAgentMetadata");
  return value::parse(raw);
}
}
