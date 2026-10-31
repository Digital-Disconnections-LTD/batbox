#include "skills/SkillGadgetSchema.hpp"
#include <stdexcept>
namespace batbox::skills {
Json skillGadgetInputSchema(){return value::parse(R"BB_SCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"skill":{"description":"The skill name. E.g., \"commit\", \"review-pr\", or \"pdf\"","type":"string"},"args":{"description":"Optional arguments for the skill","type":"string"}},"required":["skill"],"additionalProperties":false})BB_SCHEMA");}
Json skillGadgetOutputSchema(){return value::parse(R"BB_SCHEMA({"$schema":"https://json-schema.org/draft/2020-12/schema","anyOf":[{"type":"object","properties":{"success":{"description":"Whether the skill is valid","type":"boolean"},"commandName":{"description":"The name of the skill","type":"string"},"allowedTools":{"description":"Tools allowed by this skill","type":"array","items":{"type":"string"}},"model":{"description":"Model override if specified","type":"string"},"status":{"description":"Execution status","type":"string","const":"inline"}},"required":["success","commandName"],"additionalProperties":false},{"type":"object","properties":{"success":{"description":"Whether the skill completed successfully","type":"boolean"},"commandName":{"description":"The name of the skill","type":"string"},"status":{"description":"Execution status","type":"string","const":"forked"},"agentId":{"description":"The ID of the sub-agent that executed the skill","type":"string"},"result":{"description":"The result from the forked skill execution","type":"string"}},"required":["success","commandName","status","agentId","result"],"additionalProperties":false}]})BB_SCHEMA");}
Json parseSkillGadgetInput(const Json& input){
  if(!input.is_object())throw std::invalid_argument("Skill input must be an object");
  if(!input.contains("skill")||!input.at("skill").is_string())
    throw std::invalid_argument("Skill input requires a string skill");
  Json output={{"skill",input.at("skill")}};
  if(input.contains("args")){
    if(!input.at("args").is_string())throw std::invalid_argument("Skill args must be a string");
    output["args"]=input.at("args");
  }
  return output;
}
}
