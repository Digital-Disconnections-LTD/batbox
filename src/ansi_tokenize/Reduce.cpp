#include "ansi_tokenize/AnsiTokenize.hpp"
#include <algorithm>
#include <unordered_set>

namespace batbox::ansi_tokenize {
namespace {
bool endCode(const std::string& code){
  for(const int value:{0,22,23,24,55,27,28,29,39,49})
    if(code=="\x1b["+std::to_string(value)+"m")return true;
  return false;
}
}
Codes reduceAnsiCodesIncremental(const Codes& codes,const Codes& newCodes){
  Codes result=codes;
  for(const auto& code:newCodes){
    if(code->code=="\x1b[0m")result.clear();
    else if(endCode(code->code)){
      result.erase(std::remove_if(result.begin(),result.end(),[&](const CodeRef& active){
        return active->endCode==code->code;}),result.end());
    }else if(isIntensityCode(*code)){
      if(std::none_of(result.begin(),result.end(),[&](const CodeRef& active){
        return active->code==code->code&&active->endCode==code->endCode;}))
        result.push_back(code);
    }else{
      result.erase(std::remove_if(result.begin(),result.end(),[&](const CodeRef& active){
        return active->endCode==code->endCode;}),result.end());
      result.push_back(code);
    }
  }
  return result;
}
Codes reduceAnsiCodes(const Codes& codes){return reduceAnsiCodesIncremental({},codes);}
Codes undoAnsiCodes(const Codes& codes){
  const auto reduced=reduceAnsiCodes(codes);
  Codes result;
  result.reserve(reduced.size());
  for(auto it=reduced.rbegin();it!=reduced.rend();++it){
    auto copy=std::make_shared<AnsiCode>(**it);
    copy->code=copy->endCode;
    result.push_back(std::move(copy));
  }
  return result;
}
Codes diffAnsiCodes(const Codes& from,const Codes& to){
  std::unordered_set<std::string> endsInTo,startsInTo,startsInFrom;
  for(const auto& code:to){endsInTo.insert(code->endCode);startsInTo.insert(code->code);}
  for(const auto& code:from)startsInFrom.insert(code->code);
  Codes remove;
  for(const auto& code:from){
    if(isIntensityCode(*code)?!startsInTo.contains(code->code):
       !endsInTo.contains(code->endCode))remove.push_back(code);
  }
  Codes result=undoAnsiCodes(remove);
  for(const auto& code:to)if(!startsInFrom.contains(code->code))result.push_back(code);
  return result;
}
} // namespace batbox::ansi_tokenize
