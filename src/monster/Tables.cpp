#include "monster/Monster.hpp"
namespace batbox::monster {
const Tables& tables(){
  static const Tables value{
    value::parse(R"monster(["common","uncommon","rare","epic","legendary"])monster"),
    value::parse(R"monster(["bat","gargoyle","ooze","imp","dragon","kraken","cyclops","gremlin","troll","wurm","ghost","aberration","ogre","mandrake","golem","jackalope","myconid","behemoth"])monster"),
    value::parse(R"monster(["·","✦","×","◉","@","°"])monster"),
    value::parse(R"monster(["none","crown","tophat","propeller","halo","wizard","beanie","tinyduck"])monster"),
    value::parse(R"monster(["DEBUGGING","PATIENCE","CHAOS","WISDOM","SNARK"])monster"),
    value::parse(R"monster({"common":60,"uncommon":25,"rare":10,"epic":4,"legendary":1})monster"),
    value::parse(R"monster({"common":5,"uncommon":15,"rare":25,"epic":35,"legendary":50})monster"),
    value::parse(R"monster({"common":"★","uncommon":"★★","rare":"★★★","epic":"★★★★","legendary":"★★★★★"})monster"),
    value::parse(R"monster({"common":"inactive","uncommon":"success","rare":"permission","epic":"autoAccept","legendary":"warning"})monster")
  };
  return value;
}
}
