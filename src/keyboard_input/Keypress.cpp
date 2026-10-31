#include "keyboard_input/Keyboard.hpp"
#include "keyboard_input/Regex.hpp"
namespace batbox::keyboard_input {
ParsedKey parseKeypress(const std::string& s){
  ParsedKey key;key.name="";key.sequence=s;key.raw=s;
  Match m;
  static const glob::Regex16 csiU(u"^\x1b\\[(\\d+)(?:;(\\d+))?u");
  static const glob::Regex16 modify(u"^\x1b\\[27;(\\d+);(\\d+)~");
  if(match(csiU,s,m)){
    const auto codepoint=value::parseInt10(group(m,1));
    const auto modifier=group(m,2).empty()?1:value::parseInt10(group(m,2));
    const auto mods=decodeModifier(modifier);
    key.name=keycodeToName(codepoint);key.ctrl=mods.ctrl;key.meta=mods.meta;
    key.shift=mods.shift;key.super=mods.super;return key;
  }
  if(match(modify,s,m)){
    const auto mods=decodeModifier(value::parseInt10(group(m,1)));
    key.name=keycodeToName(value::parseInt10(group(m,2)));
    key.ctrl=mods.ctrl;key.meta=mods.meta;key.shift=mods.shift;key.super=mods.super;return key;
  }
  if(match(sgrMouseRegex(),s,m)){
    const auto button=toUint32(value::parseInt10(group(m,1)));
    if((button&0x43)==0x40)return createNavKey(s,"wheelup",false);
    if((button&0x43)==0x41)return createNavKey(s,"wheeldown",false);
    return createNavKey(s,"mouse",false);
  }
  const auto units=value::utf16CodeUnits(s);
  if(units.size()==6&&s.starts_with("\x1b[M")){
    const auto button=toUint32(static_cast<double>(units[3])-32);
    if((button&0x43)==0x40)return createNavKey(s,"wheelup",false);
    if((button&0x43)==0x41)return createNavKey(s,"wheeldown",false);
    return createNavKey(s,"mouse",false);
  }
  if(s=="\r"){key.raw.reset();key.name="return";}
  else if(s=="\n")key.name="enter";
  else if(s=="\t")key.name="tab";
  else if(s=="\b"||s=="\x1b\b"){key.name="backspace";key.meta=units[0]==0x1b;}
  else if(s=="\x7f"||s=="\x1b\x7f"){key.name="backspace";key.meta=units[0]==0x1b;}
  else if(s=="\x1b"||s=="\x1b\x1b"){key.name="escape";key.meta=units.size()==2;}
  else if(s==" "||s=="\x1b "){key.name="space";key.meta=units.size()==2;}
  else if(s=="\x1f"){key.name="_";key.ctrl=true;}
  else if(units.size()==1&&units[0]<=0x1a){key.name=value::fromUtf16(std::u16string(1,static_cast<char16_t>(units[0]+96)));key.ctrl=true;}
  else if(units.size()==1&&units[0]>=u'0'&&units[0]<=u'9')key.name="number";
  else if(units.size()==1&&units[0]>=u'a'&&units[0]<=u'z')key.name=s;
  else if(units.size()==1&&units[0]>=u'A'&&units[0]<=u'Z'){key.name=std::string(1,static_cast<char>(units[0]+32));key.shift=true;}
  else{
    static const glob::Regex16 meta(u"^(?:\x1b)([a-zA-Z0-9])$");
    static const glob::Regex16 fn(u"^(?:\x1b+)(O|N|\\[|\\[\\[)(?:(\\d+)(?:;(\\d+))?([~^$])|(?:1;)?(\\d+)?([a-zA-Z]))");
    if(match(meta,s,m)){
      key.meta=true;const auto letter=group(m,1);key.shift=letter.size()==1&&letter[0]>='A'&&letter[0]<='Z';
    }else if(match(fn,s,m)){
      if(units.size()>1&&units[0]==0x1b&&units[1]==0x1b)key.option=true;
      const auto code=group(m,1)+group(m,2)+group(m,4)+group(m,6);
      const auto modifierText=!group(m,3).empty()?group(m,3):group(m,5);
      const auto modifier=(modifierText.empty()?1:value::parseInt10(modifierText))-1;
      const auto bits=toUint32(modifier);
      key.ctrl=bits&4;key.meta=bits&2;key.super=bits&8;key.shift=bits&1;key.code=code;
      key.name=keyName(code);key.shift=isShiftKey(code)||key.shift;key.ctrl=isCtrlKey(code)||key.ctrl;
    }
  }
  if(key.raw==std::optional<std::string>("\x1b" "b")){key.meta=true;key.name="left";}
  else if(key.raw==std::optional<std::string>("\x1b" "f")){key.meta=true;key.name="right";}
  if(s=="\x1b[1~")return createNavKey(s,"home",false);
  if(s=="\x1b[4~")return createNavKey(s,"end",false);
  if(s=="\x1b[5~")return createNavKey(s,"pageup",false);
  if(s=="\x1b[6~")return createNavKey(s,"pagedown",false);
  if(s=="\x1b[1;5D")return createNavKey(s,"left",true);
  if(s=="\x1b[1;5C")return createNavKey(s,"right",true);
  return key;
}
}
