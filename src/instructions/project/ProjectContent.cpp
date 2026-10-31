#include "instructions/project/ProjectMemory.hpp"
#include "instructions/project/MarkdownLexer.hpp"
#include "instructions/MemoryPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <unordered_set>

namespace batbox::instructions::project {
namespace {
const std::unordered_set<std::string> textFileExtensions={
  ".md",".txt",".text",".json",".yaml",".yml",".toml",".xml",".csv",".html",".htm",".css",
  ".scss",".sass",".less",".js",".ts",".tsx",".jsx",".mjs",".cjs",".mts",".cts",".py",
  ".pyi",".pyw",".rb",".erb",".rake",".go",".rs",".java",".kt",".kts",".scala",".c",
  ".cpp",".cc",".cxx",".h",".hpp",".hxx",".cs",".swift",".sh",".bash",".zsh",".fish",
  ".ps1",".bat",".cmd",".env",".ini",".cfg",".conf",".config",".properties",".sql",".graphql",".gql",
  ".proto",".vue",".svelte",".astro",".ejs",".hbs",".pug",".jade",".php",".pl",".pm",".lua",
  ".r",".R",".dart",".ex",".exs",".erl",".hrl",".clj",".cljs",".cljc",".edn",".hs",
  ".lhs",".elm",".ml",".mli",".f",".f90",".f95",".for",".cmake",".make",".makefile",".gradle",
  ".sbt",".rst",".adoc",".asciidoc",".org",".tex",".latex",".lock",".log",".diff",".patch"
};
std::string extension(const std::string& filePath){
  auto ext=std::filesystem::path(filePath).extension().string();
  for(auto& ch:ext)if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch+('a'-'A'));
  return ext;
}
}
ParsedMemoryContent parseMemoryFileContent(const std::string& rawContent,
  const std::string& filePath,const std::string& type,
  const std::optional<std::string>& includeBasePath,
  const ParseServices& services){
  const auto ext=extension(filePath);
  if(!ext.empty()&&!textFileExtensions.contains(ext)){
    if(services.logDebug)services.logDebug(
      "Skipping non-text file in @include: "+filePath);
    return {std::nullopt,{}};
  }
  const auto frontmatter=parseFrontmatterPaths(rawContent);
  const bool hasComment=frontmatter.content.find("<!--")!=std::string::npos;
  const Json tokens=hasComment||includeBasePath
    ?lexMarkdown(frontmatter.content):Json(nullptr);
  const auto stripped=hasComment&&!tokens.is_null()
    ?stripHtmlCommentsFromTokens(tokens).content:frontmatter.content;
  const auto includePaths=includeBasePath&&!tokens.is_null()
    ?extractIncludePathsFromTokens(tokens,*includeBasePath,services)
    :std::vector<std::string>{};
  auto finalContent=stripped;
  if(type=="AutoMem"||type=="TeamMem")
    finalContent=instructions::truncateEntrypointContent(stripped).content;
  const bool differs=finalContent!=rawContent;
  MemoryFileInfo info;
  info.path=filePath;info.type=type;info.content=finalContent;
  info.globs=frontmatter.paths;
  info.contentDiffersFromDisk=differs;
  if(differs)info.rawContent=rawContent;
  return {std::move(info),includePaths};
}
Json memoryFileToJson(const MemoryFileInfo& info){
  Json result={{"path",info.path},{"type",info.type},{"content",info.content}};
  if(info.parent)result["parent"]=*info.parent;
  if(info.globs){
    result["globs"]=Json::array();
    for(const auto& pattern:*info.globs)result["globs"].push_back(pattern);
  }
  result["contentDiffersFromDisk"]=info.contentDiffersFromDisk;
  if(info.rawContent)result["rawContent"]=*info.rawContent;
  return result;
}
}
