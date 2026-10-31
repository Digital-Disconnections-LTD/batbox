#include "gadgets/files/Notebook.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
namespace notebook=batbox::gadgets::files::notebook;
static Json evaluate(const Json& t){
  const auto op=t.at("op").get<std::string>();
  if(op=="map")return notebook::mapNotebookCellsToGadgetResult(t.at("cells"),"use-1");
  if(op=="parseCellId"){
    const auto id=notebook::parseCellId(t.at("value").get<std::string>());
    return id?Json(*id):Json();
  }
  notebook::Services s;
  s.paths.getCwd=[](){return std::string("/tmp");};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.formatOutput=[](const std::string& text){return batbox::value::substring(text,0,40);};
  const auto id=t.contains("cellId")?std::optional<std::string>(t.at("cellId").get<std::string>()):std::nullopt;
  return notebook::readNotebook(t.at("path").get<std::string>(),s,id);
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
