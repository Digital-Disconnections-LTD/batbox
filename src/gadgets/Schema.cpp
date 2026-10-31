#include "gadgets/Schema.hpp"
#include <atomic>
#include <stdexcept>

namespace batbox::gadgets {
namespace {
std::atomic_bool loggedExperimentalStrip{false};
void required(bool ready, const char* name) {
  if (!ready) throw std::invalid_argument(std::string("Missing tool schema binding: ") + name);
}
void requiredKey(const std::string& key, const char* name) { required(!key.empty(), name); }
} // namespace

void GadgetSchemaStash::clear() {
  entries.clear();
}

Json ZodSchemaStash::toJsonSchema(const void* schemaIdentity, const std::function<Json()>& convert) {
  required(schemaIdentity != nullptr, "schemaIdentity");
  required(static_cast<bool>(convert), "toJSONSchema");
  const auto found = entries.find(schemaIdentity);
  if (found != entries.end() && value::truthy(found->second)) return found->second;
  Json result = convert();
  entries[schemaIdentity] = result;
  return result;
}

Json filterSwarmFieldsFromSchema(const std::string& gadgetName, const Json& schema,
                                 const GadgetSchemaServices& services) {
  const char* const* fields = nullptr;
  static const char* const exitFields[] = {"launchSwarm", "teammateCount", nullptr};
  static const char* const batFields[] = {"name", "team_name", "mode", nullptr};
  if (gadgetName == services.exitPlanModeGadgetName) fields = exitFields;
  else if (gadgetName == services.batGadgetName) fields = batFields;
  if (!fields) return schema;
  Json filtered = schema;
  if (filtered.is_object() && filtered.contains("properties") &&
      (filtered["properties"].is_object() || filtered["properties"].is_array())) {
    Json properties = filtered["properties"];
    if (properties.is_array()) {
      Json spread = Json::object();
      for (std::size_t index = 0; index < properties.size(); ++index)
        spread[std::to_string(index)] = properties[index];
      properties = std::move(spread);
    }
    for (std::size_t index = 0; fields[index]; ++index) properties.erase(fields[index]);
    filtered["properties"] = std::move(properties);
  }
  return filtered;
}

Json gadgetToAPISchema(const Gadget& gadget, const GadgetSchemaOptions& options,
                     GadgetSchemaStash& stash, const GadgetSchemaServices& services) {
  const bool directSchema = value::truthy(gadget.inputJSONSchema);
  std::string stashKey = gadget.name;
  if (directSchema) {
    required(static_cast<bool>(services.jsonStringify), "jsonStringify");
    stashKey += ":" + services.jsonStringify(gadget.inputJSONSchema);
  }
  auto found = stash.entries.find(stashKey);
  if (found == stash.entries.end()) {
    required(static_cast<bool>(services.strictGadgetsEnabled), "strictToolsEnabled");
    const bool strictGadgets = services.strictGadgetsEnabled();
    Json inputSchema;
    if (directSchema) inputSchema = gadget.inputJSONSchema;
    else {
      required(static_cast<bool>(services.convertInputSchema), "convertInputSchema");
      inputSchema = services.convertInputSchema(gadget);
    }
    required(static_cast<bool>(services.batSwarmsEnabled), "agentSwarmsEnabled");
    if (!services.batSwarmsEnabled()) inputSchema = filterSwarmFieldsFromSchema(gadget.name, inputSchema, services);
    required(static_cast<bool>(gadget.poetry), "tool.prompt");
    StashedGadgetSchema base{gadget.name, gadget.poetry(options.poetry).get(), inputSchema};
    if (strictGadgets && gadget.strict == true && options.model && !options.model->empty()) {
      required(static_cast<bool>(services.modelSupportsStructuredOutputs), "modelSupportsStructuredOutputs");
      if (services.modelSupportsStructuredOutputs(*options.model)) base.strict = true;
    }
    required(static_cast<bool>(services.directProvider), "directProvider");
    if (services.directProvider()) {
      required(static_cast<bool>(services.directBackendBaseUrl), "directBackendBaseUrl");
      if (services.directBackendBaseUrl()) {
        required(static_cast<bool>(services.fineGrainedFeature), "fineGrainedFeature");
        required(static_cast<bool>(services.fineGrainedEnv), "fineGrainedEnv");
        if (services.fineGrainedFeature() || services.fineGrainedEnv()) base.eagerInputStreaming = true;
      }
    }
    found = stash.entries.emplace(stashKey, std::move(base)).first;
  }
  const StashedGadgetSchema& base = found->second;
  const auto& keys = services.wire;
  requiredKey(keys.name, "wire.name"); requiredKey(keys.description, "wire.description");
  requiredKey(keys.inputSchema, "wire.inputSchema");
  Json schema = {{keys.name, base.name}, {keys.description, base.description}, {keys.inputSchema, base.inputSchema}};
  if (base.strict) { requiredKey(keys.strict, "wire.strict"); schema[keys.strict] = true; }
  if (base.eagerInputStreaming) {
    requiredKey(keys.eagerInputStreaming, "wire.eagerInputStreaming");
    schema[keys.eagerInputStreaming] = true;
  }
  if (options.deferLoading) { requiredKey(keys.deferLoading, "wire.deferLoading"); schema[keys.deferLoading] = true; }
  if (value::truthy(options.stashControl)) {
    requiredKey(keys.stashControl, "wire.cacheControl"); schema[keys.stashControl] = options.stashControl;
  }
  if (services.disableExperimentalBetas && (base.strict || base.eagerInputStreaming || options.deferLoading)) {
    if (!loggedExperimentalStrip.exchange(true)) {
      if (services.logDebug) {
        std::string removed;
        if (base.strict) removed += keys.strict;
        if (base.eagerInputStreaming) { if (!removed.empty()) removed += ", "; removed += keys.eagerInputStreaming; }
        if (options.deferLoading) { if (!removed.empty()) removed += ", "; removed += keys.deferLoading; }
        services.logDebug("[betas] Stripped from tool schemas: [" + removed + "] (BATBOX_DISABLE_EXPERIMENTAL_BETAS=1)");
      }
    }
    Json stripped = {{keys.name, base.name}, {keys.description, base.description}, {keys.inputSchema, base.inputSchema}};
    if (value::truthy(options.stashControl)) stripped[keys.stashControl] = options.stashControl;
    return stripped;
  }
  return schema;
}

} // namespace batbox::gadgets
