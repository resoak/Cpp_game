#pragma once
// ================================================================
//  logic.h — 訊號傳播、戰鬥計算、敵人生成、技能、主更新
// ================================================================
#include "game.h"

// ── 主動技能常數表（draw_ui.cpp 也需要讀這些）────────────────────
extern const float       SKILL_CD[];
extern const char* const SKILL_NAME[];
extern const char* const SKILL_DESC[];

// ── 函式宣告 ─────────────────────────────────────────────────────
int   CalcPCTLayer(Game& G, int towerId, int depth = 0);
void  ActivateSkill(Game& G, Tower& t);

void  PropagateSignals(Game& G, float dt);
float CalcDamageMultiplier(Game& G, Tower& cannon, Enemy& target);
void  UpdateBossAI(Game& G, Enemy& boss, float dt);

void  SpawnEnemy(Game& G);
void  StartWave(Game& G);
void  ApplyTrainingChoice(Game& G, int choiceIdx);
void  Update(Game& G, float dt);
