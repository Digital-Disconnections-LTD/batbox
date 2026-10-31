#include "instructions/Frontmatter.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto op=input.at("op").get<std::string>();
    const auto& value=batbox::value::property(input,"value");
    batbox::Json result;
    if(op=="parse"){
      const auto parsed=batbox::instructions::parseFrontmatter(value.get<std::string>(),"fixture.md");
      result={{"frontmatter",parsed.frontmatter},{"content",parsed.content}};
    }else if(op=="paths")result=batbox::instructions::splitPathInFrontmatter(value);
    else if(op=="description"){
      const auto text=batbox::instructions::coerceDescriptionToString(value);
      if(text)result=*text;
    }else if(op=="boolean")result=batbox::instructions::parseBooleanFrontmatter(value);
    else if(op=="shell"){
      const auto text=batbox::instructions::parseShellFrontmatter(value,"fixture");
      if(text)result=*text;
    }else result=batbox::instructions::extractDescriptionFromMarkdown(
      value.get<std::string>(),input.value("fallback",std::string("Custom item")));
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
