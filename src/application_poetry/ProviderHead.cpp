#include "application_poetry/Provider.hpp"
#include "application_poetry/Stamp.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace batbox::application_poetry {
namespace {
// Head files are instructions and notes, normally a few KiB. Larger files
// are compared and shown up to this many bytes.
constexpr std::size_t readLimit=512*1024;
// Up to this size the whole new file is attached; above it, a diff when that
// is shorter. The attachment never exceeds attachmentLimit.
constexpr std::size_t wholeFileLimit=4096;
constexpr std::size_t attachmentLimit=40000;

std::size_t utf8Cut(const std::string& text,std::size_t limit){
  if(text.size()<=limit)return text.size();
  auto cut=limit;
  while(cut>0&&(static_cast<unsigned char>(text[cut])&0xC0)==0x80)--cut;
  return cut;
}
std::string capped(const std::string& text){
  const auto cut=utf8Cut(text,attachmentLimit);
  if(cut==text.size())return text;
  return text.substr(0,cut)+"\n\n[... "+std::to_string(text.size()-cut)+
    " more bytes not shown; Read the file for the rest]";
}
std::optional<std::string> readText(const std::filesystem::path& path){
  std::ifstream input(path,std::ios::binary);
  if(!input)return std::nullopt;
  std::string text(readLimit,'\0');
  input.read(text.data(),static_cast<std::streamsize>(text.size()));
  if(input.bad())return std::nullopt;
  text.resize(static_cast<std::size_t>(input.gcount()));
  return text;
}
std::vector<std::string_view> lines(const std::string& text){
  std::vector<std::string_view> result;
  std::size_t start=0;
  while(start<text.size()){
    const auto end=text.find('\n',start);
    result.emplace_back(text.data()+start,(end==std::string::npos?text.size():end)-start);
    if(end==std::string::npos)break;
    start=end+1;
  }
  return result;
}
// One unified hunk around the changed lines (common prefix and suffix removed).
std::string lineDiff(const std::string& before,const std::string& after){
  const auto old=lines(before),fresh=lines(after);
  std::size_t prefix=0;
  while(prefix<old.size()&&prefix<fresh.size()&&old[prefix]==fresh[prefix])++prefix;
  if(prefix==old.size()&&prefix==fresh.size())return {};
  std::size_t suffix=0;
  while(suffix<old.size()-prefix&&suffix<fresh.size()-prefix&&
      old[old.size()-1-suffix]==fresh[fresh.size()-1-suffix])++suffix;
  const auto before3=std::min<std::size_t>(3,prefix),after3=std::min<std::size_t>(3,suffix);
  const auto oldCount=old.size()-prefix-suffix,newCount=fresh.size()-prefix-suffix;
  std::string diff="@@ -"+std::to_string(prefix-before3+1)+","+
    std::to_string(oldCount+before3+after3)+" +"+std::to_string(prefix-before3+1)+","+
    std::to_string(newCount+before3+after3)+" @@\n";
  const auto add=[&diff](char mark,std::string_view line){
    diff+=mark;diff.append(line.data(),line.size());diff+='\n';};
  for(auto i=prefix-before3;i<prefix;++i)add(' ',old[i]);
  for(auto i=prefix;i<old.size()-suffix;++i)add('-',old[i]);
  for(auto i=prefix;i<fresh.size()-suffix;++i)add('+',fresh[i]);
  for(auto i=old.size()-suffix;i<old.size()-suffix+after3;++i)add(' ',old[i]);
  return diff;
}
std::string changeText(const std::optional<std::string>& before,
    const std::optional<std::string>& after){
  if(!after)return "This file was deleted after it was loaded at the start of this "
    "Flight. Disregard the earlier copy.";
  if(!before)return "This file was created after this Flight started. "
    "Its contents:\n\n"+capped(*after);
  if(after->size()>wholeFileLimit){
    const auto diff=lineDiff(*before,*after);
    if(!diff.empty()&&diff.size()<after->size())
      return "This file changed after it was loaded at the start of this Flight. "
        "The earlier copy is out of date; the changes as a unified diff "
        "(lines starting with - were removed, + were added):\n\n"+capped(diff);
  }
  return "This file changed after it was loaded at the start of this Flight. "
    "Its current contents, which replace the earlier copy:\n\n"+capped(*after);
}
}
bool Provider::stablePromptPrefix()const{
  const auto current=settings();
  const auto& value=value::property(current,"stablePromptPrefix");
  return !value.is_boolean()||value.get<bool>();
}
std::string Provider::headKey(const std::string& path)const{
  auto value=runtime::pathFromUtf8(path);
  if(value.is_relative())value=inputs_.config.cwd/value;
  std::error_code error;
  auto resolved=std::filesystem::weakly_canonical(value,error);
  return runtime::pathToUtf8(error?value.lexically_normal():resolved);
}
void Provider::trackHeadFile(const std::string& path){
  if(path.empty())return;
  auto key=headKey(path);
  if(headFiles_.contains(key))return;
  const auto file=runtime::pathFromUtf8(key);
  auto stamp=fileStamp(file);
  auto content=stamp=="missing"?std::nullopt:readText(file);
  headFiles_.emplace(std::move(key),HeadFile{std::move(stamp),std::move(content)});
}
void Provider::resetHeadSnapshot(){
  headFiles_.clear();ownWrites_.clear();
  systemHeadFrozen_=false;
}
void Provider::noteMainModel(const std::string& model){
  if(!owner_.isOwnerThread())throw std::logic_error("Model change requires owner loop");
  if(model.empty())return;
  // A different model or endpoint has its own server cache; rebuild the head
  // and sections for it rather than keep the previous model's snapshot.
  if(!headModel_.empty()&&headModel_!=model&&stablePromptPrefix()){
    clearProjectInstructions();clearPoetrySections();
  }
  headModel_=model;
}
void Provider::noteOwnWrite(const std::string& path){
  if(!owner_.isOwnerThread())throw std::logic_error("Head tracking requires owner loop");
  if(path.empty())return;
  auto key=headKey(path);
  if(!headFiles_.contains(key))return;
  ownWrites_.insert_or_assign(key,fileStamp(runtime::pathFromUtf8(key)));
}
Json Provider::takeHeadChangeAttachments(
    const std::function<bool(const std::string&)>& coveredElsewhere){
  if(!owner_.isOwnerThread())throw std::logic_error("Head tracking requires owner loop");
  Json attachments=Json::array();
  if(headFiles_.empty()||!stablePromptPrefix())return attachments;
  const auto base=inputs_.config.cwd.lexically_normal();
  for(auto& [key,file]:headFiles_){
    const auto path=runtime::pathFromUtf8(key);
    auto stamp=fileStamp(path);
    if(stamp==file.stamp)continue;
    auto content=stamp=="missing"?std::nullopt:readText(path);
    const auto own=ownWrites_.find(key);
    const bool known=own!=ownWrites_.end()&&own->second==stamp;
    if(own!=ownWrites_.end())ownWrites_.erase(own);
    const bool unchanged=content==file.content;
    if(!known&&!unchanged&&!(coveredElsewhere&&coveredElsewhere(key))){
      const auto relative=path.lexically_normal().lexically_relative(base);
      const bool inside=!relative.empty()&&*relative.begin()!="..";
      const auto display=inside?runtime::pathToUtf8(relative):
        poetryEntries::displayPath(path);
      attachments.push_back(Json{{"type","nested_memory"},{"path",key},
        {"content",{{"path",key},{"type","Project"},
          {"content",changeText(file.content,content)}}},
        {"displayPath",display}});
    }
    file=HeadFile{std::move(stamp),std::move(content)};
  }
  return attachments;
}
} // namespace batbox::application_poetry
