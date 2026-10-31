#include "npm_wrap_ansi/Internal.hpp"
#include <cmath>
#include <utility>

namespace batbox::npm_wrap_ansi {
namespace {
std::vector<std::u16string> splitSpaces(std::u16string_view text){
  std::vector<std::u16string> words;std::size_t start=0;
  while(start<=text.size()){
    const auto end=text.find(u' ',start);
    if(end==std::u16string_view::npos){words.emplace_back(text.substr(start));break;}
    words.emplace_back(text.substr(start,end-start));start=end+1;
  }
  return words;
}
std::vector<std::u16string> codepoints(std::u16string_view text){
  std::vector<std::u16string> result;
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]>=0xd800&&text[i]<=0xdbff&&i+1<text.size()&&
       text[i+1]>=0xdc00&&text[i+1]<=0xdfff)
      result.emplace_back(text.substr(i++,2));
    else result.emplace_back(text.substr(i,1));
  }
  return result;
}
}
void wrapWord(Rows& rows,std::u16string_view word,double columns){
  const auto characters=codepoints(word);
  bool insideEscape=false,insideLinkEscape=false;
  double visible=width(stripAnsi(rows.back()));
  for(std::size_t index=0;index<characters.size();++index){
    const auto& character=characters[index];
    const double length=width(character);
    if(visible+length<=columns)rows.back()+=character;
    else{rows.push_back(character);visible=0;}
    if(character==u"\x1b"||character==u"\x9b"){
      insideEscape=true;
      std::u16string candidate;
      for(std::size_t i=index+1;i<characters.size()&&i<index+5;++i)candidate+=characters[i];
      insideLinkEscape=candidate==u"]8;;";
    }
    if(insideEscape){
      if(insideLinkEscape){
        if(character==u"\x07")insideEscape=insideLinkEscape=false;
      }else if(character==u"m")insideEscape=false;
      continue;
    }
    visible+=length;
    if(visible==columns&&index<characters.size()-1){rows.emplace_back();visible=0;}
  }
  if(visible==0&&!rows.back().empty()&&rows.size()>1){
    auto last=std::move(rows.back());rows.pop_back();rows.back()+=last;
  }
}
std::u16string trimVisibleSpacesRight(std::u16string_view text){
  const auto words=splitSpaces(text);std::size_t last=words.size();
  while(last>0){if(width(words[last-1])>0)break;--last;}
  if(last==words.size())return std::u16string(text);
  std::u16string result;
  for(std::size_t i=0;i<last;++i){if(i)result+=u' ';result+=words[i];}
  for(std::size_t i=last;i<words.size();++i)result+=words[i];
  return result;
}
std::u16string wrapLine(std::u16string_view line,double columns,const Options& options){
  if(options.trim&&blank(line))return {};
  const auto words=splitSpaces(line);
  std::vector<double> lengths;lengths.reserve(words.size());
  for(const auto& word:words)lengths.push_back(width(word));
  Rows rows{u""};
  for(std::size_t index=0;index<words.size();++index){
    if(options.trim)rows.back()=trimStart(rows.back());
    double rowLength=width(rows.back());
    if(index!=0){
      if(rowLength>=columns&&(!options.wordWrap||!options.trim)){
        rows.emplace_back();rowLength=0;
      }
      if(rowLength>0||!options.trim){rows.back()+=u' ';rowLength++;}
    }
    if(options.hard&&lengths[index]>columns){
      const auto remaining=columns-rowLength;
      const auto breaksThis=1+std::floor((lengths[index]-remaining-1)/columns);
      const auto breaksNext=std::floor((lengths[index]-1)/columns);
      if(breaksNext<breaksThis)rows.emplace_back();
      wrapWord(rows,words[index],columns);continue;
    }
    if(rowLength+lengths[index]>columns&&rowLength>0&&lengths[index]>0){
      if(!options.wordWrap&&rowLength<columns){
        wrapWord(rows,words[index],columns);continue;
      }
      rows.emplace_back();
    }
    if(rowLength+lengths[index]>columns&&!options.wordWrap){
      wrapWord(rows,words[index],columns);continue;
    }
    rows.back()+=words[index];
  }
  if(options.trim)for(auto& row:rows)row=trimVisibleSpacesRight(row);
  std::u16string pre;
  for(std::size_t i=0;i<rows.size();++i){if(i)pre+=u'\n';pre+=rows[i];}
  return preserveAnsi(pre);
}
}
