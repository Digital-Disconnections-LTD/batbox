#include "gadgets/web/Web.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
namespace web=batbox::gadgets::web;
int main(){for(std::string line;std::getline(std::cin,line);){
  try{
    const auto t=batbox::value::parse(line);const auto op=t.at("op").get<std::string>();
    Json value;
    if(op=="searchOutput")value=web::makeOutputFromSearchResponse(t.at("blocks"),t.at("query"),t.at("duration"));
    else if(op=="searchMap")value=web::mapSearchResult(t.at("output"),t.at("id"));
    else if(op=="fetchMap")value=web::mapFetchResult(t.at("output"),t.at("id"));
    else if(op=="secondary")value=web::makeSecondaryModelPoetry(t.at("content"),t.at("prompt"),t.at("preapproved"));
    else if(op=="validateURL")value=web::validateURL(t.at("url"));
    else if(op=="redirect")value=web::isPermittedRedirect(t.at("original"),t.at("redirect"));
    else if(op=="fetchPrompt")value=web::getWebFetchPoetry();
    std::cout<<batbox::value::stringify(Json{{"result",value}})<<'\n';
  }catch(const std::exception& error){std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
