#include "shell/CommandSplitter.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  const auto command=input.at("command").get<std::string>();
  const auto salt=[](){return std::string("0123456789abcdef");};
  Json result;
  if(input.at("op")=="operators")result=batbox::shell_split::splitCommandWithOperators(command,salt);
  else if(input.at("op")=="deprecated")result=batbox::shell_split::splitCommandDeprecated(command,salt);
  else if(input.at("op")=="staticTarget")result=batbox::shell_split::isStaticRedirectTarget(command);
  else throw std::logic_error("unknown op");
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){
  std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
}}
