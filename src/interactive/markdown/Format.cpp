#include "interactive/markdown/Format.hpp"
#include "interactive/markdown/Lexer.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <regex>
#include <stdexcept>

namespace batbox::interactive::markdown {
namespace {
void require(const auto& callback,const char* name){
  if(!callback)throw std::logic_error(std::string(name)+" required for Markdown");
}
std::string trim(const std::string& text){return value::trim(text);}
std::string inner(const Tokens& tokens,const FormatServices& services,
                  const Token* parent=nullptr,int depth=0,
                  std::optional<int> number={}){
  std::string out;
  bool beginning=true;
  for(const auto& token:tokens){
    auto formatted=formatToken(token,services,depth,number,parent);
    if(beginning&&token.kind==Kind::Strong&&services.strongLeadIn){
      const auto label=trim(services.stripAnsi?services.stripAnsi(formatted):token.text);
      if(!label.empty()&&label.back()==':')formatted=services.strongLeadIn(formatted);
    }
    out+=formatted;
    if(token.kind==Kind::Break||token.kind==Kind::Space)beginning=true;
    else if(token.kind!=Kind::Text||!trim(token.text).empty())beginning=false;
  }
  return out;
}
std::string listNumber(int depth,int value){
  if(depth==2){
    std::string result;
    while(value>0){--value;result.insert(result.begin(),
      static_cast<char>('a'+value%26));value/=26;}
    return result;
  }
  if(depth==3){
    std::string result;
    constexpr std::pair<int,const char*> digits[]={{1000,"m"},{900,"cm"},
      {500,"d"},{400,"cd"},{100,"c"},{90,"xc"},{50,"l"},
      {40,"xl"},{10,"x"},{9,"ix"},{5,"v"},{4,"iv"},{1,"i"}};
    for(const auto& [n,symbol]:digits)
      while(value>=n){result+=symbol;value-=n;}
    return result;
  }
  return std::to_string(value);
}
std::string linkify(const std::string& text,const FormatServices& services){
  require(services.supportsHyperlinks,"hyperlink capability");
  if(!services.supportsHyperlinks())return text;
  require(services.createHyperlink,"hyperlink creator");
  static const std::regex issue(
    R"((^|[^\w./-])([A-Za-z0-9][\w-]*/[A-Za-z0-9][\w.-]*)#([0-9]+)\b)");
  std::string result;
  std::size_t previous=0;
  for(std::sregex_iterator it(text.begin(),text.end(),issue),end;
      it!=end;++it){
    const auto& match=*it;
    result+=text.substr(previous,static_cast<std::size_t>(match.position())-previous);
    const auto repo=match.str(2),number=match.str(3);
    result+=match.str(1)+services.createHyperlink(
      "https://github.com/"+repo+"/issues/"+number,repo+"#"+number);
    previous=static_cast<std::size_t>(match.position()+match.length());
  }
  return result+text.substr(previous);
}
std::string tablePipe(const Token& table,const FormatServices& services){
  require(services.stripAnsi,"ANSI stripper");
  require(services.stringWidth,"Unicode string width");
  const auto formatCell=[&](const Tokens& tokens){return inner(tokens,services);};
  const auto width=[&](const Tokens& tokens){
    return services.stringWidth(services.stripAnsi(formatCell(tokens)));};
  std::vector<int> widths;
  for(std::size_t col=0;col<table.header.size();++col){
    int w=std::max(width(table.header[col]),3);
    for(const auto& row:table.rows)
      if(col<row.size())w=std::max(w,width(row[col]));
    widths.push_back(w);
  }
  const auto row=[&](const std::vector<Tokens>& cells){
    std::string out="| ";
    for(std::size_t col=0;col<cells.size();++col){
      if(col>=widths.size())break;
      const auto content=formatCell(cells[col]);
      const auto plain=services.stripAnsi(content);
      const auto align=col<table.align.size()?table.align[col]:"left";
      out+=padAligned(content,services.stringWidth(plain),widths[col],align)+" | ";
    }
    while(!out.empty()&&out.back()==' ')out.pop_back();
    return out+"\n";
  };
  std::string output=row(table.header)+"|";
  for(const auto width:widths)output+=std::string(width+2,'-')+"|";
  output+='\n';
  for(const auto& cells:table.rows)output+=row(cells);
  return output+'\n';
}
std::string listItem(const Token& token,const FormatServices& services,
                     int depth,std::optional<int> number){
  std::string output;
  const auto plainPrefix=number?listNumber(depth+1,*number)+". ":"- ";
  const auto prefix=services.listMarker?services.listMarker(plainPrefix):plainPrefix;
  bool first=true;
  for(const auto& child:token.children){
    if(child.kind==Kind::Paragraph){
      output+=std::string(depth*2,' ');
      if(first){output+=prefix;first=false;}
      output+=inner(child.children,services)+"\n";
    }else if(child.kind==Kind::List){
      output+=formatToken(child,services,depth+1,{},&token);
    }else{
      output+=std::string(depth*2,' ');
      if(first){output+=prefix;first=false;}
      output+=formatToken(child,services,depth+1,number,&token);
    }
  }
  if(first)output+=std::string(depth*2,' ')+prefix+"\n";
  return output;
}
} // namespace

std::string formatToken(const Token& token,const FormatServices& services,
    int listDepth,std::optional<int> orderedListNumber,const Token* parent){
  switch(token.kind){
  case Kind::BlockQuote:{
    require(services.dim,"dim style");require(services.italic,"italic style");
    const auto text=inner(token.children,services);
    std::string output;
    std::size_t start=0;
    do{
      const auto end=text.find('\n',start);
      const auto line=text.substr(start,end==std::string::npos?
        std::string::npos:end-start);
      if(!services.stripAnsi)throw std::logic_error("ANSI stripper required for Markdown");
      if(!trim(services.stripAnsi(line)).empty())
        output+=services.dim(services.blockQuoteBar)+" "+services.italic(line);
      else output+=line;
      if(end==std::string::npos)break;
      output+='\n';start=end+1;
    }while(true);
    return output;
  }
  case Kind::Code:{
    if(!services.highlight)return token.text+'\n';
    std::string language="plaintext";
    if(!token.language.empty()){
      require(services.supportsLanguage,"syntax language test");
      if(services.supportsLanguage(token.language))language=token.language;
      else{
        require(services.debug,"syntax highlighting diagnostic");
        services.debug("Language not supported while highlighting code, falling back to plaintext: "+token.language);
      }
    }
    return services.highlight(token.text,language)+'\n';
  }
  case Kind::CodeSpan:
    require(services.permission,"inline code style");
    return services.permission(token.text);
  case Kind::Emphasis:
    require(services.italic,"italic style");
    return services.italic(inner(token.children,services,parent));
  case Kind::Strong:
    require(services.bold,"bold style");
    return services.bold(inner(token.children,services,parent));
  case Kind::Heading:{
    require(services.bold,"bold style");
    const auto content=inner(token.children,services);
    if(token.depth==1){
      require(services.italic,"italic style");
      require(services.underline,"underline style");
      return services.underline(services.italic(services.bold(content)))+"\n\n";
    }
    return services.bold(content)+"\n\n";
  }
  case Kind::Rule:return "---";
  case Kind::Image:return token.href;
  case Kind::Link:{
    if(token.href.starts_with("mailto:"))return token.href.substr(7);
    require(services.stripAnsi,"ANSI stripper");
    require(services.createHyperlink,"hyperlink creator");
    const auto text=inner(token.children,services,&token);
    const auto plainText=services.stripAnsi(text);
    if(!plainText.empty()&&plainText!=token.href)
      return services.createHyperlink(token.href,text);
    return services.createHyperlink(token.href,{});
  }
  case Kind::List:{
    std::string output;
    for(std::size_t i=0;i<token.items.size();++i)
      output+=listItem(token.items[i],services,listDepth,
        token.ordered?std::optional<int>(token.start+static_cast<int>(i)):
          std::optional<int>{});
    return output;
  }
  case Kind::ListItem:return listItem(token,services,listDepth,orderedListNumber);
  case Kind::Paragraph:return inner(token.children,services)+'\n';
  case Kind::Space:
  case Kind::Break:return "\n";
  case Kind::Text:
    if(parent&&parent->kind==Kind::Link)return token.text;
    if(parent&&parent->kind==Kind::ListItem){
      const auto prefix=orderedListNumber?listNumber(listDepth,*orderedListNumber)+". ":"- ";
      return (services.listMarker?services.listMarker(prefix):prefix)+
        (token.children.empty()?linkify(token.text,services):
          inner(token.children,services,parent,listDepth,orderedListNumber))+"\n";
    }
    return linkify(token.text,services);
  case Kind::Table:return tablePipe(token,services);
  case Kind::Escape:return token.text;
  case Kind::Definition:
  case Kind::Deleted:
  case Kind::Html:return {};
  }
  return {};
}
std::string formatTokens(const Tokens& tokens,const FormatServices& services){
  return inner(tokens,services);
}
std::string padAligned(const std::string& content,int displayWidth,
    int targetWidth,const std::string& align){
  const int padding=std::max(0,targetWidth-displayWidth);
  if(align=="center"){
    const auto left=padding/2;
    return std::string(left,' ')+content+std::string(padding-left,' ');
  }
  if(align=="right")return std::string(padding,' ')+content;
  return content+std::string(padding,' ');
}
std::string stripPoetryXMLTags(const std::string& content){
  static const std::regex tags(
    R"(<(commit_analysis|context|function_analysis|pr_analysis)>[\s\S]*?</\1>\n?)");
  return trim(std::regex_replace(content,tags,""));
}
FormatServices indigenousFormatServices(bool colors,bool hyperlinks,
    std::string permissionSgr,
    std::function<int(const std::string&)> width){
  const auto style=[colors](const char* start,const char* end){
    return [colors,start,end](const std::string& text){
      return colors?std::string(start)+text+end:text;};
  };
  FormatServices services;
  services.bold=style("\x1b[1m","\x1b[22m");
  services.italic=style("\x1b[3m","\x1b[23m");
  services.underline=style("\x1b[4m","\x1b[24m");
  services.dim=style("\x1b[2m","\x1b[22m");
  services.permission=[colors,code=std::move(permissionSgr)](
      const std::string& text){
    return colors?code+text+"\x1b[39m":text;};
  services.supportsHyperlinks=[hyperlinks]{return hyperlinks;};
  services.createHyperlink=[colors,hyperlinks](const std::string& url,
      const std::optional<std::string>& content){
    if(!hyperlinks)return url;
    const auto display=content.value_or(url);
    return "\x1b]8;;"+url+"\x07"+
      (colors?"\x1b[34m"+display+"\x1b[39m":display)+"\x1b]8;;\x07";
  };
  services.stripAnsi=[](const std::string& text){
    std::string out;
    for(std::size_t i=0;i<text.size();){
      if(text[i]!='\x1b'){out+=text[i++];continue;}
      if(i+1<text.size()&&text[i+1]=='['){
        i+=2;while(i<text.size()&&text[i]!='m')++i;
        if(i<text.size())++i;
      }else if(i+1<text.size()&&text[i+1]==']'){
        i+=2;while(i<text.size()&&text[i]!='\x07')++i;
        if(i<text.size())++i;
      }else ++i;
    }
    return out;
  };
  services.stringWidth=std::move(width);
  return services;
}
} // namespace batbox::interactive::markdown
