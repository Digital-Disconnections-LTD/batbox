#include "mcp/OutputStorage.hpp"
#include "media/Media.hpp"
#include "formatting/Format.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <filesystem>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <stdexcept>
#include <system_error>

namespace batbox::mcp {
namespace {
std::string mimeBase(const std::optional<std::string>& mimeType){
  if(!mimeType)return "";
  const auto end=mimeType->find(';');
  auto text=mimeType->substr(0,end);
  const auto first=text.find_first_not_of(" \t\n\r\f\v");
  if(first==std::string::npos)return "";
  const auto last=text.find_last_not_of(" \t\n\r\f\v");
  text=text.substr(first,last-first+1);
  std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::tolower(c);});
  return text;
}
}
std::string extensionForMimeType(const std::optional<std::string>& mimeType){
  const auto mime=mimeBase(mimeType);
  const std::pair<const char*,const char*> types[]={
    {"application/pdf","pdf"},{"application/json","json"},{"text/csv","csv"},
    {"text/plain","txt"},{"text/html","html"},{"text/markdown","md"},
    {"application/zip","zip"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document","docx"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet","xlsx"},
    {"application/vnd.openxmlformats-officedocument.presentationml.presentation","pptx"},
    {"application/msword","doc"},{"application/vnd.ms-excel","xls"},
    {"audio/mpeg","mp3"},{"audio/wav","wav"},{"audio/ogg","ogg"},
    {"video/mp4","mp4"},{"video/webm","webm"},
    {"image/png","png"},{"image/jpeg","jpg"},{"image/gif","gif"},
    {"image/webp","webp"},{"image/svg+xml","svg"}};
  for(const auto& [source,ext]:types)if(mime==source)return ext;
  return "bin";
}
bool isBinaryContentType(const std::string& contentType){
  const auto mime=mimeBase(contentType);
  if(mime.empty()||mime.starts_with("text/"))return false;
  if(mime.ends_with("+json")||mime=="application/json")return false;
  if(mime.ends_with("+xml")||mime=="application/xml")return false;
  if(mime.starts_with("application/javascript")||
     mime=="application/x-www-form-urlencoded")return false;
  return true;
}
Json persistBinaryContent(const std::string& bytes,const std::optional<std::string>& mimeType,
  const std::string& persistId,const StorageConfig& config){
  if(!config.ensureGadgetResultsDir||!config.getGadgetResultsDir)
    throw std::invalid_argument("Missing MCP tool-results storage binding");
  config.ensureGadgetResultsDir();
  const auto ext=extensionForMimeType(mimeType);
  const auto path=runtime::pathToUtf8(runtime::pathFromUtf8(
    config.getGadgetResultsDir())/(persistId+"."+ext));
#ifdef _WIN32
  HANDLE file=::CreateFileW(runtime::pathFromUtf8(path).c_str(),GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return {{"error",std::system_error(
    static_cast<int>(::GetLastError()),std::system_category(),"open").what()}};
  try{
    for(std::size_t offset=0;offset<bytes.size();){
      const auto wanted=static_cast<DWORD>(std::min<std::size_t>(
        bytes.size()-offset,0xffffffffu));
      DWORD count=0;
      if(!::WriteFile(file,bytes.data()+offset,wanted,&count,nullptr))
        throw std::system_error(static_cast<int>(::GetLastError()),
          std::system_category(),"write");
      if(count==0)throw std::runtime_error("Short MCP result write");
      offset+=count;
    }
    if(!::CloseHandle(file))throw std::system_error(static_cast<int>(::GetLastError()),
      std::system_category(),"close");
  }catch(const std::exception& error){::CloseHandle(file);return {{"error",error.what()}};}
#else
  int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0666);
  if(fd<0)return {{"error",std::system_error(errno,std::generic_category(),"open").what()}};
  try{
    std::size_t offset=0;
    while(offset<bytes.size()){
      const auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);
      if(n<0&&errno==EINTR)continue;
      if(n<=0)throw std::system_error(errno,std::generic_category(),"write");
      offset+=static_cast<std::size_t>(n);
    }
    const int closed=::close(fd);fd=-1;
    if(closed!=0)throw std::system_error(errno,std::generic_category(),"close");
  }catch(const std::exception& error){if(fd>=0)::close(fd);return {{"error",error.what()}};}
#endif
  return {{"filepath",path},{"size",bytes.size()},{"ext",ext}};
}
std::string getBinaryBlobSavedMessage(const std::string& filepath,
  const std::optional<std::string>& mimeType,double size,const std::string& sourceDescription){
  return sourceDescription+"Binary content ("+mimeType.value_or("unknown type")+", "+
    formatting::formatFileSize(size)+") saved to "+filepath;
}
namespace {
std::string grouped(double value){
  auto source=value::numberToString(value);
  const auto point=source.find_first_of(".eE");
  const auto end=point==std::string::npos?source.size():point;
  const auto start=!source.empty()&&source[0]=='-'?1U:0U;
  for(std::size_t i=end;i>start+3;i-=3)source.insert(i-3,",");
  return source;
}
}
std::string getFormatDescription(const std::string& type,const std::string& schema){
  if(type=="toolResult")return "Plain text";
  if(type=="structuredContent")return schema.empty()?"JSON":"JSON with schema: "+schema;
  if(type=="contentArray")return schema.empty()?"JSON array":"JSON array with schema: "+schema;
  return "";
}
std::string getLargeOutputInstructions(const std::string& path,double contentLength,
  const std::string& formatDescription,std::optional<double> maxReadLength){
  std::string result="Error: result ("+grouped(contentLength)+
    " characters) exceeds maximum allowed tokens. Output has been saved to "+path+".\n"+
    "Format: "+formatDescription+"\n"+
    "Use offset and limit parameters to read specific portions of the file, search within it for specific content, and jq to make structured queries.\n"+
    "REQUIREMENTS FOR SUMMARIZATION/ANALYSIS/REVIEW:\n"+
    "- You MUST read the content from the file at "+path+" in sequential chunks until 100% of the content has been read.\n";
  if(maxReadLength&&*maxReadLength!=0)
    result+="- If you receive truncation warnings when reading the file (\"[N lines truncated]\"), reduce the chunk size until you have read 100% of the content without truncation ***DO NOT PROCEED UNTIL YOU HAVE DONE THIS***. Bash output is limited to "+grouped(*maxReadLength)+" chars.\n";
  else result+="- If you receive truncation warnings when reading the file, reduce the chunk size until you have read 100% of the content without truncation.\n";
  return result+"- Before producing ANY summary or analysis, you MUST explicitly describe what portion of the content you have read. ***If you did not read the entire content, you MUST explicitly state this.***\n";
}
void bindIndigenousResultServices(ResultServices& services,const StorageConfig& config){
  if(!config.ensureGadgetResultsDir||!config.getGadgetResultsDir||!config.nowMs||!config.randomSuffix)
    throw std::invalid_argument("Missing MCP result storage, clock or random binding");
  services.resizeImage=[](const std::string& bytes,std::size_t size,const std::string& ext){
    const auto resized=media::maybeResizeAndDownsampleImageBuffer(bytes,size,ext);
    return ResizedImage{resized.buffer,resized.mediaType};};
  services.persistBinaryContent=[config](const std::string& bytes,
      const std::optional<std::string>& mimeType,const std::string& id){
    return persistBinaryContent(bytes,mimeType,id,config);};
  services.nowMs=config.nowMs;
  services.randomSuffix=config.randomSuffix;
  services.hostedServerPrefix=config.hostedServerPrefix;
}
} // namespace batbox::mcp
