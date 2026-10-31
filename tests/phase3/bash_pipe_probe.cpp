#include "shell/PipeCommand.hpp"
#include "shell/ShellQuoteChecks.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  const auto op=input.at("op").get<std::string>();
  const auto command=input.at("command").get<std::string>();
  Json result;
  if(op=="pipe")result=batbox::shell_command::rearrangePipeCommand(command);
  else if(op=="malformed")result=batbox::shell_command::hasMalformedTokens(command,input.at("parsed"));
  else if(op=="singleBug")result=batbox::shell_command::hasShellQuoteSingleQuoteBug(command);
  else throw std::logic_error("unknown op");
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){
  std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
}}
