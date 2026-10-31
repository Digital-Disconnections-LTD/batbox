#include "gadgets/synthetic_output/SyntheticOutputFactory.hpp"
#include "gadgets/synthetic_output/IndigenousJsonSchemaBackend.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace batbox;using namespace batbox::gadgets::synthetic_output;
 std::vector<std::string> warnings;
 SyntheticOutputFactory factory(makeSyntheticOutputGadget("StructuredOutput"),makeIndigenousJsonSchemaBackend([&](const std::string& m){warnings.push_back(m);}));
 auto schema=std::make_shared<const Json>(Json{{"type","object"},{"properties",Json{{"answer",Json{{"type","string"}}}}},{"required",Json::array({"answer"})},{"additionalProperties",false}});
 const auto result=factory.create(schema);assert(result&&result->gadget&&!result->error);
 assert(factory.create(schema)==result);
 const auto accepted=result->gadget->call(Json{{"answer","ready"}}, {}, {}, {}, {}).get();
 assert(accepted.structuredOutput&&accepted.structuredOutput->at("answer")=="ready");
 bool rejected=false;
 try{result->gadget->call(Json{{"answer",17}}, {}, {}, {}, {}).get();}
 catch(const OutputSchemaError&){rejected=true;}
 assert(rejected);
 std::cout<<"Indigenous Draft-7 schema, synthetic Gadget, and identity stash checks pass\n";
}
