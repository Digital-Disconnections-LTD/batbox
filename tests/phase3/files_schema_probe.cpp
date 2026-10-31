#include "gadgets/files/FileSchemas.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  batbox::Json result=batbox::Json::object();
  for(const char* name:{"Read","Write","Edit","Glob","Grep"})
    result[name]={{"input",batbox::gadgets::files::fileGadgetInputSchema(name)},
      {"output",batbox::gadgets::files::fileGadgetOutputSchema(name)}};
  std::cout<<batbox::value::stringify(result)<<'\n';
}
