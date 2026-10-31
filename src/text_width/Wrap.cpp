#include "text_width/Layout.hpp"
#include <cmath>
namespace batbox::text_width {
std::string sliceFit(const std::string& text,double start,double end,const WrapServices& s){
  const auto value=s.sliceAnsi(text,start,end);
  return s.stringWidth(value)>end-start?s.sliceAnsi(text,start,end-1):value;
}
std::string truncate(const std::string& text,double columns,const std::string& position,const WrapServices& s){
  if(columns<1)return "";
  if(columns==1)return "…";
  const auto length=s.stringWidth(text);
  if(length<=columns)return text;
  if(position=="start")return "…"+sliceFit(text,length-columns+1,length,s);
  if(position=="middle"){
    const auto half=std::floor(columns/2);
    // Sequence concatenation explicitly; overloaded C++ '+' operands have
    // no left-to-right evaluation guarantee for imported function calls.
    const auto first=sliceFit(text,0,half,s);
    const auto last=sliceFit(text,length-(columns-half)+1,length,s);
    return first+"…"+last;
  }
  return sliceFit(text,0,columns-1,s)+"…";
}
std::string wrapText(const std::string& text,double maxWidth,const std::string& type,const WrapServices& s){
  if(type=="wrap")return s.wrapAnsi(text,maxWidth,false,true);
  if(type=="wrap-trim")return s.wrapAnsi(text,maxWidth,true,true);
  if(type.starts_with("truncate")){
    std::string position="end";
    if(type=="truncate-middle")position="middle";
    if(type=="truncate-start")position="start";
    return truncate(text,maxWidth,position,s);
  }
  return text;
}
}
