#include "application_editor/PoetryEditor.hpp"
#include "application_editor/IndigenousProcess.hpp"
#include "history/References.hpp"
#include "runtime/Platform.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace batbox::application_editor {
namespace {
void writeTemp(const std::filesystem::path& path,const std::string& text){
#ifdef _WIN32
  const int fd=_wopen(path.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,
    _S_IREAD|_S_IWRITE);
#else
  const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
#endif
  if(fd<0)throw std::runtime_error("Could not create external editor temp file");
  try{
    std::size_t offset=0;
    while(offset<text.size()){
#ifdef _WIN32
      const auto written=_write(fd,text.data()+offset,
        static_cast<unsigned>(std::min<std::size_t>(text.size()-offset,1<<20)));
#else
      const auto written=::write(fd,text.data()+offset,text.size()-offset);
      if(written<0&&errno==EINTR)continue;
#endif
      if(written<=0)throw std::runtime_error("Could not write editor temp file");
      offset+=static_cast<std::size_t>(written);
    }
#ifdef _WIN32
    if(_commit(fd)!=0)throw std::runtime_error("Could not sync editor temp file");
    _close(fd);
#else
    if(::fsync(fd)!=0)throw std::runtime_error("Could not sync editor temp file");
    ::close(fd);
#endif
  }catch(...){
#ifdef _WIN32
    _close(fd);
#else
    ::close(fd);
#endif
    throw;
  }
}
std::string readTemp(const std::filesystem::path& path){
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("Could not read editor temp file");
  return std::string(std::istreambuf_iterator<char>{stream},{});
}
std::string recollapse(std::string text,const Json& pasted){
  if(!pasted.is_object())return text;
  std::vector<std::pair<int,Json>> rows;
  for(auto it=pasted.begin();it!=pasted.end();++it){
    if(value::property(it.value(),"type")!="text")continue;
    try{rows.emplace_back(std::stoi(it.key()),it.value());}catch(...){continue;}
  }
  std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){
    return a.first<b.first;});
  for(const auto& [id,content]:rows){
    const auto& raw=value::property(content,"content");
    if(!raw.is_string())continue;
    const auto original=raw.get<std::string>();
    const auto position=text.find(original);
    if(position==std::string::npos)continue;
    const auto reference=history::formatPastedTextRef(id,
      history::getPastedTextRefNumLines(original));
    text.replace(position,original.size(),reference);
  }
  return text;
}
}
EditResult editPoetryInEditor(const std::string& currentPoetry,
    const Json& pastedContents){
  const auto path=std::filesystem::temp_directory_path()/
    ("batbox-prompt-"+platform::randomUUID()+".md");
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}
  } cleanup{path};
  const auto expanded=history::expandPastedTextRefs(currentPoetry,pastedContents);
  writeTemp(path,expanded);
  const auto editor=indigenous::externalEditor();
  if(!editor)return {};
  const auto status=indigenous::runInherited(*editor,path.string());
  if(status.error)return {};
  if(status.exitCode!=0)return {{},editor->displayName+
    " exited with code "+std::to_string(status.exitCode)};
  std::string content;
  try{content=readTemp(path);}
  catch(...){return {};}
  if(content.ends_with('\n')&&!content.ends_with("\n\n"))content.pop_back();
  return {recollapse(std::move(content),pastedContents),{}};
}
}
