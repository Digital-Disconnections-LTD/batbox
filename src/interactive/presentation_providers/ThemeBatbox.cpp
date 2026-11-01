#include "interactive/presentation_providers/ThemeData.hpp"

namespace batbox::interactive::presentation_providers {
namespace {
// Legacy keys keep older card palettes coherent with the role table below.
void legacy(RawTheme& theme,const RawTheme& roles){
  const auto role=[&](const char* key){return roles.at(std::string("role.")+key);};
  for(const auto* key:{"text","background","mascotBackground",
      "bashMessageBackgroundColor","memoryBackgroundColor"})theme[key]="default";
  theme["brand"]=theme["briefLabelAssistant"]=theme["merged"]=role("brand");
  theme["brandShimmer"]=role("cream");
  theme["suggestion"]=theme["permission"]=theme["autoAccept"]=role("brand");
  theme["permissionShimmer"]=role("cream");
  theme["remember"]=theme["brandBlueSpinner"]=role("spinner");
  theme["planMode"]=role("modePlan");
  theme["bashBorder"]=theme["briefLabelYou"]=role("lantern");
  theme["success"]=role("statusOk");
  theme["warning"]=role("statusWarn");
  theme["error"]=role("statusErr");
  theme["subtle"]=theme["inactive"]=role("muted");
  theme["promptBorder"]=role("border");
  theme["promptBorderShimmer"]=role("borderFocus");
  theme["userMessageBackground"]=role("userPanel");
  theme["userMessageBackgroundHover"]=role("selectionBg");
  theme["selectionBg"]=role("selectionBg");
  theme["diffAdded"]=role("diffAddBg");
  theme["diffRemoved"]=role("diffDelBg");
  theme["diffAddedWord"]=role("diffAdd");
  theme["diffRemovedWord"]=role("diffDel");
  for(const auto& [key,value]:roles)theme[key]=value;
}
} // namespace

RawTheme batboxDarkTheme(){
  // Night purple, lantern orange, mint stars and cream from the Batbox art.
  const RawTheme roles={
    {"role.brand","rgb(185,148,247)"},{"role.brandDeep","rgb(125,92,190)"},
    {"role.lantern","rgb(255,166,87)"},{"role.mint","rgb(124,229,193)"},
    {"role.cream","rgb(244,233,213)"},
    {"role.text","default"},{"role.muted","rgb(150,143,168)"},
    {"role.faint","rgb(98,91,117)"},
    {"role.userBar","rgb(255,166,87)"},{"role.userText","default"},
    {"role.userPanel","rgb(38,32,52)"},
    {"role.assistantMarker","rgb(185,148,247)"},
    {"role.thinking","rgb(150,143,168)"},
    {"role.gadgetName","default"},{"role.gadgetTarget","default"},
    {"role.gadgetRunning","rgb(124,229,193)"},
    {"role.gadgetOk","rgb(126,214,140)"},{"role.gadgetErr","rgb(255,122,138)"},
    {"role.codeBg","rgb(30,26,42)"},{"role.codeText","default"},
    {"role.codeLabel","rgb(150,143,168)"},{"role.codeFrame","rgb(76,69,96)"},
    {"role.inlineCode","rgb(214,190,255)"},{"role.inlineCodeBg","rgb(42,35,60)"},
    {"role.heading","rgb(244,233,213)"},{"role.headingGlyph","rgb(124,229,193)"},
    {"role.headingMinorGlyph","rgb(185,148,247)"},
    {"role.link","rgb(124,229,193)"},{"role.listMarker","rgb(185,148,247)"},
    {"role.quote","rgb(150,143,168)"},
    {"role.syntaxKeyword","rgb(196,163,255)"},{"role.syntaxString","rgb(143,227,184)"},
    {"role.syntaxNumber","rgb(255,179,120)"},{"role.syntaxComment","rgb(125,117,144)"},
    {"role.syntaxType","rgb(244,214,170)"},
    {"role.diffAdd","rgb(142,222,150)"},{"role.diffAddBg","rgb(24,48,32)"},
    {"role.diffDel","rgb(255,140,150)"},{"role.diffDelBg","rgb(58,24,32)"},
    {"role.diffContext","rgb(150,143,168)"},{"role.diffLineNo","rgb(98,91,117)"},
    {"role.diffHunk","rgb(125,117,144)"},
    {"role.dimLabel","rgb(150,143,168)"},{"role.kbd","rgb(214,206,232)"},
    {"role.border","rgb(76,69,96)"},{"role.borderFocus","rgb(125,92,190)"},
    {"role.selection","rgb(244,233,213)"},{"role.selectionBg","rgb(52,40,80)"},
    {"role.spinner","rgb(124,229,193)"},
    {"role.statusOk","rgb(126,214,140)"},{"role.statusWarn","rgb(246,197,96)"},
    {"role.statusErr","rgb(255,122,138)"},
    {"role.modeDefault","rgb(150,143,168)"},{"role.modePlan","rgb(124,229,193)"},
    {"role.modeAccept","rgb(185,148,247)"},{"role.modeYolo","rgb(255,122,138)"},
  };
  auto theme=darkTheme();
  legacy(theme,roles);
  return theme;
}

RawTheme batboxLightTheme(){
  // The same identity with ink-dark tones that hold contrast on white.
  const RawTheme roles={
    {"role.brand","rgb(112,64,186)"},{"role.brandDeep","rgb(150,120,210)"},
    {"role.lantern","rgb(190,92,8)"},{"role.mint","rgb(10,130,108)"},
    {"role.cream","rgb(58,40,84)"},
    {"role.text","default"},{"role.muted","rgb(108,100,128)"},
    {"role.faint","rgb(172,165,190)"},
    {"role.userBar","rgb(190,92,8)"},{"role.userText","default"},
    {"role.userPanel","rgb(243,239,250)"},
    {"role.assistantMarker","rgb(112,64,186)"},
    {"role.thinking","rgb(108,100,128)"},
    {"role.gadgetName","default"},{"role.gadgetTarget","default"},
    {"role.gadgetRunning","rgb(10,130,108)"},
    {"role.gadgetOk","rgb(30,130,60)"},{"role.gadgetErr","rgb(200,40,60)"},
    {"role.codeBg","rgb(246,243,251)"},{"role.codeText","default"},
    {"role.codeLabel","rgb(108,100,128)"},{"role.codeFrame","rgb(200,193,216)"},
    {"role.inlineCode","rgb(120,60,190)"},{"role.inlineCodeBg","rgb(240,234,250)"},
    {"role.heading","rgb(58,40,84)"},{"role.headingGlyph","rgb(10,130,108)"},
    {"role.headingMinorGlyph","rgb(112,64,186)"},
    {"role.link","rgb(10,120,100)"},{"role.listMarker","rgb(112,64,186)"},
    {"role.quote","rgb(108,100,128)"},
    {"role.syntaxKeyword","rgb(130,60,200)"},{"role.syntaxString","rgb(10,120,80)"},
    {"role.syntaxNumber","rgb(180,90,0)"},{"role.syntaxComment","rgb(140,134,156)"},
    {"role.syntaxType","rgb(150,80,30)"},
    {"role.diffAdd","rgb(22,120,50)"},{"role.diffAddBg","rgb(222,245,226)"},
    {"role.diffDel","rgb(180,30,50)"},{"role.diffDelBg","rgb(253,228,232)"},
    {"role.diffContext","rgb(108,100,128)"},{"role.diffLineNo","rgb(160,152,178)"},
    {"role.diffHunk","rgb(140,134,156)"},
    {"role.dimLabel","rgb(108,100,128)"},{"role.kbd","rgb(58,40,84)"},
    {"role.border","rgb(200,193,216)"},{"role.borderFocus","rgb(150,120,210)"},
    {"role.selection","default"},{"role.selectionBg","rgb(232,222,250)"},
    {"role.spinner","rgb(10,130,108)"},
    {"role.statusOk","rgb(30,130,60)"},{"role.statusWarn","rgb(170,110,0)"},
    {"role.statusErr","rgb(200,40,60)"},
    {"role.modeDefault","rgb(108,100,128)"},{"role.modePlan","rgb(10,130,108)"},
    {"role.modeAccept","rgb(112,64,186)"},{"role.modeYolo","rgb(200,40,60)"},
  };
  auto theme=lightTheme();
  legacy(theme,roles);
  return theme;
}

RawTheme batboxAnsiTheme(){
  // Terminal palette indexes chosen to read on both dark and light schemes:
  // no bright variants for text and no background tints.
  const RawTheme roles={
    {"role.brand","ansi:magenta"},{"role.brandDeep","ansi:magenta"},
    {"role.lantern","ansi:yellow"},{"role.mint","ansi:cyan"},
    {"role.cream","default"},
    {"role.text","default"},{"role.muted","ansi:blackBright"},
    {"role.faint","ansi:blackBright"},
    {"role.userBar","ansi:yellow"},{"role.userText","default"},
    {"role.userPanel","default"},
    {"role.assistantMarker","ansi:magenta"},
    {"role.thinking","ansi:blackBright"},
    {"role.gadgetName","default"},{"role.gadgetTarget","default"},
    {"role.gadgetRunning","ansi:cyan"},
    {"role.gadgetOk","ansi:green"},{"role.gadgetErr","ansi:red"},
    {"role.codeBg","default"},{"role.codeText","default"},
    {"role.codeLabel","ansi:blackBright"},{"role.codeFrame","ansi:blackBright"},
    {"role.inlineCode","ansi:magenta"},{"role.inlineCodeBg","default"},
    {"role.heading","default"},{"role.headingGlyph","ansi:cyan"},
    {"role.headingMinorGlyph","ansi:magenta"},
    {"role.link","ansi:cyan"},{"role.listMarker","ansi:magenta"},
    {"role.quote","ansi:blackBright"},
    {"role.syntaxKeyword","ansi:magenta"},{"role.syntaxString","ansi:green"},
    {"role.syntaxNumber","ansi:yellow"},{"role.syntaxComment","ansi:blackBright"},
    {"role.syntaxType","ansi:cyan"},
    {"role.diffAdd","ansi:green"},{"role.diffAddBg","default"},
    {"role.diffDel","ansi:red"},{"role.diffDelBg","default"},
    {"role.diffContext","default"},{"role.diffLineNo","ansi:blackBright"},
    {"role.diffHunk","ansi:blackBright"},
    {"role.dimLabel","ansi:blackBright"},{"role.kbd","default"},
    {"role.border","ansi:blackBright"},{"role.borderFocus","ansi:magenta"},
    {"role.selection","ansi:magenta"},{"role.selectionBg","default"},
    {"role.spinner","ansi:cyan"},
    {"role.statusOk","ansi:green"},{"role.statusWarn","ansi:yellow"},
    {"role.statusErr","ansi:red"},
    {"role.modeDefault","ansi:blackBright"},{"role.modePlan","ansi:cyan"},
    {"role.modeAccept","ansi:magenta"},{"role.modeYolo","ansi:red"},
  };
  auto theme=darkAnsiTheme();
  legacy(theme,roles);
  return theme;
}
} // namespace batbox::interactive::presentation_providers
