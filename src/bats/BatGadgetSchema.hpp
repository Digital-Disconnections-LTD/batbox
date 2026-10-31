#pragma once
#include "runtime/JsonValue.hpp"
#include "gadgets/Gadget.hpp"

namespace batbox::bats {
struct BatGadgetSchemaOptions {
  bool kairos=false;
  bool backgroundMissionsDisabled=false;
  bool forkSubagentEnabled=false;
  bool transcriptClassifier=false;
};
// Input schema with Batbox model selectors. Optional and description metadata
// is retained alongside the JSON Schema sent to the model.
Json batGadgetInputShape(const BatGadgetSchemaOptions& options);
Json batGadgetInputJSONSchema(const BatGadgetSchemaOptions& options);
Json inspectBatGadgetInput(const Json& input,const BatGadgetSchemaOptions& options);
Json parseBatGadgetInput(const Json& input,const BatGadgetSchemaOptions& options);
Json batGadgetOutputShape();
Json batGadgetOutputJSONSchema();
void configureBatGadgetInputSchema(gadgets::Gadget& gadget,
  const BatGadgetSchemaOptions& options);
} // namespace batbox::bats
