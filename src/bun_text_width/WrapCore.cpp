#include "bun_text_width/WrapInternal.hpp"
#include <algorithm>

namespace batbox::bun_text_width {
void processLine(std::u16string_view line,const WrapContext& context,Rows& rows){
  const auto& options=context.options;
  if(options.trim){
    std::size_t trimStart=0,trimEnd=line.size();
    while(trimStart<trimEnd&&(line[trimStart]==' '||line[trimStart]=='\t'))++trimStart;
    while(trimEnd>trimStart&&(line[trimEnd-1]==' '||line[trimEnd-1]=='\t'))--trimEnd;
    if(trimStart>=trimEnd)return;
  }
  std::vector<std::size_t> wordLengths;
  std::size_t wordStart=0;
  while(wordStart<=line.size()){
    const auto space=line.find(u' ',wordStart);
    const auto wordEnd=space==std::u16string_view::npos?line.size():space;
    wordLengths.push_back(wordStart<wordEnd?
      context.width(line.substr(wordStart,wordEnd-wordStart)):0);
    if(space==std::u16string_view::npos)break;
    wordStart=wordEnd+1;
  }
  rows.emplace_back();
  wordStart=0;
  std::size_t wordIndex=0;
  for(std::size_t i=0;i<=line.size();++i){
    if(i<line.size()&&line[i]!=' ')continue;
    const auto word=line.substr(wordStart,i-wordStart);
    if(options.trim)rows.back().trimLeadingSpaces();
    std::size_t rowLength=rows.back().width(context);
    if(wordIndex!=0){
      if(rowLength>=context.columns&&(!options.wordWrap||!options.trim)){
        rows.emplace_back();rowLength=0;
      }
      if(rowLength>0||!options.trim){
        rows.back().data+=u' ';++rowLength;
      }
    }
    const std::size_t wordLength=wordIndex<wordLengths.size()?wordLengths[wordIndex]:0;
    if(options.hard&&wordLength>context.columns){
      const auto remaining=context.columns>rowLength?context.columns-rowLength:0;
      const auto breaksThis=1+(wordLength>remaining?
        (wordLength-remaining-1)/context.columns:0);
      const auto breaksNext=wordLength>0?(wordLength-1)/context.columns:0;
      if(breaksNext<breaksThis)rows.emplace_back();
      wrapWord(rows,word,context);
      wordStart=i+1;++wordIndex;continue;
    }
    if(rowLength+wordLength>context.columns&&rowLength>0&&wordLength>0){
      if(!options.wordWrap&&rowLength<context.columns){
        wrapWord(rows,word,context);
        wordStart=i+1;++wordIndex;continue;
      }
      rows.emplace_back();
    }
    rowLength=rows.back().width(context);
    if(rowLength+wordLength>context.columns&&!options.wordWrap){
      wrapWord(rows,word,context);
      wordStart=i+1;++wordIndex;continue;
    }
    rows.back().data+=word;
    wordStart=i+1;++wordIndex;
  }
  if(options.trim)for(auto& row:rows)trimRowTrailingSpaces(row,context);
}

std::u16string wrapUnits(std::u16string_view input,const WrapContext& context){
  if(context.columns==0||input.empty())return std::u16string(input);
  std::u16string normalized;
  normalized.reserve(input.size());
  for(std::size_t i=0;i<input.size();++i){
    if(input[i]=='\r'){
      normalized+=u'\n';
      if(i+1<input.size()&&input[i+1]=='\n')++i;
    }else normalized+=input[i];
  }
  std::u16string result;
  result.reserve(input.size()+input.size()/10);
  bool firstLine=true;
  std::size_t lineStart=0;
  while(lineStart<=normalized.size()){
    const auto newline=normalized.find(u'\n',lineStart);
    const auto lineEnd=newline==std::u16string::npos?normalized.size():newline;
    if(!firstLine)result+=u'\n';
    firstLine=false;
    Rows rows;
    processLine(std::u16string_view(normalized).substr(lineStart,lineEnd-lineStart),
      context,rows);
    if(!rows.empty())joinRowsWithAnsiPreservation(rows,result);
    lineStart=lineEnd+1;
  }
  return result;
}
} // namespace batbox::bun_text_width
