#include "intl/Intl.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <iterator>
using namespace batbox;
int main(){intl::Stash stash(intl::indigenousServices());const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();
 for(const auto& item:input){const auto op=item["op"].get<std::string>();
  if(op=="text"){
   const auto text=item["text"].get<std::string>();
   const auto segments=[&](std::shared_ptr<intl::Segmenter> s){Json result=Json::array();auto cursor=s->segment(text);
    while(const auto part=cursor.next()){Json entry={{"segment",part->text},{"index",part->index},{"input",part->input}};
     if(part->isWordLike)entry["isWordLike"]=*part->isWordLike;result.push_back(entry);
    }return result;
   };
   out.push_back(Json{{"first",stash.firstGrapheme(text)},{"last",stash.lastGrapheme(text)},
    {"graphemes",segments(stash.getGraphemeSegmenter())},{"words",segments(stash.getWordSegmenter())}});
  }else if(op=="format")out.push_back(stash.getRelativeTimeFormat(item["style"],item["numeric"])->format(item["value"],item["unit"]));
  else if(op=="tz")out.push_back(stash.getTimeZone());
  else if(op=="language"){const auto value=stash.getSystemLocaleLanguage();out.push_back(value?Json(*value):Json());}
 }
 std::cout<<value::stringify(out)<<'\n';
}
