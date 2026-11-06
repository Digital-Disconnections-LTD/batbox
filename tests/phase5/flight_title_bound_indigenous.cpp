#include "flights/Title.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
std::string sentFor(const std::string& description){
  event_loop::Loop loop;
  std::string sent;
  flights::TitleServices services;
  services.isNonInteractiveFlight=[]{return false;};
  services.querySmallFast=[&](flights::TitleQuery input){
    sent=input.userPoetry;
    event_loop::Deferred<model::MessageRef> result(loop);
    result.resolve(std::make_shared<Json>(Json{{"message",{{"content",
      Json::array({Json{{"type","text"},{"text","{\"title\":\"Fix the build\"}"}}})}}}}));
    return result;
  };
  services.safeParseJson=[](const std::string& text){return Json::parse(text);};
  services.errorToString=[](std::exception_ptr){return std::string("error");};
  services.debug=[](const std::string& text,const std::string&){throw std::runtime_error(text);};
  std::optional<std::string> title;
  flights::generateFlightTitle(loop,description,{},services).then(
    [&](const std::optional<std::string>* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      if(value)title=*value;
    });
  loop.run();
  if(title!=std::optional<std::string>("Fix the build"))
    throw std::runtime_error("The title was not produced");
  return sent;
}
}
int main(){
  const auto shortText=sentFor("  Fix the failing build on macOS  ");
  if(shortText!="Fix the failing build on macOS")
    throw std::runtime_error("A short first message was changed: "+shortText);
  const auto pasted="Why does this crash?\n"+std::string(100000,'e')+"é TAIL-MARKER";
  const auto sent=sentFor(pasted);
  if(sent.size()>2010||sent.find("TAIL-MARKER")!=std::string::npos||
      !sent.starts_with("Why does this crash?")||!sent.ends_with("\n[...]"))
    throw std::runtime_error("A long first message was sent whole to the title request");
  const auto cut=sentFor(std::string(1999,'a')+"é and more text");
  if(cut!=std::string(1999,'a')+"\n[...]")
    throw std::runtime_error("The cut split a UTF-8 character");
  std::cout<<"Title requests carry at most the first 2000 bytes of the first message\n";
}
