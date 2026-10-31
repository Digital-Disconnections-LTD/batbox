#include "shell/CommandQuoting.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  const auto op=input.at("op").get<std::string>();
  const auto command=input.at("command").get<std::string>();
  Json result;
  if(op=="containsHeredoc")result=batbox::shell_command::containsHeredocForQuoting(command);
  else if(op=="containsMultiline")result=batbox::shell_command::containsMultilineString(command);
  else if(op=="hasStdin")result=batbox::shell_command::hasStdinRedirect(command);
  else if(op=="shouldAddStdin")result=batbox::shell_command::shouldAddStdinRedirect(command);
  else if(op=="quote")result=batbox::shell_command::quoteShellCommand(command,
    input.value("addStdinRedirect",true));
  else if(op=="rewriteNul")result=batbox::shell_command::rewriteWindowsNullRedirect(command);
  else if(op=="prefix")result=batbox::shell_command::formatShellPrefixCommand(
    input.at("prefix").get<std::string>(),command);
  else throw std::logic_error("unknown op");
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){
  std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
}}
