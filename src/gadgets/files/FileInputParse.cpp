#include "gadgets/files/FileSchemas.hpp"
#include <cmath>
#include <regex>
#include <stdexcept>

namespace batbox::gadgets::files {
namespace {
[[noreturn]] void invalid(const std::string& name,const std::string& key,const std::string& reason){
  throw std::invalid_argument("Invalid "+name+" input at "+key+": "+reason);
}
bool has(const Json& input,const char* key){return input.contains(key);}
void stringField(const std::string& name,const Json& input,const char* key,bool required){
  if(!has(input,key)){if(required)invalid(name,key,"required string");return;}
  if(!input.at(key).is_string())invalid(name,key,"expected string");
}
void boolField(const std::string& name,Json& input,const char* key){
  if(!has(input,key))return;
  auto& value=input[key];
  if(value.is_string()){
    if(value=="true")value=true;
    else if(value=="false")value=false;
  }
  if(!value.is_boolean())invalid(name,key,"expected boolean");
}
void numberField(const std::string& name,Json& input,const char* key,
                 bool integer=false,bool nonnegative=false,bool positive=false){
  if(!has(input,key))return;
  auto& value=input[key];
  if(value.is_string()){
    const auto text=value.get<std::string>();
    static const std::regex decimal(R"(^-?[0-9]+(\.[0-9]+)?$)");
    if(std::regex_match(text,decimal)){
      const auto parsed=value::parseFloat(text);
      if(std::isfinite(parsed))value=parsed;
    }
  }
  if(!value.is_number())invalid(name,key,"expected number");
  const double number=value.get<double>();
  if(!std::isfinite(number))invalid(name,key,"expected finite number");
  if(integer&&std::floor(number)!=number)invalid(name,key,"expected integer");
  if(nonnegative&&number<0)invalid(name,key,"must be nonnegative");
  if(positive&&number<=0)invalid(name,key,"must be positive");
}
void strict(const std::string& name,const Json& input,const Json& schema){
  if(!input.is_object())invalid(name,"$","expected object");
  const auto& properties=schema.at("properties");
  for(auto it=input.begin();it!=input.end();++it)
    if(!properties.contains(it.key()))invalid(name,it.key(),"unrecognized key");
}
} // namespace

Json parseFileGadgetInput(const std::string& name,const Json& input){
  const auto schema=fileGadgetInputSchema(name);
  strict(name,input,schema);
  Json value=input;
  if(name=="Read"){
    stringField(name,value,"file_path",true);
    numberField(name,value,"offset",true,true,false);
    numberField(name,value,"limit",true,false,true);
    stringField(name,value,"pages",false);
  }else if(name=="Write"){
    stringField(name,value,"file_path",true);stringField(name,value,"content",true);
  }else if(name=="Edit"){
    stringField(name,value,"file_path",true);stringField(name,value,"old_string",true);
    stringField(name,value,"new_string",true);boolField(name,value,"replace_all");
  }else if(name=="Glob"){
    stringField(name,value,"pattern",true);stringField(name,value,"path",false);
  }else if(name=="Grep"){
    stringField(name,value,"pattern",true);
    for(const char* key:{"path","glob","output_mode","type"})stringField(name,value,key,false);
    if(value.contains("output_mode")&&value.at("output_mode")!="content"&&
       value.at("output_mode")!="files_with_matches"&&value.at("output_mode")!="count")
      invalid(name,"output_mode","unrecognized mode");
    for(const char* key:{"-B","-A","-C","context","head_limit","offset"})
      numberField(name,value,key);
    for(const char* key:{"-n","-i","multiline"})boolField(name,value,key);
  }else throw std::invalid_argument("Unknown file tool schema: "+name);
  return value;
}
} // namespace batbox::gadgets::files
