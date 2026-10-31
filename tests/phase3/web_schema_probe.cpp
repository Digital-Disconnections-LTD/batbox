#include "gadgets/web/Web.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  const batbox::Json result={{"WebSearch",{{"input",batbox::gadgets::web::webSearchInputSchema()},
    {"output",batbox::gadgets::web::webSearchOutputSchema()}}},
    {"WebFetch",{{"input",batbox::gadgets::web::webFetchInputSchema()},
    {"output",batbox::gadgets::web::webFetchOutputSchema()}}}};
  std::cout<<batbox::value::stringify(result)<<'\n';
}
