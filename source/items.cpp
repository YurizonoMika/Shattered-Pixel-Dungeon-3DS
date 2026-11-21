#include "items.h"

// Sprite indices follow Shattered Pixel Dungeon's ItemSpriteSheet groups.
// items.png is a 16x16 grid, so the cell is (index % 16, index / 16).
const ItemDef ITEM_DEFS[IT_COUNT] = {
    {"无", "什么都没有。", K_QUEST, CAT_MISC, 0, 0, -1},
    // --- potions / scrolls / food / bomb ---
    {"治疗药水", "恢复 15 点生命。", K_USE, CAT_POTION, 15, 0, 352},
    {"力量药水", "永久增加 2 点最大生命。", K_USE, CAT_POTION, 1, 0, 353},
    {"护盾药水", "获得 12 点护盾，优先吸收伤害。", K_USE, CAT_POTION, 12, 0, 355},
    {"探图卷轴", "揭示本层的完整地图。", K_USE, CAT_SCROLL, 0, 0, 304},
    {"强化卷轴", "强化当前武器或护甲。", K_USE, CAT_SCROLL, 0, 0, 305},
    {"传送卷轴", "随机传送到本层某处。", K_USE, CAT_SCROLL, 0, 0, 306},
    {"干粮", "恢复 5 点生命的干粮。", K_FOOD, CAT_MISC, 5, 0, 432},
    {"肉排", "恢复 10 点生命的肉排。", K_FOOD, CAT_MISC, 10, 0, 437},
    {"炸弹", "瞄准后投掷，对周围造成伤害。", K_BOMB, CAT_MISC, 9, 0, 80},
    // --- tier-1 gear ---
    {"匕首", "轻巧的近战武器。", K_WEAPON, CAT_GEAR, 3, 0, 100},
    {"短剑", "可靠的近战武器。", K_WEAPON, CAT_GEAR, 5, 0, 96},
    {"木棒", "粗糙但结实的武器。", K_WEAPON, CAT_GEAR, 4, 0, 97},
    {"布甲", "最基础的护甲。", K_ARMOR, CAT_GEAR, 1, 0, 176},
    {"皮甲", "轻便的皮制护甲。", K_ARMOR, CAT_GEAR, 3, 0, 177},
    // --- wands ---
    {"魔法飞弹法杖", "单目标法术，消耗 1 充能。", K_WAND, CAT_WAND, 5, 3, 208},
    {"火焰法杖", "范围火焰法术，消耗 1 充能。", K_WAND, CAT_WAND, 7, 3, 209},
    // --- tier-2/3 weapons, tier-2..5 armor ---
    {"长剑", "监狱铁匠打造的长剑。", K_WEAPON, CAT_GEAR, 7, 0, 104},
    {"战锤", "沉重的战锤，破坏力惊人。", K_WEAPON, CAT_GEAR, 9, 0, 112},
    {"巨斧", "矮人风格的巨斧。", K_WEAPON, CAT_GEAR, 11, 0, 120},
    {"符文剑", "刻满符文的远古之剑。", K_WEAPON, CAT_GEAR, 13, 0, 128},
    {"锁甲", "由铁环编成的护甲。", K_ARMOR, CAT_GEAR, 5, 0, 178},
    {"鳞甲", "层层甲片覆盖的护甲。", K_ARMOR, CAT_GEAR, 7, 0, 179},
    {"板甲", "矮人锻造的重型板甲。", K_ARMOR, CAT_GEAR, 9, 0, 180},
    {"符文甲", "刻有守护符文的铠甲。", K_ARMOR, CAT_GEAR, 11, 0, 181},
    {"寒霜法杖", "冻结目标的法术，消耗 1 充能。", K_WAND, CAT_WAND, 6, 3, 210},
    {"闪电法杖", "范围闪电法术，消耗 1 充能。", K_WAND, CAT_WAND, 9, 3, 211},
    {"钥匙", "用来打开上锁的门。", K_KEY, CAT_MISC, 0, 0, 55},
    {"金币", "闪闪发光的金币。", K_GOLD, CAT_MISC, 0, 0, 18},
    {"幽魂护符", "幽魂遗失的护符，任务物品。", K_QUEST, CAT_MISC, 0, 0, 61},
    // --- more weapons ---
    {"手斧", "轻便的单手斧。", K_WEAPON, CAT_GEAR, 8, 0, 105},
    {"长矛", "攻击距离更长的武器。", K_WEAPON, CAT_GEAR, 7, 0, 106},
    {"弯刀", "锋利的弯刀。", K_WEAPON, CAT_GEAR, 10, 0, 114},
    {"长鞭", "难以驾驭但伤害很高。", K_WEAPON, CAT_GEAR, 8, 0, 117},
    {"巨剑", "需要双手挥动的巨剑。", K_WEAPON, CAT_GEAR, 12, 0, 121},
    {"长戟", "沉重而致命的长柄武器。", K_WEAPON, CAT_GEAR, 11, 0, 122},
    {"战斧", "矮人军队的制式战斧。", K_WEAPON, CAT_GEAR, 14, 0, 129},
    {"双手锤", "毁灭性的双手锤。", K_WEAPON, CAT_GEAR, 15, 0, 130},
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
    {"迅捷药水", "短时间内加速行动。", K_USE, CAT_POTION, 25, 0, 354},
    {"隐身药水", "让怪物暂时看不见你。", K_USE, CAT_POTION, 20, 0, 356},
    {"漂浮药水", "短时间内免疫陷阱。", K_USE, CAT_POTION, 20, 0, 357},
    {"心灵药水", "短时间内感知所有敌人。", K_USE, CAT_POTION, 15, 0, 358},
    {"经验药水", "立刻获得经验。", K_USE, CAT_POTION, 10, 0, 359},
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
    {"力量指环", "提升攻击力。", K_RING, CAT_GEAR, 0, 0, 224},
    {"精准指环", "提升命中与伤害。", K_RING, CAT_GEAR, 0, 0, 225},
    {"闪避指环", "提升防御。", K_RING, CAT_GEAR, 0, 0, 226},
    {"迅捷指环", "偶尔获得额外行动。", K_RING, CAT_GEAR, 0, 0, 227},
    {"财富指环", "拾取金币时获得额外金币。", K_RING, CAT_GEAR, 0, 0, 228},
    {"能量指环", "法杖更快恢复充能。", K_RING, CAT_GEAR, 0, 0, 229},
    {"守护指环", "定期获得护盾。", K_RING, CAT_GEAR, 0, 0, 230},
    {"元素指环", "降低陷阱伤害。", K_RING, CAT_GEAR, 0, 0, 231},
    // --- food / bombs / missiles / seeds ---
    {"糕点", "恢复 8 点生命。", K_FOOD, CAT_MISC, 8, 0, 433},
    {"火焰炸弹", "范围火焰伤害。", K_BOMB, CAT_MISC, 12, 0, 81},
    {"冰霜炸弹", "范围冻结伤害。", K_BOMB, CAT_MISC, 9, 0, 82},
    {"飞刀", "瞄准投掷的单体武器。", K_MISSILE, CAT_MISC, 5, 0, 146},
    {"飞镖", "轻便的投掷武器。", K_MISSILE, CAT_MISC, 4, 0, 160},
    {"标枪", "沉重的投掷武器。", K_MISSILE, CAT_MISC, 7, 0, 154},
    {"阳草种子", "种下后恢复生命。", K_SEED, CAT_MISC, 10, 0, 387},
    {"火焰花种子", "种下后造成火焰伤害。", K_SEED, CAT_MISC, 8, 0, 385},
    {"致盲草种子", "种下后催眠附近敌人。", K_SEED, CAT_MISC, 10, 0, 390},
    {"安卡", "死亡时自动复活并恢复一半生命。", K_USE, CAT_MISC, 50, 0, 48},
    {"尸尘", "制杖师需要的诅咒骨灰。", K_QUEST, CAT_MISC, 0, 0, 61},
    {"暗金矿石", "铁匠需要的矿石，熔于日光。", K_QUEST, CAT_MISC, 0, 0, 18},
    {"矮人徽记", "僧侣与魔像携带的金属徽记。", K_QUEST, CAT_MISC, 0, 0, 61},
    {"药水带", "增加 5 个药水背包空间。", K_BAG, CAT_GEAR, 0, 0, 48},
    {"卷轴匣", "增加 5 个卷轴背包空间。", K_BAG, CAT_GEAR, 0, 0, 49},
    {"法杖套", "增加 5 个法杖背包空间。", K_BAG, CAT_GEAR, 0, 0, 50},
    {"强化石", "强化当前武器或护甲。", K_USE, CAT_MISC, 0, 0, 51},
    {"淬毒飞镖", "命中后附加毒素/催眠。", K_MISSILE, CAT_MISC, 6, 0, 162},
    {"镐", "可以砸开路障的沉重工具。", K_WEAPON, CAT_GEAR, 6, 0, 101},
};

const char* item_name(uint8_t id) { return item_def(id).name; }

static bool is_scroll(uint8_t id) {
  return (id >= IT_SCROLL_MAP && id <= IT_SCROLL_TELE) ||
         (id >= IT_SCROLL_IDENTIFY && id <= IT_SCROLL_TRANSMUTE);
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
    case K_RING: return 120;
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
  return k == K_WEAPON || k == K_ARMOR || k == K_RING;
}

bool item_is_stackable(uint8_t id) {
  uint8_t k = item_def(id).kind;
  return k == K_USE || k == K_FOOD || k == K_BOMB || k == K_KEY ||
         k == K_GOLD || k == K_MISSILE || k == K_SEED;
}
