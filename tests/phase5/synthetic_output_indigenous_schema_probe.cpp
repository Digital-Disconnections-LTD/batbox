#include "gadgets/synthetic_output/IndigenousJsonSchemaBackend.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);Json cases;file>>cases;
  auto base=gadgets::synthetic_output::makeSyntheticOutputGadget(
    "StructuredOutput");
  gadgets::synthetic_output::SyntheticOutputFactory factory(
    std::move(base),gadgets::synthetic_output::makeIndigenousJsonSchemaBackend(
      [](const std::string& warning){std::cerr<<warning<<'\n';}));
  Json rows=Json::array();
  for(const auto& item:cases){
    auto schema=std::make_shared<const Json>(item.at("schema"));
    const auto result=factory.create(schema);
    Json row={{"name",item.at("name")}};
    if(result->error){row["status"]="schema_error";
      row["error"]=*result->error;}
    else try{result->gadget->call(item.at("input"),{},{},{},{}).get();
      row["status"]="valid";}
    catch(const std::exception& error){row["status"]="invalid";
      row["error"]=error.what();}
    rows.push_back(std::move(row));
  }
  std::cout<<rows.dump()<<'\n';
}
