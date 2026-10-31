#include "text_width/Layout.hpp"
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
using namespace batbox;
Json run(const Json& item){Json calls=Json::array(),results=Json::array();int count=0,slices=0;
 const auto width=[&](const std::string& text){++count;calls.push_back(Json::array({"width",text}));
  if(item.value("throwOn",std::string())==text&&item.contains("throwOn"))throw std::runtime_error("width failed");
  const auto values=item.value("widths",Json::object());if(values.contains(text)){
   if(values[text]=="nan")return std::numeric_limits<double>::quiet_NaN();return values[text].get<double>();
  }return static_cast<double>(value::stringLength(text));
 };
 text_width::LineWidthStash stash(width);text_width::WrapServices s;
 s.stringWidth=width;s.sliceAnsi=[&](const std::string& text,double start,double end){calls.push_back(Json::array({"slice",text,start,end}));
  return value::slice(text,start,end)+(item.value("overshoot",false)&&slices++==0?"X":"");};
 s.wrapAnsi=[&](const std::string& text,double columns,bool trim,bool hard){calls.push_back(Json::array({"wrap",text,columns,Json{{"trim",trim},{"hard",hard}}}));
  return std::string("<")+(trim?"true":"false")+":"+text+">";};
 for(const auto& step:item["steps"]){try{Json result;double columns=0;
  if(step.contains("width"))columns=step["width"]=="inf"?std::numeric_limits<double>::infinity():step["width"]=="nan"?std::numeric_limits<double>::quiet_NaN():step["width"].get<double>();
  const auto op=step["op"].get<std::string>();
  if(op=="line")result=stash.lineWidth(step["text"]);
  else if(op=="fill")for(int i=0;i<step["count"].get<int>();++i)stash.lineWidth("k"+std::to_string(i));
  else if(op=="measure"){const auto m=text_width::measureText(step["text"],columns,[&](const std::string& line){return stash.lineWidth(line);});result=Json{{"width",m.width},{"height",m.height}};}
  else if(op=="wrap")result=text_width::wrapText(step["text"],columns,step["type"],s);
  else if(op=="truncate")result=text_width::truncate(step["text"],columns,step["position"],s);
  else if(op=="fit")result=text_width::sliceFit(step["text"],step["start"],step["end"],s);
  else throw std::runtime_error("unknown layout step");
  results.push_back(result);
 }catch(const std::exception& e){results.push_back(Json{{"error",e.what()}});}}
 Json recent=Json::array();for(std::size_t i=calls.size()>12?calls.size()-12:0;i<calls.size();++i)recent.push_back(calls[i]);
 return Json{{"results",results},{"count",count},{"calls",recent}};
}
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();for(const auto& item:input)out.push_back(run(item));std::cout<<value::stringify(out)<<'\n';}
