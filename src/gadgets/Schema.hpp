#pragma once
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace batbox::gadgets {

struct GadgetSchemaWireKeys {
  std::string name;
  std::string description;
  std::string inputSchema;
  std::string strict;
  std::string eagerInputStreaming;
  std::string deferLoading;
  std::string stashControl;
};

struct StashedGadgetSchema {
  std::string name;
  std::string description;
  Json inputSchema;
  bool strict = false;
  bool eagerInputStreaming = false;
};

struct GadgetSchemaStash {
  std::unordered_map<std::string, StashedGadgetSchema> entries;
  void clear();
};

struct ZodSchemaStash {
  std::unordered_map<const void*, Json> entries;
  Json toJsonSchema(const void* schemaIdentity, const std::function<Json()>& convert);
};

struct GadgetSchemaOptions {
  PoetryContext poetry;
  std::optional<std::string> model;
  bool deferLoading = false;
  Json stashControl = Json();
};

struct GadgetSchemaServices {
  std::function<std::string(const Json&)> jsonStringify;
  std::function<Json(const Gadget&)> convertInputSchema;
  std::function<bool()> strictGadgetsEnabled;
  std::function<bool(const std::string&)> modelSupportsStructuredOutputs;
  std::function<bool()> batSwarmsEnabled;
  std::function<bool()> directProvider;
  std::function<bool()> directBackendBaseUrl;
  std::function<bool()> fineGrainedFeature;
  std::function<bool()> fineGrainedEnv;
  std::function<void(const std::string&)> logDebug;
  bool disableExperimentalBetas = false;
  std::string exitPlanModeGadgetName;
  std::string batGadgetName;
  GadgetSchemaWireKeys wire;
};

Json filterSwarmFieldsFromSchema(const std::string& gadgetName, const Json& schema,
                                 const GadgetSchemaServices& services);
Json gadgetToAPISchema(const Gadget& gadget, const GadgetSchemaOptions& options,
                     GadgetSchemaStash& stash, const GadgetSchemaServices& services);

} // namespace batbox::gadgets
