#include "intl/Intl.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
using namespace batbox;
Json run(const Json& item){Json calls=Json::array(),out=Json::array();int seq=0;
 std::map<void*,int> ids;std::map<std::string,std::shared_ptr<void>> saved;
 intl::Services s;
 s.createSegmenter=[&](const std::string& granularity){calls.push_back(Json::array({"segmenter",nullptr,granularity}));
  const auto id=++seq;auto result=std::make_shared<intl::Segmenter>();ids[result.get()]=id;
  result->segment=[&,id](const std::string& text){calls.push_back(Json::array({"segment",id,text}));auto index=std::make_shared<std::size_t>(0);
   return intl::SegmentCursor{[&,index,id]()->std::optional<intl::Segment>{
    calls.push_back(Json::array({"next",id}));const auto parts=item.value("parts",Json::array());
    if(*index>=parts.size())return {};return intl::Segment{parts[(*index)++].get<std::string>(),"",0,{}};
   }};
  };return result;
 };
 s.createRelativeTimeFormat=[&](const std::string& locale,const std::string& style,const std::string& numeric){
  calls.push_back(Json::array({"rtf",locale,style,numeric}));auto result=std::make_shared<intl::RelativeTimeFormat>();ids[result.get()]=++seq;return result;
 };
 const auto resolve=[&]{calls.push_back(Json::array({"date"}));if(item.value("localeError",false))throw std::runtime_error("locale unavailable");calls.push_back(Json::array({"resolved"}));};
 s.resolvedTimeZone=[&]{resolve();return item.value("zone",std::string("America/New_York"));};
 s.resolvedLocale=[&]{resolve();return item.value("locale",std::string("en-US"));};
 s.localeLanguage=[&](const std::string& locale)->std::optional<std::string>{calls.push_back(Json::array({"locale",locale}));
  if(item.value("languageError",false))throw std::runtime_error("language unavailable");
  if(!item.contains("language")||item["language"].is_null())return {};return item["language"].get<std::string>();};
 intl::Stash stash(s);
 for(const auto& step:item["steps"]){try{
  Json value;std::shared_ptr<void> object;const auto op=step["op"].get<std::string>();
  if(op=="g")object=stash.getGraphemeSegmenter();
  else if(op=="w")object=stash.getWordSegmenter();
  else if(op=="first")value=stash.firstGrapheme(step["text"]);
  else if(op=="last")value=stash.lastGrapheme(step["text"]);
  else if(op=="rtf")object=stash.getRelativeTimeFormat(step["style"],step["numeric"]);
  else if(op=="tz")value=stash.getTimeZone();
  else if(op=="lang"){const auto lang=stash.getSystemLocaleLanguage();if(lang)value=*lang;}
  else throw std::runtime_error("unknown Intl step");
  if(object){const auto key=step.value("save",std::string());const bool same=!key.empty()&&saved[key]==object;if(!key.empty())saved[key]=object;
   value=Json{{"id",ids.at(object.get())},{"same",same}};
  }out.push_back(value);
 }catch(const std::exception& e){out.push_back(Json{{"error",e.what()}});}}
 return Json{{"results",out},{"calls",calls}};
}
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();for(const auto& item:input)out.push_back(run(item));std::cout<<value::stringify(out)<<'\n';}
