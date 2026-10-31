#include "gadgets/files/FileRead.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
using batbox::Json;
namespace files=batbox::gadgets::files;
static Json evaluate(const Json& t){
  const std::string op=t.at("op").get<std::string>();
  if(op=="pages"){
    const auto p=files::parsePDFPageRange(t.at("value").get<std::string>());
    if(!p)return nullptr;
    return Json{{"firstPage",p->firstPage},{"lastPage",std::isinf(p->lastPage)?Json():Json(p->lastPage)}};
  }
  if(op=="pdfExt")return files::isPDFExtension(t.at("value").get<std::string>());
  if(op=="binary")return files::hasBinaryExtension(t.at("value").get<std::string>());
  if(op=="blocked")return files::isBlockedDevicePath(t.at("value").get<std::string>());
  if(op=="alternate"){
    const auto path=files::getAlternateScreenshotPath(t.at("value").get<std::string>());
    return path ? Json(*path) : Json();
  }
  if(op=="numbered")return files::addLineNumbers(t.at("value").get<std::string>(),
    t.at("startLine").get<std::size_t>(),t.value("compact",false));
  files::FileReadServices s;
  s.paths.getCwd=[&](){return t.value("cwd",std::string("/tmp"));};
  s.paths.homeDirectory=[](){return std::string("/home/batbox");};
  s.paths.platform=[](){return std::string("linux");};
  s.paths.normalizeNFC=[](const std::string& p){return p;};
  s.matchingReadDenyRule=[&](const std::string&){return t.value("deny",false);};
  const auto v=files::validateReadInput(t.at("input"),s);
  Json result={{"result",v.result}};
  if(v.message)result["message"]=*v.message;
  if(v.errorCode)result["errorCode"]=*v.errorCode;
  return result;
}
int main(){std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  try{std::cout<<batbox::value::stringify(Json{{"result",evaluate(batbox::value::parse(input))}})<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}}
