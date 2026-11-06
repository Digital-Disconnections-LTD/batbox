#include "attachments/local/LocalCollectors.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace batbox::attachments::local {
namespace {
std::vector<std::string_view> splitLines(const std::string& text){
  std::vector<std::string_view> lines;
  std::size_t start=0;
  while(start<text.size()){
    const auto end=text.find('\n',start);
    lines.emplace_back(text.data()+start,(end==std::string::npos?text.size():end)-start);
    if(end==std::string::npos)break;
    start=end+1;
  }
  return lines;
}
}
Json lineHunks(const std::string& before,const std::string& after,int context){
  const auto old=splitLines(before),fresh=splitLines(after);
  std::size_t prefix=0;
  while(prefix<old.size()&&prefix<fresh.size()&&old[prefix]==fresh[prefix])++prefix;
  if(prefix==old.size()&&prefix==fresh.size())return Json::array();
  std::size_t suffix=0;
  while(suffix<old.size()-prefix&&suffix<fresh.size()-prefix&&
      old[old.size()-1-suffix]==fresh[fresh.size()-1-suffix])++suffix;
  const auto around=static_cast<std::size_t>(std::max(context,0));
  const auto lead=std::min(around,prefix),trail=std::min(around,suffix);
  Json body=Json::array();
  for(auto i=prefix-lead;i<prefix;++i)body.push_back(" "+std::string(old[i]));
  for(auto i=prefix;i<old.size()-suffix;++i)body.push_back("-"+std::string(old[i]));
  for(auto i=prefix;i<fresh.size()-suffix;++i)body.push_back("+"+std::string(fresh[i]));
  for(auto i=old.size()-suffix;i<old.size()-suffix+trail;++i)
    body.push_back(" "+std::string(old[i]));
  return Json::array({{{"oldStart",prefix-lead+1},
    {"oldLines",old.size()-prefix-suffix+lead+trail},{"newStart",prefix-lead+1},
    {"newLines",fresh.size()-prefix-suffix+lead+trail},{"lines",std::move(body)}}});
}
Services makeIndigenousLocalServices(event_loop::Loop& loop,
    std::shared_ptr<files::Services> fileServices,
    const gadgets::files::FileReadServices& readServices){
  if(!fileServices||!fileServices->getAppState)
    throw std::invalid_argument("Native changed-file services required");
  Services services(loop);
  services.files=std::move(fileServices);
  services.getAppState=services.files->getAppState;
  services.structuredPatch=[](const std::string&,const std::string& before,
      const std::string& after,int context){
    return Json{{"hunks",lineHunks(before,after,context)}};};
  services.readImageWithTokenBudget=[&loop,image=readServices.image,
      limit=readServices.defaultLimits.maxTokens](const std::string& path){
    event_loop::Deferred<Json> output(loop);
    runtime::contextThread([output,path,image,limit]{
      try{output.resolve(gadgets::files::readImageWithTokenBudget(
        path,limit,std::nullopt,image));}
      catch(...){output.reject(std::current_exception());}
    }).detach();
    return output;
  };
  return services;
}
} // namespace batbox::attachments::local
