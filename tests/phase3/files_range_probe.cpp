#include "filesystem/Range.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stop_token>
#include <string>

using batbox::Json;
static std::string formatFileSize(double bytes){
  const double kb=bytes/1024.0;
  if(kb<1)return batbox::value::stringify(Json(bytes))+" bytes";
  const auto fixed=[](double value,const std::string& unit){
    std::ostringstream out;out<<std::fixed<<std::setprecision(1)<<value;
    std::string text=out.str();if(text.size()>2&&text.substr(text.size()-2)==".0")text.resize(text.size()-2);
    return text+unit;};
  if(kb<1024)return fixed(kb,"KB");
  const double mb=kb/1024;if(mb<1024)return fixed(mb,"MB");
  return fixed(mb/1024,"GB");
}
static Json evaluate(const Json& t){
  std::stop_source stop;if(t.value("abort",false))stop.request_stop();
  const auto result=batbox::filesystem::readFileInRange(t.at("path").get<std::string>(),
    t.value("offset",0),t.contains("maxLines")&&t.at("maxLines").is_number_integer()
      ?std::optional<std::size_t>(t.at("maxLines").get<std::size_t>()):std::nullopt,
    t.contains("maxBytes")&&t.at("maxBytes").is_number_integer()
      ?std::optional<std::size_t>(t.at("maxBytes").get<std::size_t>()):std::nullopt,
    stop.get_token(),t.value("truncate",false),{formatFileSize});
  Json value={{"content",result.content},{"lineCount",result.lineCount},{"totalLines",result.totalLines},
    {"totalBytes",result.totalBytes},{"readBytes",result.readBytes},{"mtimeMs",std::floor(result.mtimeMs)}};
  if(result.truncatedByBytes)value["truncatedByBytes"]=true;
  return Json{{"result",value}};
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
  try{std::cout<<batbox::value::stringify(evaluate(batbox::value::parse(input)))<<'\n';}
  catch(const std::exception& error){
    const std::string message=error.what();
    const std::string name=message.rfind("File content (",0)==0?"FileTooLargeError":
      message=="This operation was aborted"?"AbortError":"Error";
    std::cout<<batbox::value::stringify(Json{{"error",message},{"name",name}})<<'\n';
  }
}
