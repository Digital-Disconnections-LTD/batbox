#include "gadgets/files/FileRead.hpp"
#include "formatting/Format.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  files::FileReadServices s;
  s.formatFileSize=batbox::formatting::formatFileSize;
  s.compactLinePrefixEnabled=[&](){return t.value("compact",false);};
  s.shouldIncludeFileReadMitigation=[](){return true;};
  s.memoryFreshnessNote=[](double value){return "mtime:"+std::to_string(static_cast<long long>(value))+"\n";};
  s.mapNotebookCellsToGadgetResult=[](const Json& cells,const std::string& id){
    return Json{{"tool_use_id",id},{"type","tool_result"},{"content",cells}};};
  const auto mtime=t.contains("memoryMtimeMs")?std::optional<double>(t.at("memoryMtimeMs").get<double>()):std::nullopt;
  return files::mapReadResult(t.at("data"),t.value("toolUseID",std::string("use-1")),s,mtime);
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
