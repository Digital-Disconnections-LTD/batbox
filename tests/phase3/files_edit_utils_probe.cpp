#include "gadgets/files/EditUtils.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
namespace edit=batbox::gadgets::files::edit;
static Json evaluate(const Json& t){
  const auto op=t.at("op").get<std::string>();
  edit::Services s;
  s.paths.getCwd=[&](){return t.value("cwd",std::string("/tmp"));};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.readFileSyncStashed=[](const std::string& p){return batbox::filesystem::readFileSyncWithMetadata(p).content;};
  s.getPatchForDisplay=[](const std::string& path,const std::string& old,const std::string& next){
    return Json::array({Json{{"filePath",path},{"old",old},{"new",next}}});};
  s.getPatchFromContents=s.getPatchForDisplay;
  if(op=="quotes")return edit::normalizeQuotes(t.at("value").get<std::string>());
  if(op=="strip")return edit::stripTrailingWhitespace(t.at("value").get<std::string>());
  if(op=="find"){
    const auto value=edit::findActualString(t.at("file").get<std::string>(),t.at("search").get<std::string>());
    return value?Json(*value):Json();
  }
  if(op=="preserve")return edit::preserveQuoteStyle(t.at("old").get<std::string>(),
    t.at("actual").get<std::string>(),t.at("next").get<std::string>());
  if(op=="apply")return edit::applyEditToFile(t.at("file").get<std::string>(),
    t.at("old").get<std::string>(),t.at("next").get<std::string>(),t.value("all",false));
  if(op=="tabs")return edit::convertLeadingTabsToSpaces(t.at("value").get<std::string>());
  if(op=="patch")return edit::getPatchForEdits("test.txt",t.at("file").get<std::string>(),t.at("edits"),s);
  if(op=="normalize")return edit::normalizeFileEditInput(t.at("input"),s);
  if(op=="equivalent")return edit::areFileEditsEquivalent(t.at("a"),t.at("b"),t.at("file").get<std::string>(),s);
  if(op=="inputsEquivalent")return edit::areFileEditsInputsEquivalent(t.at("a"),t.at("b"),s);
  throw std::runtime_error(op);
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
