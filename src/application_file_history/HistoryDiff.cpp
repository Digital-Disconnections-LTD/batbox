#include "application_file_history/HistoryInternal.hpp"
#include "application/Async.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace batbox::application_file_history {
namespace {
std::vector<std::string> lines(const std::string& text){
  std::vector<std::string> result;
  std::size_t start=0;
  while(start<text.size()){
    const auto end=text.find('\n',start);
    if(end==std::string::npos){result.push_back(text.substr(start));break;}
    result.push_back(text.substr(start,end-start+1));start=end+1;
  }
  return result;
}
std::pair<std::size_t,std::size_t> lineChanges(
    const std::string& original,const std::string& backup){
  const auto a=lines(original),b=lines(backup);
  const auto n=static_cast<std::ptrdiff_t>(a.size());
  const auto m=static_cast<std::ptrdiff_t>(b.size());
  const auto maximum=n+m;
  if(maximum==0)return {0,0};
  std::vector<std::ptrdiff_t> frontier(
    static_cast<std::size_t>(2*maximum+3),0);
  const auto offset=maximum+1;
  for(std::ptrdiff_t depth=0;depth<=maximum;++depth){
    for(std::ptrdiff_t diagonal=-depth;diagonal<=depth;diagonal+=2){
      const auto slot=static_cast<std::size_t>(offset+diagonal);
      const bool down=diagonal==-depth||
        (diagonal!=depth&&frontier[slot-1]<frontier[slot+1]);
      auto x=down?frontier[slot+1]:frontier[slot-1]+1;
      auto y=x-diagonal;
      while(x<n&&y<m&&a[static_cast<std::size_t>(x)]==
          b[static_cast<std::size_t>(y)]){++x;++y;}
      frontier[slot]=x;
      if(x>=n&&y>=m){
        const auto deletions=(depth+n-m)/2;
        const auto insertions=depth-deletions;
        return {static_cast<std::size_t>(insertions),
          static_cast<std::size_t>(deletions)};
      }
    }
  }
  return {b.size(),a.size()};
}
}
DiffStats History::diffStats(const Json& state,const Json& target,
    const std::string& flightId)const{
  DiffStats result;
  for(const auto& path:detail::tracked(state)){
    try{
      const auto file=expandPath(path);
      const auto backup=backupAt(state,target,path);
      if(!backup){
        report(std::make_exception_ptr(std::runtime_error(
          "FileHistory: Error finding the backup file to apply")));
        continue;
      }
      const auto originalContent=detail::readOrNull(file);
      const auto backupContent=*backup?
        detail::readOrNull(backupPath(**backup,flightId)):
        std::optional<std::string>{};
      const auto [added,removed]=lineChanges(originalContent.value_or(""),
        backupContent.value_or(""));
      std::error_code error;
      const bool zeroByteAdded=!(*backup)&&
        std::filesystem::exists(file,error);
      if(added||removed||zeroByteAdded){
        result.filesChanged.push_back(runtime::pathToUtf8(file));
        result.insertions+=added;result.deletions+=removed;
      }
    }catch(...){report(std::current_exception());}
  }
  return result;
}
event_loop::Deferred<std::optional<DiffStats>> History::getDiffStats(
    const std::string& messageId){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("File history diff requires owner loop");
  event_loop::Deferred<std::optional<DiffStats>> result(inputs_.owner);
  if(!enabled()){result.resolve(std::nullopt);return result;}
  const auto captured=state();
  const auto snapshot=findSnapshot(captured,messageId);
  if(!snapshot){result.resolve(std::nullopt);return result;}
  auto self=shared_from_this();
  const auto flightId=inputs_.flight->id();
  auto worker=application::runInWorker<DiffStats>(inputs_.owner,
    [self,captured,target=*snapshot,flightId]{
      return self->diffStats(captured,target,flightId);});
  worker.then([result](const DiffStats* value,const std::exception_ptr& error){
    if(error)result.reject(error);
    else result.resolve(*value);
  });
  return result;
}
} // namespace batbox::application_file_history
