#include "input.h"
#include "constants.h"
#include "globals.h"
#include "path.h"
#include "logic.h"
#include "tutorial.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>

#if defined(LGD_DEBUG_WAVE_JUMP)
static void DebugJumpToWave(Game& G, int targetWave) {
    targetWave = std::max(1, targetWave);
    G.Reset();
    G.wave = targetWave - 1;
    G.phase = Game::BUILD;
    G.credits = 5000;
    G.lives = 50;
    G.cpuHp = 100.f;
    StartWave(G);

    char msg[128];
    snprintf(msg, 128, "DEBUG Wave %d：%d lanes / %d enemies",
        G.wave, G.ActiveLaneCount(), G.waveCount);
    G.SetMsg(msg);
}
#endif

// ══════════════════════════════════════════════════════════════════
//  HandleInput  —  鍵盤 + 滑鼠輸入處理
// ══════════════════════════════════════════════════════════════════
void HandleInput(Game& G) {
    Vector2 mp  = VirtualMouse();
    int     hgx = -1, hgy = -1;
    bool    onMap = ScreenToGrid(G, mp, hgx, hgy);

#if defined(LGD_DEBUG_WAVE_JUMP)
    if (IsKeyPressed(KEY_F7))  { DebugJumpToWave(G, 7);  return; }
    if (IsKeyPressed(KEY_F8))  { DebugJumpToWave(G, 10); return; }
    if (IsKeyPressed(KEY_F9))  { DebugJumpToWave(G, 16); return; }
    if (IsKeyPressed(KEY_F10)) { DebugJumpToWave(G, 30); return; }
#endif

    // ── 主選單輸入 ───────────────────────────────────────────────
    if (G.phase == Game::MENU) {
        if (G.showHelp) {
            if (IsKeyPressed(KEY_H)         ||
                IsKeyPressed(KEY_ESCAPE)    ||
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                G.showHelp = false;
            }
            return;
        }
        int cx = VIRT_W / 2, cy = VIRT_H / 2;
        int frameW = MENU_FRAME_W;
        int fx = cx - frameW / 2;
        int fy = cy - MENU_FRAME_OFFSET_Y;
        int actionY = fy + MENU_ACTION_Y_OFFSET;
        int actionH = MENU_ACTION_H;
        int actionGap = MENU_ACTION_GAP;
        int actionW = (frameW - MENU_ACTION_INSET * 2 - actionGap) / 2;
        Rectangle startBtn = {(float)fx + (float)MENU_ACTION_INSET, (float)actionY, (float)actionW, (float)actionH};
        Rectangle tutorialBtn = {startBtn.x + actionW + actionGap, (float)actionY, (float)actionW, (float)actionH};

        if (IsKeyPressed(KEY_H))                                G.showHelp = true;
        if (IsKeyPressed(KEY_T))                                EnterTutorialSelect(G);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) G.Reset();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mp, startBtn)) {
                G.Reset();
            } else if (CheckCollisionPointRec(mp, tutorialBtn)) {
                EnterTutorialSelect(G);
            }
        }
        return;
    }

    // ── 教學章節選單輸入 ───────────────────────────────────────
    if (G.phase == Game::TUTORIAL_SELECT) {
        if (G.showHelp) {
            if (IsKeyPressed(KEY_H) ||
                IsKeyPressed(KEY_ESCAPE) ||
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                G.showHelp = false;
            }
            return;
        }

        if (IsKeyPressed(KEY_H)) {
            G.showHelp = true;
            return;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            G.phase = Game::MENU;
            G.tutorial.ResetTransient();
            return;
        }
        if (IsKeyPressed(KEY_UP)) {
            G.tutorial.selectedLesson = ClampTutorialLessonIndex(G.tutorial.selectedLesson - TUTORIAL_SELECT_COLS);
            return;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            G.tutorial.selectedLesson = ClampTutorialLessonIndex(G.tutorial.selectedLesson + TUTORIAL_SELECT_COLS);
            return;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            G.tutorial.selectedLesson = ClampTutorialLessonIndex(G.tutorial.selectedLesson - 1);
            return;
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            G.tutorial.selectedLesson = ClampTutorialLessonIndex(G.tutorial.selectedLesson + 1);
            return;
        }

        int picked = -1;
        static const int NUM_KEYS[TUTORIAL_LESSON_COUNT] = {
            KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE,
            KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE
        };
        static const int KP_KEYS[TUTORIAL_LESSON_COUNT] = {
            KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4, KEY_KP_5,
            KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9
        };
        for (int i = 0; i < TUTORIAL_LESSON_COUNT; i++) {
            if (IsKeyPressed(NUM_KEYS[i]) || IsKeyPressed(KP_KEYS[i])) {
                picked = i;
                break;
            }
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            for (int i = 0; i < TUTORIAL_LESSON_COUNT; i++) {
                Rectangle card = TutorialLessonCardRect(i);
                if (CheckCollisionPointRec(mp, card)) {
                    picked = i;
                    break;
                }
            }
        }

        if (picked >= 0) {
            G.tutorial.selectedLesson = ClampTutorialLessonIndex(picked);
            StartTutorial(G, (TutorialLessonId)G.tutorial.selectedLesson);
            return;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            StartTutorial(G, (TutorialLessonId)G.tutorial.selectedLesson);
            return;
        }
        return;
    }

    // ── 教學進行中輸入優先權 ───────────────────────────────────
    if (HandleTutorialInput(G)) return;

    // ── 說明頁面開關 ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_H)) {
        G.showHelp = !G.showHelp;
        if (G.showHelp && G.phase == Game::FIGHT) G.paused = true;
        else if (!G.showHelp)                     G.paused = false;
        return;
    }
    if (G.showHelp) {
        if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            G.showHelp = false;
            G.paused   = false;
        }
        return;
    }

    // ── 全域快捷鍵 ───────────────────────────────────────────────
    if (IsKeyPressed(KEY_P) && G.phase != Game::GAMEOVER) {
        G.paused = !G.paused;
        return;
    }
    if (IsKeyPressed(KEY_R)) {
        G.Reset();
        return;
    }
    if (IsKeyPressed(KEY_T)) {
        G.showThreat = !G.showThreat;
        G.SetMsg(G.showThreat ? "熱圖：開啟" : "熱圖：關閉");
        return;
    }
    if (IsKeyPressed(KEY_A)) {
        G.showAIHints = !G.showAIHints;
        G.SetMsg(G.showAIHints ? "AI 提示：開啟" : "AI 提示：關閉");
        return;
    }
    if (G.phase == Game::GAMEOVER) {
        if (IsKeyPressed(KEY_ENTER)) G.phase = Game::MENU;
        return;
    }
    if (G.paused) return;

    if (G.phase == Game::TRAINING) {
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) { ApplyTrainingChoice(G, 0); return; }
        if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) { ApplyTrainingChoice(G, 1); return; }
        if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) { ApplyTrainingChoice(G, 2); return; }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            mp.x >= LEFT_CTRL_X && mp.x < LEFT_CTRL_X + LEFT_CTRL_W) {
            int startY = TOPBAR_H + 76;
            for (int i = 0; i < G.trainingChoiceCount; i++) {
                int by = startY + i * (TRAIN_CARD_H + TRAIN_CARD_GAP);
                if (mp.y >= by && mp.y < by + TRAIN_CARD_H) {
                    ApplyTrainingChoice(G, i);
                    return;
                }
            }
        }
        return;
    }

    // ── 技能觸發 [Q] ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_Q) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (st && HasTowerSkillSlot(st->type)) {
            if (st->activeCd > 0.f) {
                char b2[48];
                snprintf(b2, 48, "技能冷卻中...%.0f 秒", st->activeCd);
                G.SetMsg(b2);
            } else {
                ActivateSkill(G, *st);
                RecordTutorialSkillUsed(G, st->type);
                int ti = (int)st->type;
                char b2[48];
                snprintf(b2, 48, "[%s] %s！", TDef(st->type).sym, SKILL_NAME[ti]);
                G.SetMsg(b2);
            }
        }
    }

    // ── 升級 [U] ─────────────────────────────────────────────────
    if (IsKeyPressed(KEY_U) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (st) G.UpgradeTower(*st);
    }

    // ── 左側面板按鈕（滑鼠點擊）────────────────────────────────
    static TType ORDER[] = {
        TType::SENSOR, TType::PERCEPTRON, TType::AND, TType::OR,
        TType::XOR,    TType::NAND,       TType::CANNON
    };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        mp.x >= LEFT_CTRL_X && mp.x < LEFT_CTRL_X + LEFT_CTRL_W) {
        for (int i = 0; i < 7; i++) {
            int by = G.btnY0 + i * (LEFT_TOWER_BTN_H + LEFT_TOWER_BTN_GAP);
            if (mp.y >= by && mp.y < by + LEFT_TOWER_BTN_H) {
                TType tt  = ORDER[i];
                if (G.tutorial.active && !IsTutorialTowerAllowed(G, tt)) {
                    char msg[128];
                    snprintf(msg, 128, "教學提示：目前步驟是「%s」。%s",
                             TutorialCurrentStepTitle(G), TutorialCurrentStepHint(G));
                    G.SetMsg(msg);
                    return;
                }
                G.placing = (G.placing == tt) ? TType::NONE : tt;
                G.selectedId = -1;
                G.connectSrc = -1;
                return;
            }
        }
        if (mp.y >= G.waveBtnY && mp.y < G.waveBtnY + LEFT_WAVE_BTN_H &&
            G.phase == Game::BUILD) {
            if (G.tutorial.active && !IsTutorialWaveStartAllowed(G)) {
                G.SetMsg("教學提示：請先完成目前步驟，再發動短波次。");
                return;
            }
            StartWave(G);
            return;
        }
    }

    // ── 地圖點擊（放塔 / 連線 / 選取）──────────────────────────
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && onMap) {

        // 連線模式：點擊目標
        if (G.connectSrc >= 0) {
            Tower* dst = G.TowerAt(hgx, hgy);
            if (dst && dst->id != G.connectSrc) {
                Tower* src = G.FindTower(G.connectSrc);
                if (src) {
                    auto it = std::find(src->conns.begin(), src->conns.end(), dst->id);
                    if (it != src->conns.end()) {
                        src->conns.erase(it);
                        G.SetMsg("連線已取消！");
                    } else {
                        std::string err = G.ValidateConnect(G.connectSrc, dst->id);
                        if (err.empty()) {
                            src->conns.push_back(dst->id);
                            RecordTutorialConnection(G, src->type, dst->type);
                            G.SetMsg("連接成功！");
                        } else {
                            G.SetMsg("[錯誤] " + err);
                        }
                    }
                }
            }
            G.connectSrc = -1;
            return;
        }

        // 點到已有塔 → 選取
        Tower* ex = G.TowerAt(hgx, hgy);
        if (ex) {
            G.selectedId = ex->id;
            G.placing    = TType::NONE;
            RecordTutorialTowerSelected(G, ex->type);
            return;
        }

        // 放置新塔
        if (G.placing != TType::NONE) {
            if (!IsBuildableTowerType(G.placing)) {
                G.placing = TType::NONE;
                G.SetMsg("[錯誤] 無效元件類型");
            } else if (G.IsPath(hgx, hgy)) {
                G.SetMsg("[錯誤] 不能放在路徑上！");
            } else if (G.credits >= TDef(G.placing).baseCost) {
                Tower nt;
                nt.id       = G.nextId++;
                nt.type     = G.placing;
                nt.gx       = hgx;
                nt.gy       = hgy;
                nt.level    = 1;
                nt.sig      = 0.f;
                nt.w1       = 0.8f;
                nt.w2       = 0.6f;
                nt.bias     = -0.5f;
                nt.cooldown = 0;
                nt.anim     = 0.f;
                G.ApplyTowerStats(nt);
                TType placedType = nt.type;
                G.towers.push_back(nt);
                G.credits -= TDef(G.placing).baseCost;
                RecordTutorialTowerPlaced(G, placedType);

                if (G.placing == TType::PERCEPTRON) {
                    G.SetMsg("感知器已放置，每波結束會自動學習！");
                } else {
                    char b2[48];
                    snprintf(b2, 48, "已放置 %s", TDef(G.placing).label);
                    G.SetMsg(b2);
                }
            } else {
                G.SetMsg("金幣不足！");
            }
            return;
        }

        // 點到空地（且無選取模式）→ 取消選取
        G.selectedId = -1;
    }

    // ── 其他快捷鍵 ───────────────────────────────────────────────
    if (IsKeyPressed(KEY_SPACE) && G.phase == Game::BUILD) {
        if (G.tutorial.active && !IsTutorialWaveStartAllowed(G)) {
            G.SetMsg("教學提示：請先完成目前步驟，再發動短波次。");
        } else {
            StartWave(G);
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        G.placing    = TType::NONE;
        G.connectSrc = -1;
    }

    // ── 連線模式 [C] ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_C) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (!st) {
            // 找不到塔，忽略
        } else if (st->type == TType::CANNON) {
            G.SetMsg("[錯誤] 砲塔是終端節點，不能輸出");
        } else if (st->type == TType::CPU) {
            G.SetMsg("[錯誤] CPU不能連接");
        } else if ((int)st->conns.size() >= Game::MAX_CONNS) {
            G.SetMsg("[錯誤] 已達8條上限");
        } else {
            G.connectSrc = G.selectedId;
            G.SetMsg("點擊目標建立連線 | 點已連接目標取消 | [ESC]取消");
        }
    }

    // ── 移除塔 [DEL] ─────────────────────────────────────────────
    if (IsKeyPressed(KEY_DELETE) && G.selectedId >= 0) {
        Tower* st = G.FindTower(G.selectedId);
        if (st && st->type != TType::CPU) {
            int rid    = G.selectedId;
            int refund = TDef(st->type).baseCost / 2;
            G.credits += refund;

            G.towers.erase(
                std::remove_if(G.towers.begin(), G.towers.end(),
                    [rid](const Tower& t) { return t.id == rid; }),
                G.towers.end()
            );
            for (auto& t : G.towers) {
                t.conns.erase(
                    std::remove(t.conns.begin(), t.conns.end(), rid),
                    t.conns.end()
                );
            }

            G.selectedId = -1;
            char b3[48];
            snprintf(b3, 48, "已移除，退款+%d CR", refund);
            G.SetMsg(b3);
        }
    }
}
