#include "interactive/markdown/Highlight.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace batbox::interactive::markdown {
namespace {
enum class Family {None,CLike,Script,Python,Rust,Go,Shell,Json,Yaml,Sql,Diff};

struct Language {
  Family family=Family::None;
  std::unordered_set<std::string> keywords;
  bool lineSlash=false,lineHash=false,lineDash=false,block=false;
  bool backtick=false,singleQuote=true,typeCase=true,caseInsensitive=false;
};

std::string lower(std::string_view value){
  std::string out(value);
  std::transform(out.begin(),out.end(),out.begin(),[](unsigned char ch){
    return static_cast<char>(std::tolower(ch));});
  return out;
}
std::unordered_set<std::string> words(std::initializer_list<const char*> list){
  return {list.begin(),list.end()};
}

Family family(std::string_view name){
  static const std::unordered_map<std::string,Family> aliases={
    {"c",Family::CLike},{"h",Family::CLike},{"cpp",Family::CLike},{"c++",Family::CLike},
    {"cc",Family::CLike},{"cxx",Family::CLike},{"hpp",Family::CLike},{"hxx",Family::CLike},
    {"objc",Family::CLike},{"java",Family::CLike},{"kotlin",Family::CLike},
    {"kt",Family::CLike},{"swift",Family::CLike},{"cs",Family::CLike},
    {"csharp",Family::CLike},{"c#",Family::CLike},
    {"js",Family::Script},{"javascript",Family::Script},{"jsx",Family::Script},
    {"mjs",Family::Script},{"cjs",Family::Script},{"ts",Family::Script},
    {"typescript",Family::Script},{"tsx",Family::Script},
    {"py",Family::Python},{"python",Family::Python},{"python3",Family::Python},
    {"rs",Family::Rust},{"rust",Family::Rust},
    {"go",Family::Go},{"golang",Family::Go},
    {"sh",Family::Shell},{"bash",Family::Shell},{"zsh",Family::Shell},
    {"shell",Family::Shell},{"console",Family::Shell},{"fish",Family::Shell},
    {"json",Family::Json},{"jsonc",Family::Json},{"json5",Family::Json},
    {"yaml",Family::Yaml},{"yml",Family::Yaml},{"toml",Family::Yaml},{"ini",Family::Yaml},
    {"sql",Family::Sql},{"diff",Family::Diff},{"patch",Family::Diff},
  };
  const auto found=aliases.find(lower(name));
  return found==aliases.end()?Family::None:found->second;
}

const Language& language(Family kind){
  static const std::unordered_map<int,Language> table=[]{
    std::unordered_map<int,Language> out;
    Language clike;clike.family=Family::CLike;clike.lineSlash=clike.block=true;
    clike.keywords=words({"alignas","auto","bool","break","case","catch","char",
      "class","const","consteval","constexpr","continue","co_await","co_return",
      "decltype","default","delete","do","double","else","enum","explicit","export",
      "extern","false","final","float","for","friend","goto","if","import","inline",
      "int","long","mutable","namespace","new","noexcept","nullptr","operator",
      "override","private","protected","public","return","short","signed","sizeof",
      "static","static_assert","struct","switch","template","this","throw","true",
      "try","typedef","typename","union","unsigned","using","virtual","void",
      "volatile","while","include","define","ifdef","ifndef","endif","pragma",
      "abstract","extends","implements","instanceof","interface","package","super",
      "synchronized","fun","val","var","let","func","guard","in","is","as","null",
      "when","object","data","sealed","internal","async","await","readonly","string"});
    out[static_cast<int>(Family::CLike)]=clike;
    Language script;script.family=Family::Script;script.lineSlash=script.block=true;
    script.backtick=true;
    script.keywords=words({"async","await","break","case","catch","class","const",
      "continue","debugger","default","delete","do","else","export","extends","false",
      "finally","for","from","function","if","import","in","instanceof","let","new",
      "null","of","return","static","super","switch","this","throw","true","try",
      "typeof","undefined","var","void","while","with","yield","interface","type",
      "enum","implements","private","protected","public","readonly","as","keyof",
      "declare","namespace","abstract","any","boolean","number","string","never",
      "unknown","satisfies"});
    out[static_cast<int>(Family::Script)]=script;
    Language python;python.family=Family::Python;python.lineHash=true;
    python.keywords=words({"and","as","assert","async","await","break","class",
      "continue","def","del","elif","else","except","False","finally","for","from",
      "global","if","import","in","is","lambda","None","nonlocal","not","or","pass",
      "raise","return","True","try","while","with","yield","match","case","self"});
    out[static_cast<int>(Family::Python)]=python;
    Language rust;rust.family=Family::Rust;rust.lineSlash=rust.block=true;
    rust.singleQuote=false;
    rust.keywords=words({"as","async","await","break","const","continue","crate",
      "dyn","else","enum","extern","false","fn","for","if","impl","in","let","loop",
      "match","mod","move","mut","pub","ref","return","self","Self","static","struct",
      "super","trait","true","type","unsafe","use","where","while","Some","None",
      "Ok","Err"});
    out[static_cast<int>(Family::Rust)]=rust;
    Language go;go.family=Family::Go;go.lineSlash=go.block=go.backtick=true;
    go.keywords=words({"break","case","chan","const","continue","default","defer",
      "else","fallthrough","for","func","go","goto","if","import","interface","map",
      "package","range","return","select","struct","switch","type","var","nil",
      "true","false","iota","string","int","bool","error","byte","rune"});
    out[static_cast<int>(Family::Go)]=go;
    Language shell;shell.family=Family::Shell;shell.lineHash=shell.backtick=true;
    shell.typeCase=false;
    shell.keywords=words({"if","then","else","elif","fi","for","while","until","do",
      "done","case","esac","in","function","return","export","local","readonly",
      "set","unset","source","echo","cd","exit","shift","trap","eval","exec","sudo"});
    out[static_cast<int>(Family::Shell)]=shell;
    Language json;json.family=Family::Json;json.lineSlash=json.block=true;
    json.typeCase=false;json.keywords=words({"true","false","null"});
    out[static_cast<int>(Family::Json)]=json;
    Language yaml;yaml.family=Family::Yaml;yaml.lineHash=true;yaml.typeCase=false;
    yaml.keywords=words({"true","false","null","yes","no","on","off"});
    out[static_cast<int>(Family::Yaml)]=yaml;
    Language sql;sql.family=Family::Sql;sql.lineDash=sql.block=true;
    sql.typeCase=false;sql.caseInsensitive=true;
    sql.keywords=words({"select","from","where","and","or","not","insert","into",
      "values","update","set","delete","create","table","index","drop","alter","add",
      "join","left","right","inner","outer","on","as","group","by","order","having",
      "limit","offset","distinct","union","all","null","is","in","like","primary",
      "key","foreign","references","default","unique","case","when","then","else",
      "end","exists","between","integer","text","varchar","boolean","returning",
      "with","begin","commit","rollback","transaction","view","if"});
    out[static_cast<int>(Family::Sql)]=sql;
    Language diff;diff.family=Family::Diff;
    out[static_cast<int>(Family::Diff)]=diff;
    return out;
  }();
  static const Language none;
  const auto found=table.find(static_cast<int>(kind));
  return found==table.end()?none:found->second;
}

bool identStart(char ch){return std::isalpha(static_cast<unsigned char>(ch))||ch=='_';}
bool identPart(char ch){return std::isalnum(static_cast<unsigned char>(ch))||ch=='_';}

struct Emitter {
  const SyntaxPalette& palette;
  std::string out;
  void plain(std::string_view text){out.append(text);}
  void styled(std::string_view text,const std::string& sgr,bool italic=false){
    if(sgr.empty()&&!italic){out.append(text);return;}
    if(italic)out+="\x1b[3m";
    out+=sgr;out.append(text);
    out+=italic?"\x1b[39;23m":"\x1b[39m";
  }
};

std::string highlightDiff(std::string_view code,const SyntaxPalette& palette){
  Emitter emit{palette,{}};
  std::size_t start=0;
  while(start<=code.size()){
    const auto end=code.find('\n',start);
    const auto line=code.substr(start,end==std::string_view::npos?
      std::string_view::npos:end-start);
    if(line.starts_with("+++")||line.starts_with("---"))emit.styled(line,palette.hunk);
    else if(line.starts_with("+"))emit.styled(line,palette.added);
    else if(line.starts_with("-"))emit.styled(line,palette.removed);
    else if(line.starts_with("@@"))emit.styled(line,palette.hunk);
    else emit.plain(line);
    if(end==std::string_view::npos)break;
    emit.plain("\n");start=end+1;
  }
  return std::move(emit.out);
}
} // namespace

bool supportsSyntaxLanguage(std::string_view name){
  return family(name)!=Family::None;
}

std::string languageForPath(std::string_view path){
  const auto slash=path.find_last_of("/\\");
  const auto name=lower(path.substr(slash==std::string_view::npos?0:slash+1));
  if(name=="makefile"||name=="dockerfile")return "sh";
  const auto dot=name.find_last_of('.');
  const auto extension=dot==std::string::npos?name:name.substr(dot+1);
  return supportsSyntaxLanguage(extension)?extension:std::string{};
}

std::string highlightSyntax(std::string_view code,std::string_view name,
    const SyntaxPalette& palette){
  const auto kind=family(name);
  if(kind==Family::None)return std::string(code);
  if(kind==Family::Diff)return highlightDiff(code,palette);
  const auto& lang=language(kind);
  Emitter emit{palette,{}};
  std::size_t i=0;
  bool lineStart=true;
  const auto rest=[&](std::size_t from){return code.substr(from);};
  while(i<code.size()){
    const char ch=code[i];
    if(ch=='\n'){emit.plain("\n");++i;lineStart=true;continue;}
    const bool startOfLine=lineStart;
    if(!std::isspace(static_cast<unsigned char>(ch)))lineStart=false;
    // Comments.
    const bool slashComment=lang.lineSlash&&rest(i).starts_with("//");
    const bool hashComment=lang.lineHash&&ch=='#'&&
      (kind!=Family::Shell||startOfLine||i==0||std::isspace(static_cast<unsigned char>(code[i-1])));
    const bool dashComment=lang.lineDash&&rest(i).starts_with("--");
    if(slashComment||hashComment||dashComment){
      const auto end=code.find('\n',i);
      const auto length=end==std::string_view::npos?code.size()-i:end-i;
      emit.styled(code.substr(i,length),palette.comment,true);
      i+=length;continue;
    }
    if(lang.block&&rest(i).starts_with("/*")){
      const auto end=code.find("*/",i+2);
      const auto length=end==std::string_view::npos?code.size()-i:end+2-i;
      emit.styled(code.substr(i,length),palette.comment,true);
      i+=length;continue;
    }
    // Strings, including Python triple quotes that span lines.
    if(kind==Family::Python&&(rest(i).starts_with("\"\"\"")||rest(i).starts_with("'''"))){
      const auto quote=code.substr(i,3);
      const auto end=code.find(quote,i+3);
      const auto length=end==std::string_view::npos?code.size()-i:end+3-i;
      emit.styled(code.substr(i,length),palette.string);
      i+=length;continue;
    }
    const bool quote=ch=='"'||(ch=='\''&&lang.singleQuote)||(ch=='`'&&lang.backtick)||
      (ch=='\''&&kind==Family::Rust&&i+2<code.size()&&
        (code[i+2]=='\''||(code[i+1]=='\\'&&i+3<code.size()&&code[i+3]=='\'')));
    if(quote){
      std::size_t j=i+1;
      while(j<code.size()&&code[j]!=ch){
        if(code[j]=='\\'&&j+1<code.size()){j+=2;continue;}
        if(code[j]=='\n'&&ch!='`')break;
        ++j;
      }
      const auto length=std::min(code.size(),j+1)-i;
      const auto text=code.substr(i,length);
      // JSON and YAML keys read better in the type color than as strings.
      std::size_t after=i+length;
      while(after<code.size()&&(code[after]==' '||code[after]=='\t'))++after;
      const bool key=(kind==Family::Json)&&after<code.size()&&code[after]==':';
      emit.styled(text,key?palette.type:palette.string);
      i+=length;continue;
    }
    if(std::isdigit(static_cast<unsigned char>(ch))&&(i==0||!identPart(code[i-1]))){
      std::size_t j=i+1;
      while(j<code.size()&&(identPart(code[j])||code[j]=='.'||
          ((code[j]=='+'||code[j]=='-')&&(code[j-1]=='e'||code[j-1]=='E'))))++j;
      emit.styled(code.substr(i,j-i),palette.number);
      i=j;continue;
    }
    if(kind==Family::Shell&&ch=='$'&&i+1<code.size()&&
        (identStart(code[i+1])||code[i+1]=='{')){
      std::size_t j=i+1;
      if(code[j]=='{'){while(j<code.size()&&code[j]!='}')++j;if(j<code.size())++j;}
      else while(j<code.size()&&identPart(code[j]))++j;
      emit.styled(code.substr(i,j-i),palette.type);
      i=j;continue;
    }
    if(identStart(ch)){
      std::size_t j=i+1;
      while(j<code.size()&&identPart(code[j]))++j;
      const auto word=std::string(code.substr(i,j-i));
      const bool keyword=lang.keywords.contains(lang.caseInsensitive?lower(word):word);
      std::size_t after=j;
      while(after<code.size()&&code[after]==' ')++after;
      const bool yamlKey=kind==Family::Yaml&&startOfLine&&after<code.size()&&
        (code[after]==':'||code[after]=='=');
      if(keyword)emit.styled(word,palette.keyword);
      else if(yamlKey)emit.styled(word,palette.type);
      else if(lang.typeCase&&std::isupper(static_cast<unsigned char>(word.front()))&&
          std::any_of(word.begin(),word.end(),[](char c){
            return std::islower(static_cast<unsigned char>(c));}))
        emit.styled(word,palette.type);
      else emit.plain(word);
      i=j;continue;
    }
    emit.plain(code.substr(i,1));++i;
  }
  return std::move(emit.out);
}
} // namespace batbox::interactive::markdown
