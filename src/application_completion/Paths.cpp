#include "application_completion/Paths.hpp"
#include "application_completion/Commands.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace batbox::application_completion {
namespace {
namespace fs=std::filesystem;
std::string lower(std::string value){
  for(auto& ch:value)ch=static_cast<char>(std::tolower(
    static_cast<unsigned char>(ch)));
  return value;
}
bool pathLike(const std::string& token){
  return token=="~"||token=="."||token==".."||
    token.starts_with("~/")||token.starts_with("/")||
    token.starts_with("./")||token.starts_with("../");
}
fs::path expand(const std::string& partial,const std::string& cwd){
  if(partial.starts_with("~/")){
#ifdef _WIN32
    const auto* home=std::getenv("USERPROFILE");
#else
    const auto* home=std::getenv("HOME");
#endif
    if(home&&*home)return runtime::pathFromUtf8(home)/
      runtime::pathFromUtf8(partial.substr(2));
  }
  auto path=runtime::pathFromUtf8(partial);
  if(path.is_absolute())return path;
  return runtime::pathFromUtf8(cwd)/path;
}
std::string portion(const std::string& partial){
  const auto slash=partial.find_last_of("/\\");
  if(slash==std::string::npos)return {};
  auto result=partial.substr(0,slash+1);
  if(result.starts_with("./")||result.starts_with(".\\"))
    result.erase(0,2);
  return result;
}
}
std::optional<PathSpan> findPathSpan(
    const std::string& text,std::size_t caretUtf16){
  const auto before=value::slice(text,0,caretUtf16);
  const auto whitespace=before.find_last_of(" \t\r\n");
  const auto start=whitespace==std::string::npos?0:whitespace+1;
  const auto token=before.substr(start);
  if(token.empty())return {};
  std::string prefix,partial;
  bool includeFiles=true;
  if(token.starts_with('@')){
    prefix="@";partial=token.substr(1);
  }else if(pathLike(token)){
    partial=token;
    const auto leading=before.substr(0,start);
    if(leading=="!cd "||leading=="cd ")includeFiles=false;
  }else{
    const auto leading=before.substr(0,start);
    if(leading=="!cd "||leading=="cd "){
      partial=token;includeFiles=false;
    }else return {};
  }
  if(partial.find('"')!=std::string::npos)return {};
  std::size_t end=before.size();
  while(end<text.size()&&!std::isspace(
      static_cast<unsigned char>(text[end])))++end;
  return PathSpan{partial,prefix,value::stringLength(text.substr(0,start)),
    value::stringLength(text.substr(0,end)),includeFiles};
}
std::shared_ptr<Provider> Provider::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread()||!inputs.cwd||!inputs.commands)
    throw std::invalid_argument("Completion needs owner, cwd and commands");
  if(!inputs.log)inputs.log=[](const std::string&){};
  return std::shared_ptr<Provider>(new Provider(std::move(inputs)));
}
std::vector<Provider::PathEntry> Provider::scan(
    const std::string& directory,bool hidden){
  const auto key=directory+(hidden?":hidden":":visible");
  {
    std::lock_guard lock(stashMutex_);
    const auto found=pathStash_.find(key);
    if(found!=pathStash_.end()&&
        std::chrono::steady_clock::now()-found->second.loaded<
          std::chrono::minutes(5))
      return found->second.entries;
  }
  std::vector<PathEntry> entries;
  std::error_code error;
  for(fs::directory_iterator it(runtime::pathFromUtf8(directory),error),end;
      !error&&it!=end;it.increment(error)){
    const auto name=runtime::pathToUtf8(it->path().filename());
    if(name.empty()||(!hidden&&name.starts_with('.')))continue;
    const bool isDirectory=it->is_directory(error);
    if(error){error.clear();continue;}
    entries.push_back({name,isDirectory});
  }
  std::sort(entries.begin(),entries.end(),[](const PathEntry& left,
      const PathEntry& right){
    if(left.directory!=right.directory)return left.directory;
    return lower(left.name)<lower(right.name);
  });
  if(entries.size()>100)entries.resize(100);
  {
    std::lock_guard lock(stashMutex_);
    if(pathStash_.size()>=500)pathStash_.clear();
    pathStash_[key]={entries,std::chrono::steady_clock::now()};
  }
  return entries;
}
void Provider::clearPathStash(){
  std::lock_guard lock(stashMutex_);pathStash_.clear();
}
event_loop::Deferred<std::vector<Suggestion>> Provider::suggest(
    const std::string& text,std::size_t caretUtf16){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Completion owner required");
  event_loop::Deferred<std::vector<Suggestion>> output(inputs_.owner);
  try{
    if(const auto command=findCommandSpan(text,caretUtf16)){
      output.resolve(commandSuggestions(*command,inputs_.commands()));
      return output;
    }
    const auto span=findPathSpan(text,caretUtf16);
    if(!span){output.resolve({});return output;}
    const auto cwd=inputs_.cwd();
    const auto self=shared_from_this();
    runtime::contextThread([self,output,span=*span,cwd]{
      try{
        const auto resolved=expand(span.partial,cwd);
        const bool trailing=!span.partial.empty()&&
          (span.partial.ends_with('/')||span.partial.ends_with('\\'));
        const auto directory=trailing?resolved:resolved.parent_path();
        const auto prefix=trailing?std::string{}:
          runtime::pathToUtf8(resolved.filename());
        const auto includeHidden=!prefix.empty()&&prefix.starts_with('.');
        const auto entries=self->scan(runtime::pathToUtf8(directory),
          includeHidden);
        const auto wanted=lower(prefix);
        const auto dirPortion=portion(span.partial);
        std::vector<Suggestion> found;
        for(const auto& entry:entries){
          if(!span.includeFiles&&!entry.directory)continue;
          if(!lower(entry.name).starts_with(wanted))continue;
          const auto full=dirPortion+entry.name+
            (entry.directory?"/":"");
          Suggestion suggestion;
          suggestion.kind=entry.directory?Suggestion::Kind::Directory:
            Suggestion::Kind::Path;
          suggestion.display=full;
          suggestion.description=entry.directory?"directory":"file";
          suggestion.replacement=span.prefix+full;
          suggestion.startUtf16=span.startUtf16;
          suggestion.endUtf16=span.endUtf16;
          found.push_back(std::move(suggestion));
          if(found.size()>=10)break;
        }
        output.resolve(std::move(found));
      }catch(...){output.reject(std::current_exception());}
    }).detach();
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::application_completion
