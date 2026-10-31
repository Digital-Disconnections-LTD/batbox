#include "gadgets/bash/Helpers.hpp"
#include "shell/ManagedShell.hpp"
#include <iostream>
using batbox::Json;
using namespace batbox::bash_gadget;
static Json evaluate(const Json& input){
 const auto op=input.at("op").get<std::string>();
 const auto content=input.value("content",std::string());
 const auto command=input.value("command",std::string());
 SplitCommand split=[&](const std::string&){
   if(input.contains("parts"))return input.at("parts").get<std::vector<std::string>>();
   return std::vector<std::string>{command};
 };
 Json result;
 if(op=="strip")result=stripEmptyLines(content);
 else if(op=="image")result=isImageOutput(content);
 else if(op=="uri"){auto x=parseDataUri(content);result=x?*x:Json(nullptr);}
 else if(op=="imageResult")result=buildImageGadgetResult(content,input.value("toolUseID",std::string("toolu_1")));
 else if(op=="format")result=formatOutput(content,input.value("maxOutputLength",30000));
 else if(op=="semantic")result=interpretCommandResult(command,input.at("code").get<int>(),input.value("stdout",std::string()),input.value("stderr",std::string()),split);
 else if(op=="blockedSleep"){auto x=detectBlockedSleepPattern(command,split);result=x?Json(*x):Json(nullptr);}
 else if(op=="searchRead")result=isSearchOrReadBashCommand(command,split);
 else if(op=="silent")result=isSilentBashCommand(command,split);
 else if(op=="autoBackground")result=batbox::shell::isAutobackgroundingAllowed(command,split);
 else throw std::logic_error("unknown op");
 return {{"result",result}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{std::cout<<batbox::value::stringify(evaluate(Json::parse(line)))<<'\n';}
 catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"fatal",e.what()}})<<'\n';}}
