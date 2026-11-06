#include "bats/BatGadgetSchema.hpp"
#include <cassert>
#include <iostream>

int main(){
  using namespace batbox;
  bats::BatGadgetSchemaOptions options;
  gadgets::Gadget gadget;
  bats::configureBatGadgetInputSchema(gadget,options);
  assert(gadget.inputJSONSchema.at("properties").at("model").at("enum")==
    Json::array({"standard","premium","fast"}));
  assert(gadget.inputJSONSchema.at("required")==Json::array({"description","prompt"}));
  Json input={{"description","Explore code"},{"prompt","Inspect files"},
    {"model","fast"},{"run_in_background",true},{"ignored",1}};
  const auto parsed=gadget.parseInput(input);
  assert(parsed.size()==4 && parsed.at("model")=="fast" &&
    !parsed.contains("ignored"));
  options.backgroundMissionsDisabled=true;
  bats::configureBatGadgetInputSchema(gadget,options);
  const auto stripped=gadget.parseInput(input);
  assert(!stripped.contains("run_in_background"));
  assert(!gadget.inputJSONSchema.at("properties").contains("run_in_background"));
  bool rejected=false;
  try{gadget.parseInput(Json{{"description","Explore"},{"prompt","Inspect"},
    {"model","sonnet"}});}catch(const std::invalid_argument&){rejected=true;}
  assert(rejected);
  bats::configureBatGadgetInputSchema(gadget,bats::BatGadgetSchemaOptions{});
  assert(!gadget.inputJSONSchema.at("properties").contains("cwd"));
  assert(!gadget.parseInput(Json{{"description","Explore"},{"prompt","Inspect"},
    {"cwd","/repo"}}).contains("cwd"));
  rejected=false;
  try{gadget.parseInput(Json{{"description","Explore"},{"prompt","Inspect"},
    {"mode","auto"}});}catch(const std::invalid_argument&){rejected=true;}
  assert(rejected);
  std::cout<<"BatGadget branded input schema, validation and gating passed\n";
}
