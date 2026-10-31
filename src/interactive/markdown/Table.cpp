#include "interactive/markdown/Table.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <regex>
#include <stdexcept>

namespace batbox::interactive::markdown {
namespace {
constexpr int safetyMargin=4,minColumnWidth=3,maxRowLines=4;
std::vector<std::string> splitLines(const std::string& text){
  std::vector<std::string> lines;
  std::size_t start=0;
  do{
    const auto end=text.find('\n',start);
    lines.push_back(text.substr(start,end==std::string::npos?
      std::string::npos:end-start));
    if(end==std::string::npos)break;
    start=end+1;
  }while(true);
  return lines;
}
std::string trimEnd(std::string text){
  while(!text.empty()&&std::isspace(static_cast<unsigned char>(text.back())))
    text.pop_back();
  return text;
}
std::vector<std::string> wrapText(const std::string& text,int width,bool hard,
    const WrapAnsi& wrap){
  if(width<=0)return {text};
  if(!wrap)throw std::logic_error("ANSI wrapping required for Markdown table");
  auto lines=splitLines(wrap(trimEnd(text),width,hard));
  lines.erase(std::remove_if(lines.begin(),lines.end(),
    [](const auto& line){return line.empty();}),lines.end());
  if(lines.empty())lines.emplace_back();
  return lines;
}
std::string repeat(const std::string& text,int count){
  if(count<0)throw std::range_error("Invalid Markdown table width");
  std::string output;
  for(int i=0;i<count;++i)output+=text;
  return output;
}
} // namespace

std::string renderTable(const Token& table,int terminalWidth,
    const FormatServices& services,const WrapAnsi& wrapAnsi){
  if(table.kind!=Kind::Table)throw std::invalid_argument("Markdown table token required");
  if(!services.stripAnsi||!services.stringWidth)
    throw std::logic_error("Markdown table width imports required");
  const auto formatCell=[&](const Tokens& tokens){return formatTokens(tokens,services);};
  const auto plainCell=[&](const Tokens& tokens){
    return services.stripAnsi(formatCell(tokens));};
  const auto minWidth=[&](const Tokens& tokens){
    const auto text=plainCell(tokens);
    int width=minColumnWidth;
    static const std::regex words(R"([^\s]+)");
    for(std::sregex_iterator it(text.begin(),text.end(),words),end;it!=end;++it)
      width=std::max(width,services.stringWidth(it->str()));
    return width;
  };
  const auto idealWidth=[&](const Tokens& tokens){
    return std::max(services.stringWidth(plainCell(tokens)),minColumnWidth);};
  std::vector<int> mins,ideals;
  const auto columns=table.header.size();
  for(std::size_t col=0;col<columns;++col){
    int min=minWidth(table.header[col]),ideal=idealWidth(table.header[col]);
    for(const auto& row:table.rows){
      if(col>=row.size())continue;
      min=std::max(min,minWidth(row[col]));
      ideal=std::max(ideal,idealWidth(row[col]));
    }
    mins.push_back(min);ideals.push_back(ideal);
  }
  const auto overhead=1+static_cast<int>(columns)*3;
  const auto available=std::max(terminalWidth-overhead-safetyMargin,
    static_cast<int>(columns)*minColumnWidth);
  int totalMin=0,totalIdeal=0;
  for(const auto value:mins)totalMin+=value;
  for(const auto value:ideals)totalIdeal+=value;
  bool hard=false;
  std::vector<int> widths;
  if(totalIdeal<=available)widths=ideals;
  else if(totalMin<=available){
    const auto extra=available-totalMin;
    const auto overflow=totalIdeal-totalMin;
    for(std::size_t i=0;i<columns;++i)
      widths.push_back(mins[i]+(overflow==0?0:
        static_cast<int>(std::floor(static_cast<double>(ideals[i]-mins[i])/
          overflow*extra))));
  }else{
    hard=true;
    const auto scale=static_cast<double>(available)/totalMin;
    for(const auto min:mins)widths.push_back(std::max(
      static_cast<int>(std::floor(min*scale)),minColumnWidth));
  }
  int maximumLines=1;
  const auto scan=[&](const std::vector<Tokens>& cells){
    for(std::size_t i=0;i<cells.size()&&i<widths.size();++i)
      maximumLines=std::max(maximumLines,static_cast<int>(
        wrapText(formatCell(cells[i]),widths[i],hard,wrapAnsi).size()));
  };
  scan(table.header);
  for(const auto& row:table.rows)scan(row);
  const auto vertical=[&](){
    std::vector<std::string> lines,headers;
    for(const auto& cell:table.header)headers.push_back(plainCell(cell));
    const auto separator=repeat("─",std::min(terminalWidth-1,40));
    for(std::size_t rowIndex=0;rowIndex<table.rows.size();++rowIndex){
      if(rowIndex)lines.push_back(separator);
      const auto& row=table.rows[rowIndex];
      for(std::size_t col=0;col<row.size();++col){
        const auto label=col<headers.size()&&!headers[col].empty()?headers[col]:
          "Column "+std::to_string(col+1);
        auto value=trimEnd(formatCell(row[col]));
        value=std::regex_replace(value,std::regex(R"(\n+)")," ");
        value=value::trim(std::regex_replace(value,std::regex(R"(\s+)")," "));
        const auto firstWidth=terminalWidth-services.stringWidth(label)-3;
        const auto followingWidth=terminalWidth-3;
        auto wrapped=wrapText(value,std::max(firstWidth,10),false,wrapAnsi);
        if(wrapped.size()>1&&followingWidth>firstWidth){
          std::string remainder;
          for(std::size_t i=1;i<wrapped.size();++i){
            if(!remainder.empty())remainder+=' ';
            remainder+=value::trim(wrapped[i]);
          }
          const auto tail=wrapText(remainder,followingWidth,false,wrapAnsi);
          wrapped.resize(1);wrapped.insert(wrapped.end(),tail.begin(),tail.end());
        }
        lines.push_back("\x1b[1m"+label+":\x1b[22m "+wrapped.front());
        for(std::size_t i=1;i<wrapped.size();++i)
          if(!value::trim(wrapped[i]).empty())lines.push_back("  "+wrapped[i]);
      }
    }
    std::string out;
    for(std::size_t i=0;i<lines.size();++i){if(i)out+='\n';out+=lines[i];}
    return out;
  };
  if(maximumLines>maxRowLines)return vertical();
  const auto border=[&](char which){
    const char* left=which=='t'?"┌":which=='m'?"├":"└";
    const char* cross=which=='t'?"┬":which=='m'?"┼":"┴";
    const char* right=which=='t'?"┐":which=='m'?"┤":"┘";
    std::string result=left;
    for(std::size_t col=0;col<columns;++col){
      result+=repeat("─",widths[col]+2);
      result+=col+1<columns?cross:right;
    }
    return result;
  };
  const auto rowLines=[&](const std::vector<Tokens>& cells,bool header){
    std::vector<std::vector<std::string>> wrapped;
    int maxLines=1;
    for(std::size_t col=0;col<cells.size()&&col<widths.size();++col){
      wrapped.push_back(wrapText(formatCell(cells[col]),widths[col],hard,wrapAnsi));
      maxLines=std::max(maxLines,static_cast<int>(wrapped.back().size()));
    }
    std::vector<std::string> lines;
    for(int line=0;line<maxLines;++line){
      std::string output="│";
      for(std::size_t col=0;col<wrapped.size();++col){
        const auto offset=(maxLines-static_cast<int>(wrapped[col].size()))/2;
        const auto index=line-offset;
        const auto content=index>=0&&index<static_cast<int>(wrapped[col].size())?
          wrapped[col][index]:"";
        const auto align=header?"center":
          col<table.align.size()?table.align[col]:"left";
        output+=" "+padAligned(content,services.stringWidth(content),
          widths[col],align)+" │";
      }
      lines.push_back(std::move(output));
    }
    return lines;
  };
  std::vector<std::string> lines{border('t')};
  const auto headerRows=rowLines(table.header,true);
  lines.insert(lines.end(),headerRows.begin(),headerRows.end());
  lines.push_back(border('m'));
  for(std::size_t row=0;row<table.rows.size();++row){
    const auto data=rowLines(table.rows[row],false);
    lines.insert(lines.end(),data.begin(),data.end());
    if(row+1<table.rows.size())lines.push_back(border('m'));
  }
  lines.push_back(border('b'));
  int maximumWidth=0;
  for(const auto& line:lines)
    maximumWidth=std::max(maximumWidth,
      services.stringWidth(services.stripAnsi(line)));
  if(maximumWidth>terminalWidth-safetyMargin)return vertical();
  std::string output;
  for(std::size_t i=0;i<lines.size();++i){if(i)output+='\n';output+=lines[i];}
  return output;
}
} // namespace batbox::interactive::markdown
