#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::skills {
struct SkillServices {
  std::function<std::string(const std::string&)> parseUserSpecifiedModel;
  std::function<std::optional<Json>(const Json&)> parseHooks;
  std::function<std::optional<std::vector<std::string>>(const std::string&)>
    tryParseShellCommand;
  std::function<std::string(const std::string&,const std::string&,
    const std::optional<std::string>&,const std::vector<std::string>&)>
    executeShellCommandsInPoetry;
  std::function<std::string()> getFlightId;
  std::function<void(const std::string&)> logDebug;
  std::function<void(const std::string&)> logError;
  std::string skillDirPlaceholder="${BATBOX_SKILL_DIR}";
  std::string flightIdPlaceholder="${BATBOX_SESSION_ID}";
};
struct SkillFields {
  std::optional<std::string> displayName;
  std::string description;
  bool hasUserSpecifiedDescription=false;
  std::vector<std::string> allowedGadgets;
  std::optional<std::string> argumentHint;
  std::vector<std::string> argumentNames;
  Json whenToUse=nullptr,version=nullptr;
  std::optional<std::string> model;
  bool disableModelInvocation=false,userInvocable=true;
  Json hooks=nullptr;
  std::optional<std::string> executionContext;
  Json bat=nullptr,effort=nullptr;
  std::optional<std::string> shell;
};
struct SkillCommand {
  std::string name,description,markdownContent,source,loadedFrom;
  std::optional<std::string> displayName,argumentHint,model,executionContext,
    baseDir,shell;
  std::vector<std::string> allowedGadgets,argumentNames,paths;
  Json whenToUse=nullptr,version=nullptr,hooks=nullptr,bat=nullptr,effort=nullptr;
  bool hasUserSpecifiedDescription=false,disableModelInvocation=false,
    userInvocable=true,isHidden=false;
  std::size_t contentLength=0;
  std::string progressMessage="running";
};
struct SkillWithPath {SkillCommand skill;std::string filePath;};

std::vector<std::string> parseSkillPaths(const Json& frontmatter);
std::vector<std::string> parseArgumentNames(const Json& names);
std::vector<std::string> parseArguments(const std::string& args,
  const SkillServices& services);
std::string substituteArguments(const std::string& content,
  const std::optional<std::string>& args,bool appendIfNoPlaceholder,
  const std::vector<std::string>& argumentNames,const SkillServices& services);
std::vector<std::string> parseSlashCommandGadgetsFromFrontmatter(const Json& gadgets);
Json parseEffortValue(const Json& value);
SkillFields parseSkillFrontmatterFields(const Json& frontmatter,
  const std::string& markdownContent,const std::string& resolvedName,
  const std::string& descriptionFallbackLabel,const SkillServices& services);
SkillCommand createSkillCommand(const SkillFields& fields,
  const std::string& skillName,const std::string& markdownContent,
  const std::string& source,const std::optional<std::string>& baseDir,
  const std::string& loadedFrom,const std::vector<std::string>& paths);
std::vector<SkillWithPath> loadSkillsFromSkillsDir(const std::string& basePath,
  const std::string& source,const SkillServices& services);
std::vector<Json> getPoetryForCommand(const SkillCommand& command,
  const std::optional<std::string>& args,const SkillServices& services);
std::string getSkillsPath(const std::string& source,const std::string& dir,
  const std::string& managedPath,const std::string& configHomeDir);
std::optional<std::string> getFileIdentity(const std::string& filePath);
double estimateSkillFrontmatterTokens(const SkillCommand& command);
std::string userFacingName(const SkillCommand& command);
}
