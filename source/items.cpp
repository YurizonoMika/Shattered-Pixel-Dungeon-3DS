#include "items.h"

// Sprite indices follow Shattered Pixel Dungeon's ItemSpriteSheet groups.
// items.png is a 16x16 grid, so the cell is (index % 16, index / 16).
const ItemDef ITEM_DEFS[IT_COUNT] = {
    {"无", "什么都没有。", K_QUEST, CAT_MISC, 0, 0, -1},
    // --- potions / scrolls / food / bomb ---
    {"治疗药水", "持续恢复生命，总量为最大生命的八成加14。", K_USE, CAT_POTION, 15, 0, 352},
    {"力量药水", "永久增加1点力量。", K_USE, CAT_POTION, 1, 0, 353},
    {"护盾药水", "获得 12 点护盾，优先吸收伤害。", K_USE, CAT_POTION, 12, 0, 355},
    {"探图卷轴", "揭示本层的完整地图。", K_USE, CAT_SCROLL, 0, 0, 304},
    {"强化卷轴", "强化当前武器或护甲。", K_USE, CAT_SCROLL, 0, 0, 305},
    {"传送卷轴", "随机传送到本层某处。", K_USE, CAT_SCROLL, 0, 0, 306},
    {"干粮", "恢复300点饱食度，进食需要3回合。", K_FOOD, CAT_MISC, 5, 0, 437},
    {"肉排", "恢复150点饱食度，进食需要3回合。", K_FOOD, CAT_MISC, 10, 0, 433},
    {"炸弹", "瞄准后投掷，对周围造成伤害。", K_BOMB, CAT_MISC, 9, 0, 80},
    // --- tier-1 gear ---
    {"匕首", "轻巧的近战武器。", K_WEAPON, CAT_GEAR, 3, 0, 100},
    {"短剑", "可靠的近战武器。", K_WEAPON, CAT_GEAR, 5, 0, 104},
    {"木棒", "粗糙但结实的武器。", K_WEAPON, CAT_GEAR, 4, 0, 97},
    {"布甲", "最基础的护甲。", K_ARMOR, CAT_GEAR, 1, 0, 176},
    {"皮甲", "轻便的皮制护甲。", K_ARMOR, CAT_GEAR, 3, 0, 177},
    // --- wands ---
    {"魔法飞弹法杖", "单目标法术，消耗 1 充能。", K_WAND, CAT_WAND, 5, 3, 208},
    {"火焰法杖", "范围火焰法术，消耗 1 充能。", K_WAND, CAT_WAND, 7, 3, 209},
    // --- tier-2/3 weapons, tier-2..5 armor ---
    {"长剑", "监狱铁匠打造的长剑。", K_WEAPON, CAT_GEAR, 7, 0, 120},
    {"战锤", "沉重的战锤，破坏力惊人。", K_WEAPON, CAT_GEAR, 9, 0, 129},
    {"巨斧", "矮人风格的巨斧。", K_WEAPON, CAT_GEAR, 11, 0, 131},
    {"符文剑", "刻满符文的远古之剑。", K_WEAPON, CAT_GEAR, 13, 0, 123},
    {"锁甲", "由铁环编成的护甲。", K_ARMOR, CAT_GEAR, 5, 0, 178},
    {"鳞甲", "层层甲片覆盖的护甲。", K_ARMOR, CAT_GEAR, 7, 0, 179},
    {"板甲", "矮人锻造的重型板甲。", K_ARMOR, CAT_GEAR, 9, 0, 180},
    {"符文甲", "刻有守护符文的铠甲。", K_ARMOR, CAT_GEAR, 11, 0, 181},
    {"寒霜法杖", "冻结目标的法术，消耗 1 充能。", K_WAND, CAT_WAND, 6, 3, 210},
    {"闪电法杖", "范围闪电法术，消耗 1 充能。", K_WAND, CAT_WAND, 9, 3, 211},
    {"铁钥匙", "用来打开这一层的上锁房门。", K_KEY, CAT_MISC, 0, 0, 55},
    {"金币", "闪闪发光的金币。", K_GOLD, CAT_MISC, 0, 0, 18},
    {"Yendor护符", "地牢最深处的宝物，将它带回地表。", K_QUEST, CAT_MISC, 0, 0, 61},
    // --- more weapons ---
    {"手斧", "轻便的单手斧。", K_WEAPON, CAT_GEAR, 8, 0, 105},
    {"长矛", "攻击距离更长的武器。", K_WEAPON, CAT_GEAR, 7, 0, 106},
    {"弯刀", "锋利的弯刀。", K_WEAPON, CAT_GEAR, 10, 0, 114},
    {"长鞭", "难以驾驭但伤害很高。", K_WEAPON, CAT_GEAR, 8, 0, 117},
    {"巨剑", "需要双手挥动的巨剑。", K_WEAPON, CAT_GEAR, 12, 0, 128},
    {"长戟", "沉重而致命的长柄武器。", K_WEAPON, CAT_GEAR, 11, 0, 130},
    {"战斧", "矮人军队的制式战斧。", K_WEAPON, CAT_GEAR, 14, 0, 121},
    {"双手锤", "毁灭性的双手锤。", K_WEAPON, CAT_GEAR, 15, 0, 129},
    // --- more wands ---
    {"瓦解法杖", "直线穿透伤害，消耗 1 充能。", K_WAND, CAT_WAND, 10, 3, 212},
    {"腐蚀法杖", "范围腐蚀伤害，消耗 1 充能。", K_WAND, CAT_WAND, 8, 3, 214},
    {"大地法杖", "缠绕目标并造成伤害。", K_WAND, CAT_WAND, 7, 3, 215},
    {"冲击波法杖", "击退周围敌人。", K_WAND, CAT_WAND, 9, 3, 216},
    {"腐化法杖", "腐蚀心智的范围法术。", K_WAND, CAT_WAND, 8, 3, 217},
    {"守护法杖", "为自己附上护盾。", K_WAND, CAT_WAND, 0, 3, 218},
    {"再生法杖", "恢复自己的生命。", K_WAND, CAT_WAND, 0, 3, 219},
    {"输血法杖", "吸取目标生命。", K_WAND, CAT_WAND, 6, 3, 220},
    {"棱光法杖", "随机元素的强力法术。", K_WAND, CAT_WAND, 11, 3, 213},
    // --- more potions ---
    {"迅捷药水", "20回合内移动速度提高至三倍。", K_USE, CAT_POTION, 20, 0, 354},
    {"隐身药水", "让怪物暂时看不见你。", K_USE, CAT_POTION, 20, 0, 356},
    {"漂浮药水", "短时间内免疫陷阱。", K_USE, CAT_POTION, 20, 0, 357},
    {"心灵药水", "短时间内感知所有敌人。", K_USE, CAT_POTION, 15, 0, 358},
    {"经验药水", "立刻升一级；满级时获得30回合祝福。", K_USE, CAT_POTION, 10, 0, 359},
    {"强力药水", "短时间内提升攻击。", K_USE, CAT_POTION, 20, 0, 360},
    {"剧毒药水", "对周围敌人释放毒气。", K_USE, CAT_POTION, 5, 0, 361},
    {"烈焰药水", "对周围敌人造成火焰伤害。", K_USE, CAT_POTION, 8, 0, 362},
    // --- more scrolls ---
    {"鉴定卷轴", "鉴定背包中的物品。", K_USE, CAT_SCROLL, 0, 0, 307},
    {"祛邪卷轴", "移除装备上的诅咒。", K_USE, CAT_SCROLL, 0, 0, 308},
    {"恐惧卷轴", "让附近敌人逃跑。", K_USE, CAT_SCROLL, 0, 0, 309},
    {"催眠卷轴", "让附近敌人陷入沉睡。", K_USE, CAT_SCROLL, 0, 0, 310},
    {"充能卷轴", "为所有法杖恢复充能。", K_USE, CAT_SCROLL, 0, 0, 311},
    {"狂暴卷轴", "让附近敌人互相攻击。", K_USE, CAT_SCROLL, 0, 0, 312},
    {"挑战卷轴", "召唤更多敌人。", K_USE, CAT_SCROLL, 0, 0, 313},
    {"嬗变卷轴", "随机转化一件物品。", K_USE, CAT_SCROLL, 0, 0, 314},
    // --- rings ---
    {"力量指环", "提升力量。", K_RING, CAT_GEAR, 0, 0, 224},
    {"精准指环", "提升命中。", K_RING, CAT_GEAR, 0, 0, 225},
    {"闪避指环", "提升闪避。", K_RING, CAT_GEAR, 0, 0, 226},
    {"迅捷指环", "提高移动速度。", K_RING, CAT_GEAR, 0, 0, 227},
    {"财富指环", "拾取金币时获得额外金币。", K_RING, CAT_GEAR, 0, 0, 228},
    {"能量指环", "法杖更快恢复充能。", K_RING, CAT_GEAR, 0, 0, 229},
    {"武力指环", "提升近战伤害。", K_RING, CAT_GEAR, 0, 0, 230},
    {"元素指环", "降低陷阱伤害。", K_RING, CAT_GEAR, 0, 0, 231},
    // --- food / bombs / missiles / seeds ---
    {"糕点", "恢复450点饱食度，进食需要3回合。", K_FOOD, CAT_MISC, 8, 0, 438},
    {"火焰炸弹", "范围火焰伤害。", K_BOMB, CAT_MISC, 12, 0, 82},
    {"冰霜炸弹", "范围冻结伤害。", K_BOMB, CAT_MISC, 9, 0, 83},
    {"飞刀", "瞄准投掷的单体武器。", K_MISSILE, CAT_MISC, 5, 0, 146},
    {"飞镖", "轻便的投掷武器。", K_MISSILE, CAT_MISC, 4, 0, 160},
    {"标枪", "沉重的投掷武器。", K_MISSILE, CAT_MISC, 7, 0, 154},
    {"阳草种子", "种下后恢复生命。", K_SEED, CAT_MISC, 10, 0, 387},
    {"火焰花种子", "种下后造成火焰伤害。", K_SEED, CAT_MISC, 8, 0, 385},
    {"致盲草种子", "种下后催眠附近敌人。", K_SEED, CAT_MISC, 10, 0, 395},
    {"安卡", "死亡时自动复活并恢复一半生命。", K_USE, CAT_MISC, 50, 0, 48},
    {"尸尘", "制杖师需要的诅咒骨灰。", K_QUEST, CAT_MISC, 0, 0, 465},
    {"暗金矿石", "铁匠需要的矿石，熔于日光。", K_QUEST, CAT_MISC, 0, 0, 469},
    {"矮人徽记", "僧侣与魔像携带的金属徽记。", K_QUEST, CAT_MISC, 0, 0, 470},
    {"药水带", "增加 5 个药水背包空间。", K_BAG, CAT_GEAR, 0, 0, 484},
    {"卷轴匣", "增加 5 个卷轴背包空间。", K_BAG, CAT_GEAR, 0, 0, 483},
    {"法杖套", "增加 5 个法杖背包空间。", K_BAG, CAT_GEAR, 0, 0, 485},
    {"强化石", "调整武器的速度或伤害，或护甲的闪避与防御。", K_USE, CAT_MISC, 0, 0, 337},
    {"淬毒飞镖", "命中后附加毒素/催眠。", K_MISSILE, CAT_MISC, 6, 0, 167},
    {"镐", "可以砸开路障的沉重工具。", K_WEAPON, CAT_GEAR, 6, 0, 468},
    {"金钥匙", "可以打开这一层的上锁宝箱。", K_KEY, CAT_MISC, 0, 0, 56},
    {"水晶钥匙", "可以打开这一层的水晶宝箱。", K_KEY, CAT_MISC, 0, 0, 57},
    {"破旧短剑", "一阶近战武器。", K_WEAPON, CAT_GEAR, 1, 0, 96},
    {"镶钉手套", "攻击速度快的一阶武器。", K_WEAPON, CAT_GEAR, 1, 0, 98},
    {"细剑", "提供1点额外防御。", K_WEAPON, CAT_GEAR, 1, 0, 99},
    {"铁头棍", "提供2点额外防御。", K_WEAPON, CAT_GEAR, 2, 0, 107},
    {"短刺剑", "伏击伤害更稳定。", K_WEAPON, CAT_GEAR, 2, 0, 108},
    {"镰刀", "伤害高，命中率较低。", K_WEAPON, CAT_GEAR, 2, 0, 109},
    {"剑", "三阶近战武器。", K_WEAPON, CAT_GEAR, 3, 0, 112},
    {"硬头锤", "命中率较高的三阶武器。", K_WEAPON, CAT_GEAR, 3, 0, 113},
    {"圆盾", "提供额外防御的三阶武器。", K_WEAPON, CAT_GEAR, 3, 0, 115},
    {"双钗", "攻击速度快的三阶武器。", K_WEAPON, CAT_GEAR, 3, 0, 116},
    {"链枷", "伤害高，无法伏击。", K_WEAPON, CAT_GEAR, 4, 0, 122},
    {"刺杀之刃", "伏击时造成更稳定的高伤害。", K_WEAPON, CAT_GEAR, 4, 0, 124},
    {"十字弩", "四阶武器，可强化飞镖。", K_WEAPON, CAT_GEAR, 4, 0, 125},
    {"武士刀", "提供3点额外防御。", K_WEAPON, CAT_GEAR, 4, 0, 126},
    {"巨盾", "提供额外防御的五阶武器。", K_WEAPON, CAT_GEAR, 5, 0, 132},
    {"石拳套", "攻击速度快的五阶武器。", K_WEAPON, CAT_GEAR, 5, 0, 133},
    {"战镰", "伤害高，命中率较低。", K_WEAPON, CAT_GEAR, 5, 0, 134},
    {"狂怒指环", "提高攻击速度。", K_RING, CAT_GEAR, 0, 0, 232},
    {"韧性指环", "生命越低，受到的伤害越少。", K_RING, CAT_GEAR, 0, 0, 233},
    {"神射指环", "提升投掷武器等级与耐久。", K_RING, CAT_GEAR, 0, 0, 234},
    {"奥术指环", "增强附魔与刻印的触发概率或强度。", K_RING, CAT_GEAR, 0, 0, 235},
    {"露珠", "每滴恢复最大生命的5%，水袋未满时优先收集。", K_QUEST, CAT_MISC, 0, 0, 19},
    {"水袋", "最多储存20滴露珠。饮用按缺失生命消耗露珠，每滴恢复最大生命的5%。", K_QUEST, CAT_POTION, 0, 0, 480},
    {"炼金工具箱","佩戴后随经验积累炼金能量，消耗能量升级。",K_ARTIFACT,CAT_GEAR,0,0,245},
    {"蓄血圣杯","加速自然恢复；献血升级会造成危险的伤害。",K_ARTIFACT,CAT_GEAR,0,0,253},
    {"暗影斗篷","潜行时消耗充能，随使用成长。",K_ARTIFACT,CAT_GEAR,0,0,240},
    {"干枯玫瑰","充能完毕后召唤幽灵盟友，收集花瓣升级。",K_ARTIFACT,CAT_GEAR,0,0,260},
    {"虚无锁链","消耗充能拉动敌人或自己，穿过障碍。",K_ARTIFACT,CAT_GEAR,0,0,248},
    {"神圣法典","牧师的神圣法术源泉，随施法成长。",K_ARTIFACT,CAT_GEAR,0,0,263},
    {"丰饶之角","随经验储存食物，喂食可升级。",K_ARTIFACT,CAT_GEAR,0,0,249},
    {"神偷袖章","消耗充能偷窃物品，随偷窃成长。",K_ARTIFACT,CAT_GEAR,0,0,241},
    {"自然之履","喂食不同种子成长，踩草积蓄自然能量。",K_ARTIFACT,CAT_GEAR,0,0,256},
    {"万能钥匙","消耗充能打开锁或施放钥匙之墙。",K_ARTIFACT,CAT_GEAR,0,0,264},
    {"先见护符","感知隐藏危险，消耗充能探查目标区域。",K_ARTIFACT,CAT_GEAR,0,0,243},
    {"时光沙漏","消耗充能冻结时间或进入时间静止。",K_ARTIFACT,CAT_GEAR,0,0,244},
    {"无序魔典","消耗充能释放随机卷轴；注入卷轴升级。",K_ARTIFACT,CAT_GEAR,0,0,246},
    {"附魔符石","为选择的武器或护甲赋予随机附魔或刻印。",K_USE,CAT_MISC,0,0,344},
    {"附魔卷轴","为武器或护甲提供三个附魔或刻印选项。",K_USE,CAT_SCROLL,0,0,320},
    {"奥术刻笔","为护甲刻上随机刻印。",K_USE,CAT_MISC,0,0,49},
    {"幽灵花瓣","用于提升干枯玫瑰。",K_USE,CAT_MISC,0,0,20},
    {"沙袋","用于提升时光沙漏。",K_USE,CAT_MISC,0,0,21},
    {"腐莓种子","种植腐莓，激活后释放毒气并留下种子。",K_SEED,CAT_MISC,0,0,384},
    {"速行蓟种子","激活后获得6回合时间气泡。",K_SEED,CAT_MISC,0,0,386},
    {"冰冠花种子","冻结周围的生物并扑灭火焰。",K_SEED,CAT_MISC,0,0,388},
    {"风暴藤种子","令生物陷入眩晕。",K_SEED,CAT_MISC,0,0,389},
    {"断肠苔种子","令生物中毒，毒素随深度增长。",K_SEED,CAT_MISC,0,0,390},
    {"梦皇花种子","清除多种负面状态。",K_SEED,CAT_MISC,0,0,391},
    {"地缚根种子","站定时获得能吸收伤害的草药护甲。",K_SEED,CAT_MISC,0,0,392},
    {"星花种子","使目标获得祝福。",K_SEED,CAT_MISC,0,0,393},
    {"衰落叶种子","将生物传送至本层另一处。",K_SEED,CAT_MISC,0,0,394},
    {"投掷尖刺","一阶投掷武器，可回收。",K_MISSILE,CAT_MISC,1,0,145},
    {"投石","一阶投掷武器，不会留在敌人体内。",K_MISSILE,CAT_MISC,1,0,147},
    {"捕鱼矛","二阶投掷武器。",K_MISSILE,CAT_MISC,2,0,148},
    {"手里剑","二阶投掷武器。",K_MISSILE,CAT_MISC,2,0,149},
    {"投掷棒","二阶投掷武器，可立即拾取。",K_MISSILE,CAT_MISC,2,0,150},
    {"投矛","三阶投掷武器。",K_MISSILE,CAT_MISC,3,0,151},
    {"流星索","三阶投掷武器，命中使敌人残废。",K_MISSILE,CAT_MISC,3,0,152},
    {"苦无","三阶投掷武器，伏击伤害更稳定。",K_MISSILE,CAT_MISC,3,0,153},
    {"飞斧","四阶投掷武器，使目标流血。",K_MISSILE,CAT_MISC,4,0,155},
    {"重型回旋镖","四阶投掷武器，五回合后返回出手位置。",K_MISSILE,CAT_MISC,4,0,156},
    {"三叉戟","五阶投掷武器。",K_MISSILE,CAT_MISC,5,0,157},
    {"飞锤","五阶投掷武器，可立即拾取。",K_MISSILE,CAT_MISC,5,0,158},
    {"原力魔方","五阶投掷武器，影响目标周围的生物。",K_MISSILE,CAT_MISC,5,0,159},
    {"占卜卷轴","揭示四种尚未识别的药水、卷轴或戒指类型。",K_USE,CAT_SCROLL,0,0,321},
    {"驱魔卷轴","暂时使魔法失效。",K_USE,CAT_SCROLL,0,0,322},
    {"虹卫卷轴","建立一个保护你的虹色守卫。",K_USE,CAT_SCROLL,0,0,323},
    {"神秘能量卷轴","在30回合内持续为神器充能。",K_USE,CAT_SCROLL,0,0,324},
    {"归返卷轴","返回上方最近的区域入口。",K_USE,CAT_SCROLL,0,0,325},
    {"塞壬之歌卷轴","永久魅惑一个目标，并暂时魅惑视野内的其他敌人。",K_USE,CAT_SCROLL,0,0,326},
    {"预知卷轴","在较长时间内自动发现附近的隐藏事物。",K_USE,CAT_SCROLL,0,0,327},
    {"恐惧卷轴","使可见敌人逃离楼层。",K_USE,CAT_SCROLL,0,0,330},
    {"灵爆卷轴","对可见敌人造成极大伤害；目标少时反噬更强。",K_USE,CAT_SCROLL,0,0,329},
    {"蜕变卷轴","替换已投入点数的天赋。",K_USE,CAT_SCROLL,0,0,331},
    {"决斗卷轴","建立持续100回合的决斗区域。",K_USE,CAT_SCROLL,0,0,328},
    {"镜像卷轴","召唤与你并肩战斗的镜像。",K_USE,CAT_SCROLL,0,0,307},
    {"复仇卷轴","根据受伤程度伤害并致盲可见敌人。",K_USE,CAT_SCROLL,0,0,313},
};

const char* item_name(uint8_t id) { return item_def(id).name; }

bool item_random_identity(uint8_t id) {
  if(item_is_exotic(id)) return false;
  const auto& def=item_def(id);
  return def.kind==K_RING || (def.kind==K_USE && (def.cat==CAT_POTION || def.cat==CAT_SCROLL));
}
bool item_is_exotic(uint8_t id) {
  return id==IT_SCROLL_ENCHANT || (id>=IT_SCROLL_DIVINATION && id<=IT_SCROLL_ARENA);
}

static bool is_scroll(uint8_t id) {
  return item_def(id).cat==CAT_SCROLL;
}

int item_value(uint8_t id) {
  const ItemDef& d = item_def(id);
  switch (d.kind) {
    case K_USE: return is_scroll(id) ? 40 : 30;
    case K_FOOD: return 20;
    case K_BOMB: return 30;
    case K_KEY: return 10;
    case K_QUEST: return 0;
    case K_WEAPON: return 40 + d.power * 10;
    case K_ARMOR: return 35 + d.power * 10;
    case K_WAND: return 80 + d.power * 10;
    case K_RING: case K_ARTIFACT: return 120;
    case K_MISSILE: return 15 + d.power * 5;
    case K_BAG: return 100;
    case K_SEED: return 25;
    default: return 0;
  }
}

void item_sprite_cell(uint8_t id, int& cx, int& cy) {
  int idx = item_def(id).sprite;
  if (idx < 0) { cx = 0; cy = 0; return; }
  cx = (idx % 16) * 16;
  cy = (idx / 16) * 16;
}

bool item_is_equipment(uint8_t id) {
  uint8_t k = item_def(id).kind;
  return k == K_WEAPON || k == K_ARMOR || k == K_RING || k==K_ARTIFACT;
}

bool item_is_stackable(uint8_t id) {
  uint8_t k = item_def(id).kind;
  return k == K_USE || k == K_FOOD || k == K_BOMB || k == K_KEY ||
         k == K_GOLD || k == K_MISSILE || k == K_SEED;
}
