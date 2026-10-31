#include "intl/Intl.hpp"
namespace batbox::intl {
Stash::Stash(Services services):services_(std::move(services)){}
std::shared_ptr<Segmenter> Stash::getGraphemeSegmenter(){
  if(!grapheme_)grapheme_=services_.createSegmenter("grapheme");
  return grapheme_;
}
std::string Stash::firstGrapheme(const std::string& text){
  if(text.empty())return "";
  auto segments=getGraphemeSegmenter()->segment(text);
  const auto first=segments.next();return first?first->text:"";
}
std::string Stash::lastGrapheme(const std::string& text){
  if(text.empty())return "";
  std::string last;auto segments=getGraphemeSegmenter()->segment(text);
  while(const auto value=segments.next())last=value->text;
  return last;
}
std::shared_ptr<Segmenter> Stash::getWordSegmenter(){
  if(!word_)word_=services_.createSegmenter("word");
  return word_;
}
std::shared_ptr<RelativeTimeFormat> Stash::getRelativeTimeFormat(
    const std::string& style,const std::string& numeric){
  const auto key=style+":"+numeric;
  auto it=rtf_.find(key);auto value=it==rtf_.end()?nullptr:it->second;
  if(!value){value=services_.createRelativeTimeFormat("en",style,numeric);rtf_[key]=value;}
  return value;
}
std::string Stash::getTimeZone(){
  if(!timeZone_||timeZone_->empty())timeZone_=services_.resolvedTimeZone();
  return *timeZone_;
}
std::optional<std::string> Stash::getSystemLocaleLanguage(){
  if(!language_){
    try{const auto locale=services_.resolvedLocale();language_.emplace(services_.localeLanguage(locale));}
    catch(...){language_.emplace(std::nullopt);}
  }
  return *language_;
}
} // namespace batbox::intl
