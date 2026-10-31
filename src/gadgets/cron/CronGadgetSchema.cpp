#include "gadgets/cron/CronGadgets.hpp"
#include <stdexcept>

namespace batbox::gadgets::cron {
namespace {
[[noreturn]] void invalid(const std::string& field){
  throw std::invalid_argument("Invalid Cron tool input: "+field);
}
bool semanticBoolean(const Json& value){
  if(value=="true")return true;
  if(value=="false")return false;
  if(value.is_boolean())return value.get<bool>();
  invalid("boolean");
}
}
Json parseCronCreateInput(const Json& input){
  if(!input.is_object())invalid("object");
  for(auto it=input.begin();it!=input.end();++it)
    if(it.key()!="cron"&&it.key()!="prompt"&&
       it.key()!="recurring"&&it.key()!="durable")invalid(it.key());
  if(!input.contains("cron")||!input.at("cron").is_string())
    invalid("cron");
  if(!input.contains("prompt")||!input.at("prompt").is_string())
    invalid("prompt");
  Json output=Json::object();output["cron"]=input.at("cron");
  output["prompt"]=input.at("prompt");
  if(input.contains("recurring"))
    output["recurring"]=semanticBoolean(input.at("recurring"));
  if(input.contains("durable"))
    output["durable"]=semanticBoolean(input.at("durable"));
  return output;
}
Json parseCronDeleteInput(const Json& input){
  if(!input.is_object())invalid("object");
  for(auto it=input.begin();it!=input.end();++it)
    if(it.key()!="id")invalid(it.key());
  if(!input.contains("id")||!input.at("id").is_string())
    invalid("id");
  Json output=Json::object();output["id"]=input.at("id");
  return output;
}
Json parseCronListInput(const Json& input){
  if(!input.is_object()||!input.empty())invalid("object");
  return Json::object();
}
Json cronInputSchema(const std::string& name){
  if(name=="CronCreate")return value::parse(R"BATBOXSCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"cron":{"description":"Standard 5-field cron expression in local time: \"M H DoM Mon DoW\" (e.g. \"*/5 * * * *\" = every 5 minutes, \"30 14 28 2 *\" = Feb 28 at 2:30pm local once).","type":"string"},"prompt":{"description":"The prompt to enqueue at each fire time.","type":"string"},"recurring":{"description":"true (default) = fire on every cron match until deleted or auto-expired after 7 days. false = fire once at the next match, then auto-delete. Use false for \"remind me at X\" one-shot requests with pinned minute/hour/dom/month.","type":"boolean"},"durable":{"description":"true = persist to .batbox/scheduled_tasks.json and survive restarts. false (default) = in-memory only, dies when this Batbox session ends. Use true only when the user asks the task to survive across sessions.","type":"boolean"}},"required":["cron","prompt"],"additionalProperties":false})BATBOXSCHEMA");
  if(name=="CronDelete")return value::parse(R"BATBOXSCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"id":{"description":"Job ID returned by CronCreate.","type":"string"}},"required":["id"],"additionalProperties":false})BATBOXSCHEMA");
  if(name=="CronList")return value::parse(R"BATBOXSCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{},"additionalProperties":false})BATBOXSCHEMA");
  throw std::invalid_argument("Unknown Cron input schema: "+name);
}
Json cronOutputSchema(const std::string& name){
  if(name=="CronCreate")return value::parse(R"BATBOXSCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"id":{"type":"string"},"humanSchedule":{"type":"string"},"recurring":{"type":"boolean"},"durable":{"type":"boolean"}},"required":["id","humanSchedule","recurring"],"additionalProperties":false})BATBOXSCHEMA");
  if(name=="CronDelete")return value::parse(R"BATBOXSCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"id":{"type":"string"}},"required":["id"],"additionalProperties":false})BATBOXSCHEMA");
  if(name=="CronList")return value::parse(R"BATBOXSCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"jobs":{"type":"array","items":{"type":"object","properties":{"id":{"type":"string"},"cron":{"type":"string"},"humanSchedule":{"type":"string"},"prompt":{"type":"string"},"recurring":{"type":"boolean"},"durable":{"type":"boolean"}},"required":["id","cron","humanSchedule","prompt"],"additionalProperties":false}}},"required":["jobs"],"additionalProperties":false})BATBOXSCHEMA");
  throw std::invalid_argument("Unknown Cron output schema: "+name);
}
}
