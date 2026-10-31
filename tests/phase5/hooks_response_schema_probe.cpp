#include "instructions/hooks/runtime/HookResponseSchema.hpp"
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);batbox::Json cases;file>>cases;
  batbox::Json rows=batbox::Json::array();
  for(const auto& item:cases){
    const auto parsed=batbox::instructions::hooks::runtime::
      hookResponseSchemaSafeParse(item.at("value"));
    batbox::Json row={{"name",item.at("name")},{"success",parsed.success}};
    if(parsed.success)row["data"]=parsed.data;
    else row["errorMessage"]=parsed.errorMessage;
    rows.push_back(std::move(row));
  }
  std::cout<<rows.dump()<<'\n';
}
