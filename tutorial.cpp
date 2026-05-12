// ================================================================
//  tutorial.cpp — 教學關卡選單與 Phase 1 runtime 骨架
// ================================================================
#include "tutorial.h"
#include "draw.h"
#include <algorithm>
#include <cstdio>

static const TutorialLessonInfo LESSONS[TUTORIAL_LESSON_COUNT] = {
    { TutorialLessonId::BASIC_DEFENSE, "基礎防線", "SENSOR + CANNON", "BASIC", "2 分鐘",
      "放置感測器與砲塔，建立第一條訊號鏈。", COL_SENSOR },
    { TutorialLessonId::SENSOR, "感測器", "SENSOR", "FAST / STEALTH", "2 分鐘",
      "理解偵測範圍、前段預警與隱形敵人。", COL_SENSOR },
    { TutorialLessonId::CANNON, "砲塔", "CANNON", "BASIC / BOSS", "2 分鐘",
      "理解射程、訊號門檻與終端輸出。", COL_CANNON },
    { TutorialLessonId::AND_GATE, "AND 閘", "AND", "ARMORED", "3 分鐘",
      "用全輸入成立的訊號強化對裝甲傷害。", COL_AND },
    { TutorialLessonId::OR_GATE, "OR 閘", "OR", "SWARM", "3 分鐘",
      "用任一輸入成立的訊號穩定處理雜兵。", COL_OR },
    { TutorialLessonId::XOR_GATE, "XOR 閘", "XOR", "ELITE", "3 分鐘",
      "用單一路徑訊號對精英敵人打出高傷害。", COL_XOR },
    { TutorialLessonId::NAND_GATE, "NAND 閘", "NAND", "ARMORED / SIEGE", "3 分鐘",
      "理解破甲與後期重甲反制。", COL_NAND },
    { TutorialLessonId::PERCEPTRON, "感知器", "PERCEPTRON", "MIXED", "3 分鐘",
      "觀察連續訊號、權重與跨波學習概念。", COL_PERC },
    { TutorialLessonId::MULTI_LANE, "多線防守", "LANE CONTROL", "MULTI-LANE", "3 分鐘",
      "觀察路線預告與 lane 壓力，分區補強火力。", COL_AI },
};

static TutorialObjective Obj(TutorialObjectiveKind kind,
                             TType tower = TType::NONE,
                             EType enemy = EType::BASIC,
                             int count = 1,
                             float seconds = 0.f) {
    TutorialObjective obj{};
    obj.kind = kind;
    obj.towerType = tower;
    obj.enemyType = enemy;
    obj.requiredCount = count;
    obj.requiredSeconds = seconds;
    return obj;
}

static const TutorialStep BASIC_STEPS[] = {
    {"建立第一條防線", "教學會一步步帶你完成最小可運作防線。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置感測器", "在路線旁放置 SENSOR，讓防線能偵測敵人。", "點左側感測器，再點地圖空格。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::SENSOR)},
    {"放置砲塔", "砲塔收到訊號後才會攻擊。", "點左側砲塔，放在感測器附近。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::CANNON)},
    {"建立連線", "把 SENSOR 的訊號接到 CANNON。", "選取感測器，按 C，再點砲塔。", Obj(TutorialObjectiveKind::CONNECT_TOWERS, TType::SENSOR)},
    {"啟動短波次", "現在可以發動波次，觀察訊號驅動砲塔。", "按空白鍵或左側發動按鈕。", Obj(TutorialObjectiveKind::START_WAVE)},
    {"完成", "基礎防線流程已完成。", "按 ESC 可回到章節選單。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
};

static const TutorialStep SENSOR_STEPS[] = {
    {"感測器教學", "感測器負責把敵人距離轉成訊號。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置 SENSOR", "把感測器放在路線前段，能更早觸發火力。", "點左側感測器，再點地圖。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::SENSOR)},
    {"啟動觀察", "Phase 4 會加入指定快速/隱形敵人。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep CANNON_STEPS[] = {
    {"砲塔教學", "砲塔是終端輸出，必須收到訊號才會開火。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置 CANNON", "砲塔要覆蓋路線，但不能放在路徑上。", "點左側砲塔，再點地圖。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::CANNON)},
    {"啟動觀察", "Phase 4 會加入射程與訊號門檻情境。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep AND_STEPS[] = {
    {"AND 閘教學", "AND 適合強化對裝甲敵人的傷害。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置 AND", "AND 需要輸入訊號，後續會接到砲塔。", "點左側 AND 閘並放置。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::AND)},
    {"啟動觀察", "Phase 4 會加入裝甲敵人驗證。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep OR_STEPS[] = {
    {"OR 閘教學", "OR 任一輸入成立就輸出，適合處理雜兵壓力。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置 OR", "OR 可整合多個感測器訊號。", "點左側 OR 閘並放置。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::OR)},
    {"啟動觀察", "Phase 4 會加入蜂群情境。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep XOR_STEPS[] = {
    {"XOR 閘教學", "XOR 對精英敵人有高傷害價值。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置 XOR", "XOR 的單一有效輸入最容易理解。", "點左側 XOR 閘並放置。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::XOR)},
    {"啟動觀察", "Phase 4 會加入精英敵人驗證。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep NAND_STEPS[] = {
    {"NAND 閘教學", "NAND 是後期破甲與重甲反制工具。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置 NAND", "NAND 可降低護甲壓力。", "點左側 NAND 並放置。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::NAND)},
    {"啟動觀察", "Phase 4 會加入高護甲敵人驗證。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep PCT_STEPS[] = {
    {"感知器教學", "感知器接收多個輸入，輸出連續訊號。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置 PCT", "感知器會在後續章節接上學習目標。", "點左側感知器並放置。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::PERCEPTRON)},
    {"啟動觀察", "Phase 4 會加入 mixed 敵人情境。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep MULTI_STEPS[] = {
    {"多線防守教學", "多線壓力需要分區補強，而不是只守單一路線。", "先看完這段提示。", Obj(TutorialObjectiveKind::WAIT_SECONDS, TType::NONE, EType::BASIC, 1, 1.0f), true},
    {"放置任一防禦元件", "Phase 4 會啟用多 lane 腳本。", "先放置一個感測器。", Obj(TutorialObjectiveKind::PLACE_TOWER, TType::SENSOR)},
    {"啟動觀察", "觀察上方與右側 lane 資訊。", "先啟動一次短波次。", Obj(TutorialObjectiveKind::START_WAVE)},
};

static const TutorialStep* StepsForLesson(TutorialLessonId id, int& count) {
    switch (id) {
        case TutorialLessonId::BASIC_DEFENSE:
            count = (int)(sizeof(BASIC_STEPS) / sizeof(BASIC_STEPS[0]));
            return BASIC_STEPS;
        case TutorialLessonId::SENSOR:
            count = (int)(sizeof(SENSOR_STEPS) / sizeof(SENSOR_STEPS[0]));
            return SENSOR_STEPS;
        case TutorialLessonId::CANNON:
            count = (int)(sizeof(CANNON_STEPS) / sizeof(CANNON_STEPS[0]));
            return CANNON_STEPS;
        case TutorialLessonId::AND_GATE:
            count = (int)(sizeof(AND_STEPS) / sizeof(AND_STEPS[0]));
            return AND_STEPS;
        case TutorialLessonId::OR_GATE:
            count = (int)(sizeof(OR_STEPS) / sizeof(OR_STEPS[0]));
            return OR_STEPS;
        case TutorialLessonId::XOR_GATE:
            count = (int)(sizeof(XOR_STEPS) / sizeof(XOR_STEPS[0]));
            return XOR_STEPS;
        case TutorialLessonId::NAND_GATE:
            count = (int)(sizeof(NAND_STEPS) / sizeof(NAND_STEPS[0]));
            return NAND_STEPS;
        case TutorialLessonId::PERCEPTRON:
            count = (int)(sizeof(PCT_STEPS) / sizeof(PCT_STEPS[0]));
            return PCT_STEPS;
        case TutorialLessonId::MULTI_LANE:
            count = (int)(sizeof(MULTI_STEPS) / sizeof(MULTI_STEPS[0]));
            return MULTI_STEPS;
        default:
            count = (int)(sizeof(BASIC_STEPS) / sizeof(BASIC_STEPS[0]));
            return BASIC_STEPS;
    }
}

static const TutorialStep& CurrentStep(Game& G, int* outCount = nullptr) {
    int count = 0;
    const TutorialStep* steps = StepsForLesson(G.tutorial.lessonId, count);
    if (outCount) *outCount = count;
    return steps[std::max(0, std::min(count - 1, G.tutorial.stepIndex))];
}

bool IsTutorialTowerAllowed(Game& G, TType type) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return true;
    const TutorialObjective& obj = CurrentStep(G).objective;
    if (obj.kind != TutorialObjectiveKind::PLACE_TOWER) return false;
    return obj.towerType == TType::NONE || obj.towerType == type;
}

bool IsTutorialWaveStartAllowed(Game& G) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return true;
    return CurrentStep(G).objective.kind == TutorialObjectiveKind::START_WAVE;
}

const char* TutorialCurrentStepTitle(Game& G) {
    if (!G.tutorial.active) return "";
    return CurrentStep(G).title;
}

const char* TutorialCurrentStepHint(Game& G) {
    if (!G.tutorial.active) return "";
    return CurrentStep(G).hint;
}

static Tower& AddTutorialTower(Game& G, TType type, int gx, int gy, int level = 1) {
    Tower t;
    t.id = G.nextId++;
    t.type = type;
    t.gx = gx;
    t.gy = gy;
    t.level = std::max(1, std::min(3, level));
    t.sig = (type == TType::CPU) ? 1.f : 0.f;
    t.w1 = 0.8f;
    t.w2 = 0.6f;
    t.bias = -0.5f;
    t.cooldown = 0;
    t.anim = 0.f;
    G.ApplyTowerStats(t);
    G.towers.push_back(t);
    return G.towers.back();
}

static void ConnectTutorialTowers(Tower& src, Tower& dst) {
    src.conns.push_back(dst.id);
}

static void ConfigureTutorialLanes(Game& G, int laneCount) {
    laneCount = std::max(1, std::min(MAX_LANES, laneCount));
    G.activeLaneCount = laneCount;
    G.dualPath = laneCount > 1;
    for (int lane = 0; lane < MAX_LANES; lane++) {
        SetActiveLanePreset(lane, lane);
    }
}

int ClampTutorialLessonIndex(int index) {
    return std::max(0, std::min(TUTORIAL_LESSON_COUNT - 1, index));
}

const TutorialLessonInfo& GetTutorialLessonInfo(int index) {
    return LESSONS[ClampTutorialLessonIndex(index)];
}

static void ClearTutorialSessionWorld(Game& G) {
    G.towers.clear();
    G.enemies.clear();
    G.bullets.clear();
    G.pulses.clear();
    G.particles.clear();
    G.floats.clear();
    G.aiHints.clear();

    G.placing = TType::NONE;
    G.selectedId = -1;
    G.connectSrc = -1;
    G.spawned = 0;
    G.spawnLaneCursor = 0;
    G.spawnTimer = 0.f;
    G.waveCount = 0;
    G.trainingTimer = 0.f;
    G.trainingChoiceCount = 0;
    G.waveTelegraphTimer = 0.f;
    G.spawnPulseTimer = 0.f;
    G.combo = 0;
    G.comboTimer = 0.f;
    G.comboSurgeTimer = 0.f;
    G.currentEvent = Game::WaveEvent::NONE;
    G.eventName = "";
    G.eventBannerTimer = 0.f;
    G.currentIncident = Game::Incident::NONE;
    G.incidentName = "";
    G.incidentTimer = 0.f;
    G.incidentBannerTimer = 0.f;
    G.incidentRollTimer = 0.f;
    G.incidentTriggered = false;
    G.paused = false;
    G.showHelp = false;
    G.SetMsg("");
}

void EnterTutorialSelect(Game& G) {
    ClearTutorialSessionWorld(G);
    G.phase = Game::TUTORIAL_SELECT;
    G.paused = false;
    G.showHelp = false;
    G.tutorial.ResetTransient();
    G.tutorial.selectedLesson = ClampTutorialLessonIndex(G.tutorial.selectedLesson);
}

void StartTutorial(Game& G, TutorialLessonId lessonId) {
    int selected = ClampTutorialLessonIndex((int)lessonId);
    G.Reset();
    G.tutorial.active = true;
    G.tutorial.lessonId = (TutorialLessonId)selected;
    G.tutorial.selectedLesson = selected;
    G.tutorial.stepIndex = 0;
    G.tutorial.stepTimer = 0.f;
    G.tutorial.exitPromptOpen = false;
    G.tutorial.exitPromptChoice = 0;
    G.tutorial.returnToSelectOnExit = TutorialExitTarget::SELECT;
    G.tutorial.lessonComplete = false;
    G.tutorial.ResetObjectiveCounters();
    G.phase = Game::BUILD;
    G.credits = 320;
    G.lives = 20;
    G.cpuHp = 100.f;
    G.showAIHints = false;
    G.rng.seed(7300u + (unsigned)selected);
    BuildTutorialScenario(G, G.tutorial.lessonId);

    char msg[128];
    snprintf(msg, 128, "教學：%s — %s",
             GetTutorialLessonInfo(selected).title, CurrentStep(G).title);
    G.SetMsg(msg);
}

void RestartTutorialLesson(Game& G) {
    StartTutorial(G, G.tutorial.lessonId);
}

void AdvanceTutorialStep(Game& G) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;

    int count = 0;
    StepsForLesson(G.tutorial.lessonId, count);
    G.tutorial.stepIndex++;
    G.tutorial.stepTimer = 0.f;
    G.tutorial.ResetObjectiveCounters();

    if (G.tutorial.stepIndex >= count) {
        CompleteTutorialLesson(G);
        return;
    }

    char msg[128];
    snprintf(msg, 128, "教學：%s", CurrentStep(G).title);
    G.SetMsg(msg);
}

void CompleteTutorialLesson(Game& G) {
    if (!G.tutorial.active) return;
    int idx = ClampTutorialLessonIndex((int)G.tutorial.lessonId);
    G.tutorial.completedLessons[idx] = true;
    G.tutorial.lessonComplete = true;
    G.tutorial.stepTimer = 0.f;
    G.phase = Game::BUILD;
    G.placing = TType::NONE;
    G.connectSrc = -1;
    G.SetMsg("教學章節完成！按 ESC 可回章節選單，或按 R 重練。");
}

void RecordTutorialTowerPlaced(Game& G, TType type) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;
    G.tutorial.lastPlacedType = type;
    G.tutorial.placedCount++;
    const TutorialStep& step = CurrentStep(G);
    if (step.objective.kind == TutorialObjectiveKind::PLACE_TOWER &&
        step.objective.towerType != TType::NONE &&
        step.objective.towerType != type) {
        char msg[96];
        snprintf(msg, 96, "教學提示：這一步需要放置 %s。", TDef(step.objective.towerType).label);
        G.SetMsg(msg);
    }
}

void RecordTutorialTowerSelected(Game& G, TType type) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;
    G.tutorial.lastSelectedType = type;
    G.tutorial.selectedCount++;
    const TutorialStep& step = CurrentStep(G);
    if (step.objective.kind == TutorialObjectiveKind::SELECT_TOWER &&
        step.objective.towerType != TType::NONE &&
        step.objective.towerType != type) {
        char msg[96];
        snprintf(msg, 96, "教學提示：這一步需要選取 %s。", TDef(step.objective.towerType).label);
        G.SetMsg(msg);
    }
}

void RecordTutorialConnection(Game& G, TType srcType, TType dstType) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;
    G.tutorial.lastConnSrcType = srcType;
    G.tutorial.lastConnDstType = dstType;
    G.tutorial.connectionCount++;
    const TutorialStep& step = CurrentStep(G);
    if (step.objective.kind == TutorialObjectiveKind::CONNECT_TOWERS &&
        step.objective.towerType != TType::NONE &&
        step.objective.towerType != srcType) {
        char msg[128];
        snprintf(msg, 128, "教學提示：這一步需要從 %s 建立連線。", TDef(step.objective.towerType).label);
        G.SetMsg(msg);
    }
}

void RecordTutorialWaveStarted(Game& G) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;
    G.tutorial.waveStartCount++;
}

void RecordTutorialSkillUsed(Game& G, TType type) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;
    G.tutorial.lastSkillType = type;
    G.tutorial.skillUseCount++;
    const TutorialStep& step = CurrentStep(G);
    if (step.objective.kind == TutorialObjectiveKind::USE_SKILL &&
        step.objective.towerType != TType::NONE &&
        step.objective.towerType != type) {
        char msg[96];
        snprintf(msg, 96, "教學提示：這一步需要使用 %s 的技能。", TDef(step.objective.towerType).label);
        G.SetMsg(msg);
    }
}

void RecordTutorialEnemyKilled(Game& G, EType type) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;
    int idx = (int)type;
    if (idx >= 0 && idx < (int)G.tutorial.killCountByType.size()) {
        G.tutorial.killCountByType[idx]++;
    }
}

void RecordTutorialSurvived(Game& G) {
    if (!G.tutorial.active || G.tutorial.lessonComplete) return;
    G.tutorial.survivalCount++;
}

void ExitTutorial(Game& G, TutorialExitTarget target) {
    int selected = G.tutorial.selectedLesson;
    ClearTutorialSessionWorld(G);
    G.tutorial.ResetTransient();
    G.tutorial.selectedLesson = ClampTutorialLessonIndex(selected);

    if (target == TutorialExitTarget::MENU) {
        G.phase = Game::MENU;
    } else {
        G.phase = Game::TUTORIAL_SELECT;
    }
}

bool HandleTutorialInput(Game& G) {
    if (!G.tutorial.active) return false;

    Vector2 mp = VirtualMouse();

    if (G.tutorial.exitPromptOpen) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) {
            G.tutorial.exitPromptChoice = (G.tutorial.exitPromptChoice + 2) % 3;
            return true;
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB)) {
            G.tutorial.exitPromptChoice = (G.tutorial.exitPromptChoice + 1) % 3;
            return true;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            G.tutorial.exitPromptOpen = false;
            G.tutorial.exitPromptChoice = 0;
            G.paused = false;
            return true;
        }

        Rectangle resumeBtn = {(float)(VIRT_W / 2 - 300), (float)(VIRT_H / 2 + 84), 180.f, 48.f};
        Rectangle selectBtn = {(float)(VIRT_W / 2 - 90),  (float)(VIRT_H / 2 + 84), 180.f, 48.f};
        Rectangle menuBtn   = {(float)(VIRT_W / 2 + 120), (float)(VIRT_H / 2 + 84), 180.f, 48.f};
        bool clickedButton = false;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mp, resumeBtn)) {
                G.tutorial.exitPromptChoice = 0;
                clickedButton = true;
            } else if (CheckCollisionPointRec(mp, selectBtn)) {
                G.tutorial.exitPromptChoice = 1;
                clickedButton = true;
            } else if (CheckCollisionPointRec(mp, menuBtn)) {
                G.tutorial.exitPromptChoice = 2;
                clickedButton = true;
            }
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || clickedButton) {
            if (G.tutorial.exitPromptChoice == 0) {
                G.tutorial.exitPromptOpen = false;
                G.paused = false;
            } else if (G.tutorial.exitPromptChoice == 1) {
                ExitTutorial(G, TutorialExitTarget::SELECT);
            } else {
                ExitTutorial(G, TutorialExitTarget::MENU);
            }
            return true;
        }
        return true;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        G.showHelp = false;
        G.tutorial.exitPromptOpen = true;
        G.tutorial.exitPromptChoice = 0;
        G.paused = true;
        return true;
    }
    if (IsKeyPressed(KEY_R)) {
        RestartTutorialLesson(G);
        return true;
    }
    if (IsKeyPressed(KEY_H)) {
        G.showHelp = !G.showHelp;
        if (G.showHelp) G.paused = true;
        else            G.paused = false;
        return true;
    }
    if (IsKeyPressed(KEY_P)) {
        G.paused = !G.paused;
        return true;
    }

    if (G.showHelp) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            G.showHelp = false;
            G.paused = false;
        }
        return true;
    }

    if (IsKeyPressed(KEY_T)) {
        if (G.tutorial.lessonId == TutorialLessonId::MULTI_LANE) {
            G.showThreat = !G.showThreat;
            G.SetMsg(G.showThreat ? "教學：熱圖已開啟。" : "教學：熱圖已關閉。");
        } else {
            G.SetMsg("教學提示：此章節不使用熱圖，請依照左上角目標操作。");
        }
        return true;
    }
    if (IsKeyPressed(KEY_A)) {
        if (G.tutorial.lessonId == TutorialLessonId::PERCEPTRON ||
            G.tutorial.lessonId == TutorialLessonId::MULTI_LANE) {
            G.showAIHints = !G.showAIHints;
            G.SetMsg(G.showAIHints ? "教學：AI 提示已開啟。" : "教學：AI 提示已關閉。");
        } else {
            G.SetMsg("教學提示：此章節不使用 AI 提示，請依照教學目標操作。");
        }
        return true;
    }

    if (IsKeyPressed(KEY_N) && CurrentStep(G).allowSkip) {
        AdvanceTutorialStep(G);
        return true;
    }

    return false;
}

static bool ObjectiveComplete(Game& G, const TutorialStep& step) {
    const TutorialObjective& obj = step.objective;
    switch (obj.kind) {
        case TutorialObjectiveKind::NONE:
            return true;
        case TutorialObjectiveKind::WAIT_SECONDS:
            return G.tutorial.stepTimer >= obj.requiredSeconds;
        case TutorialObjectiveKind::PLACE_TOWER:
            return G.tutorial.placedCount >= std::max(1, obj.requiredCount) &&
                   (obj.towerType == TType::NONE || G.tutorial.lastPlacedType == obj.towerType);
        case TutorialObjectiveKind::SELECT_TOWER:
            return G.tutorial.selectedCount >= std::max(1, obj.requiredCount) &&
                   (obj.towerType == TType::NONE || G.tutorial.lastSelectedType == obj.towerType);
        case TutorialObjectiveKind::CONNECT_TOWERS:
            return G.tutorial.connectionCount >= std::max(1, obj.requiredCount) &&
                   (obj.towerType == TType::NONE || G.tutorial.lastConnSrcType == obj.towerType);
        case TutorialObjectiveKind::START_WAVE:
            return G.tutorial.waveStartCount >= std::max(1, obj.requiredCount);
        case TutorialObjectiveKind::KILL_ENEMY: {
            int idx = (int)obj.enemyType;
            if (idx < 0 || idx >= (int)G.tutorial.killCountByType.size()) return false;
            return G.tutorial.killCountByType[idx] >= std::max(1, obj.requiredCount);
        }
        case TutorialObjectiveKind::SURVIVE:
            return G.tutorial.survivalCount >= std::max(1, obj.requiredCount);
        case TutorialObjectiveKind::USE_SKILL:
            return G.tutorial.skillUseCount >= std::max(1, obj.requiredCount) &&
                   (obj.towerType == TType::NONE || G.tutorial.lastSkillType == obj.towerType);
    }
    return false;
}

void UpdateTutorial(Game& G, float dt) {
    if (!G.tutorial.active || G.tutorial.exitPromptOpen || G.tutorial.lessonComplete) return;
    G.tutorial.stepTimer += dt;

    if (ObjectiveComplete(G, CurrentStep(G))) {
        AdvanceTutorialStep(G);
    }
}

void BuildTutorialScenario(Game& G, TutorialLessonId lessonId) {
    G.towers.reserve(16);
    G.enemies.clear();
    G.bullets.clear();
    G.pulses.clear();
    G.particles.clear();
    G.floats.clear();
    G.wave = 0;
    G.waveCount = 0;
    G.spawned = 0;
    G.spawnTimer = 0.f;
    G.spawnLaneCursor = 0;
    G.trainingTimer = 0.f;
    G.trainingChoiceCount = 0;
    G.currentEvent = Game::WaveEvent::NONE;
    G.eventName = "";
    G.eventBannerTimer = 0.f;
    G.currentIncident = Game::Incident::NONE;
    G.incidentName = "";
    G.incidentTimer = 0.f;
    G.incidentBannerTimer = 0.f;
    G.incidentRollTimer = 0.f;
    G.incidentTriggered = true;
    G.blackoutActive = false;
    G.hasPlannedRouteChange = false;
    G.nextPreviewPaths = {{1, -1, -1, -1, -1, -1}};
    G.nextPreviewLaneCount = 1;
    G.showThreat = false;
    G.showAIHints = false;
    G.threatMap = ThreatMap{};
    G.intel = EnemyIntel{};
    G.defNN = DefenseAdvisorNN{};

    ConfigureTutorialLanes(G, lessonId == TutorialLessonId::MULTI_LANE ? 3 : 1);

    switch (lessonId) {
        case TutorialLessonId::BASIC_DEFENSE:
            G.credits = 420;
            G.lives = 20;
            G.cpuHp = 100.f;
            break;
        case TutorialLessonId::SENSOR: {
            G.credits = 500;
            Tower& cannon = AddTutorialTower(G, TType::CANNON, 10, 5, 1);
            (void)cannon;
            break;
        }
        case TutorialLessonId::CANNON: {
            G.credits = 520;
            Tower& sensor = AddTutorialTower(G, TType::SENSOR, 2, 8, 2);
            (void)sensor;
            break;
        }
        case TutorialLessonId::AND_GATE: {
            G.credits = 560;
            Tower& sensor = AddTutorialTower(G, TType::SENSOR, 2, 8, 2);
            Tower& cannon = AddTutorialTower(G, TType::CANNON, 10, 5, 2);
            ConnectTutorialTowers(sensor, cannon);
            break;
        }
        case TutorialLessonId::OR_GATE: {
            G.credits = 560;
            Tower& s1 = AddTutorialTower(G, TType::SENSOR, 2, 7, 1);
            Tower& s2 = AddTutorialTower(G, TType::SENSOR, 5, 10, 1);
            Tower& cannon = AddTutorialTower(G, TType::CANNON, 10, 5, 2);
            ConnectTutorialTowers(s1, cannon);
            ConnectTutorialTowers(s2, cannon);
            break;
        }
        case TutorialLessonId::XOR_GATE: {
            G.credits = 560;
            Tower& sensor = AddTutorialTower(G, TType::SENSOR, 2, 8, 2);
            Tower& cannon = AddTutorialTower(G, TType::CANNON, 10, 5, 2);
            ConnectTutorialTowers(sensor, cannon);
            break;
        }
        case TutorialLessonId::NAND_GATE: {
            G.credits = 580;
            Tower& sensor = AddTutorialTower(G, TType::SENSOR, 2, 8, 2);
            Tower& cannon = AddTutorialTower(G, TType::CANNON, 10, 5, 3);
            ConnectTutorialTowers(sensor, cannon);
            break;
        }
        case TutorialLessonId::PERCEPTRON: {
            G.credits = 620;
            Tower& s1 = AddTutorialTower(G, TType::SENSOR, 2, 7, 1);
            Tower& s2 = AddTutorialTower(G, TType::SENSOR, 5, 10, 1);
            Tower& cannon = AddTutorialTower(G, TType::CANNON, 10, 5, 2);
            ConnectTutorialTowers(s1, cannon);
            ConnectTutorialTowers(s2, cannon);
            break;
        }
        case TutorialLessonId::MULTI_LANE: {
            G.credits = 720;
            G.nextPreviewLaneCount = 3;
            G.nextPreviewPaths = {{0, 1, 2, -1, -1, -1}};
            Tower& s0 = AddTutorialTower(G, TType::SENSOR, 2, 8, 1);
            Tower& c0 = AddTutorialTower(G, TType::CANNON, 9, 5, 2);
            Tower& s1 = AddTutorialTower(G, TType::SENSOR, 6, 2, 1);
            Tower& c1 = AddTutorialTower(G, TType::CANNON, 13, 6, 2);
            ConnectTutorialTowers(s0, c0);
            ConnectTutorialTowers(s1, c1);
            break;
        }
        default:
            break;
    }

    G.SetMsg("教學場景已建立。依照左上角目標操作。");
}

void SpawnTutorialEnemy(Game& G, EType type, int laneSlot, float pathPos) {
    Enemy e;
    e.id = G.nextId++;
    e.type = type;
    e.pathPos = std::max(0.f, pathPos);
    e.angle = 0.f;
    e.bossState = BossState::CHARGE;
    e.evadeSpdMult = 1.f;
    e.pathIdx = std::max(0, std::min(MAX_LANES - 1, laneSlot));

    switch (type) {
        case EType::BASIC:
            e.hp = e.maxHp = 38.f;
            e.spd = 1.25f;
            e.reward = 8;
            break;
        case EType::FAST:
            e.hp = e.maxHp = 28.f;
            e.spd = 2.0f;
            e.reward = 8;
            break;
        case EType::ARMORED:
            e.hp = e.maxHp = 90.f;
            e.armor = 0.45f;
            e.spd = 0.75f;
            e.reward = 16;
            break;
        case EType::ELITE:
            e.hp = e.maxHp = 80.f;
            e.spd = 0.95f;
            e.reward = 18;
            e.shielded = true;
            e.shieldHp = e.maxHp * 0.20f;
            break;
        case EType::BOSS:
            e.hp = e.maxHp = 260.f;
            e.armor = 0.30f;
            e.spd = 0.45f;
            e.reward = 80;
            break;
    }

    e.spawnFx = 0.65f;
    G.enemies.push_back(e);
    int ti = (int)type;
    if (ti >= 0 && ti < 5) G.intel.typeSpawned[ti]++;
    if (e.pathIdx >= 0 && e.pathIdx < MAX_LANES) G.intel.pathSpawned[e.pathIdx]++;
}

static void DrawTutorialCellHint(Game& G, int gx, int gy, Color col, const char* label) {
    auto o = G.MapOrigin();
    float x = o.x + gx * CELL;
    float y = o.y + gy * CELL;
    float t = (float)GetTime();
    float pulse = 0.72f + 0.28f * sinf(t * 5.f + gx * 0.4f + gy * 0.2f);
    DrawRectangle((int)x + 3, (int)y + 3, CELL - 6, CELL - 6, AlphaOf(col, (int)(28 * pulse)));
    DrawRectangleLinesEx({x + 3.f, y + 3.f, (float)CELL - 6.f, (float)CELL - 6.f}, 2.f,
                         AlphaOf(col, (int)(150 + 70 * pulse)));
    DrawCircleLinesV({x + CELL * 0.5f, y + CELL * 0.5f}, CELL * (0.36f + 0.05f * pulse),
                     AlphaOf(col, (int)(120 + 70 * pulse)));
    if (label && label[0] != '\0') {
        DTC(label, (int)(x + CELL * 0.5f), (int)(y + CELL * 0.5f), FS_TINY, AlphaOf(WHITE, 160));
    }
}

static void DrawTutorialMapHints(Game& G) {
    if (!G.tutorial.active || G.tutorial.exitPromptOpen || G.tutorial.lessonComplete) return;
    const TutorialObjective& obj = CurrentStep(G).objective;
    const TutorialLessonInfo& lesson = GetTutorialLessonInfo((int)G.tutorial.lessonId);

    if (obj.kind == TutorialObjectiveKind::PLACE_TOWER) {
        switch (obj.towerType) {
            case TType::SENSOR:
                DrawTutorialCellHint(G, 2, 8, lesson.col, "SEN");
                DrawTutorialCellHint(G, 3, 7, lesson.col, "");
                DrawTutorialCellHint(G, 5, 10, lesson.col, "");
                break;
            case TType::CANNON:
                DrawTutorialCellHint(G, 9, 5, lesson.col, "GUN");
                DrawTutorialCellHint(G, 10, 6, lesson.col, "");
                break;
            case TType::AND:
            case TType::OR:
            case TType::XOR:
            case TType::NAND:
            case TType::PERCEPTRON:
                DrawTutorialCellHint(G, 6, 6, lesson.col, TDef(obj.towerType).sym);
                DrawTutorialCellHint(G, 7, 7, lesson.col, "");
                break;
            default:
                break;
        }
    } else if (obj.kind == TutorialObjectiveKind::CONNECT_TOWERS) {
        for (auto& src : G.towers) {
            if (src.type != obj.towerType) continue;
            for (auto& dst : G.towers) {
                if (dst.type != TType::CANNON) continue;
                Vector2 a = G.CC(src.gx, src.gy);
                Vector2 b = G.CC(dst.gx, dst.gy);
                DrawLineEx(a, b, 4.f, AlphaOf(lesson.col, 90));
                DrawCircleLinesV(a, CELL * 0.45f, AlphaOf(lesson.col, 185));
                DrawCircleLinesV(b, CELL * 0.45f, AlphaOf(COL_CANNON, 185));
            }
        }
    } else if (obj.kind == TutorialObjectiveKind::START_WAVE) {
        int laneCount = G.ActiveLaneCount();
        for (int lane = 0; lane < laneCount; lane++) {
            const auto& cells = G.LaneCells(lane);
            if (cells.empty()) continue;
            DrawTutorialCellHint(G, cells.front().gx, cells.front().gy, lesson.col, "IN");
        }
    }
}

static void SpawnTutorialWaveForLesson(Game& G, TutorialLessonId lessonId) {
    switch (lessonId) {
        case TutorialLessonId::BASIC_DEFENSE:
            SpawnTutorialEnemy(G, EType::BASIC, 0, 0.f);
            break;
        case TutorialLessonId::SENSOR:
            SpawnTutorialEnemy(G, EType::FAST, 0, 0.f);
            SpawnTutorialEnemy(G, EType::FAST, 0, 1.5f);
            break;
        case TutorialLessonId::CANNON:
            SpawnTutorialEnemy(G, EType::BASIC, 0, 0.f);
            SpawnTutorialEnemy(G, EType::BOSS, 0, 0.5f);
            break;
        case TutorialLessonId::AND_GATE:
            SpawnTutorialEnemy(G, EType::ARMORED, 0, 0.f);
            break;
        case TutorialLessonId::OR_GATE:
            SpawnTutorialEnemy(G, EType::BASIC, 0, 0.f);
            SpawnTutorialEnemy(G, EType::BASIC, 0, 1.f);
            SpawnTutorialEnemy(G, EType::FAST, 0, 2.f);
            break;
        case TutorialLessonId::XOR_GATE:
            SpawnTutorialEnemy(G, EType::ELITE, 0, 0.f);
            break;
        case TutorialLessonId::NAND_GATE:
            SpawnTutorialEnemy(G, EType::ARMORED, 0, 0.f);
            SpawnTutorialEnemy(G, EType::ARMORED, 0, 1.2f);
            break;
        case TutorialLessonId::PERCEPTRON:
            SpawnTutorialEnemy(G, EType::BASIC, 0, 0.f);
            SpawnTutorialEnemy(G, EType::FAST, 0, 1.f);
            SpawnTutorialEnemy(G, EType::ARMORED, 0, 2.f);
            break;
        case TutorialLessonId::MULTI_LANE:
            SpawnTutorialEnemy(G, EType::BASIC, 0, 0.f);
            SpawnTutorialEnemy(G, EType::FAST, 1, 0.f);
            SpawnTutorialEnemy(G, EType::ARMORED, 2, 0.f);
            break;
        default:
            SpawnTutorialEnemy(G, EType::BASIC, 0, 0.f);
            break;
    }
}

bool StartTutorialWave(Game& G) {
    if (!G.tutorial.active || G.phase != Game::BUILD) return false;

    G.wave++;
    G.enemies.clear();
    G.bullets.clear();
    G.spawned = 0;
    G.waveCount = 0;
    G.spawnTimer = 0.f;
    G.spawnLaneCursor = 0;
    G.phase = Game::FIGHT;
    G.trainingTimer = 0.f;
    G.trainingChoiceCount = 0;
    G.waveKills = 0;
    G.waveEscaped = 0;
    G.waveDmg = 0.f;
    G.waveTelegraphTimer = 0.6f;
    G.spawnPulseTimer = 0.3f;
    G.spawnPulsePath = 0;
    G.currentEvent = Game::WaveEvent::NONE;
    G.eventName = "";
    G.eventBannerTimer = 0.f;
    G.currentIncident = Game::Incident::NONE;
    G.incidentName = "";
    G.incidentTimer = 0.f;
    G.incidentBannerTimer = 0.f;
    G.incidentRollTimer = 0.f;
    G.incidentTriggered = true;
    G.blackoutActive = false;
    G.placing = TType::NONE;
    G.connectSrc = -1;

    SpawnTutorialWaveForLesson(G, G.tutorial.lessonId);
    G.waveCount = (int)G.enemies.size();
    G.spawned = G.waveCount;
    RecordTutorialWaveStarted(G);

    char msg[128];
    snprintf(msg, 128, "教學短波次：%d 個指定敵人。", G.waveCount);
    G.SetMsg(msg);
    return true;
}

void DrawTutorialSelect(Game& G) {
    float t = (float)GetTime();
    int cx = VIRT_W / 2;

    DrawRectangleGradientV(0, 0, VIRT_W, VIRT_H, Color{3, 7, 14, 255}, Color{8, 18, 35, 255});
    DrawStars(G);

    DTC("教學關卡", cx, 92, FS_BIG, AlphaOf(COL_CPU, 232));
    DTC("選擇想練習的主題；所有章節都可自由進入", cx, 142, FS_MED, AlphaOf(WHITE, 130));

    int gridX = 150;
    int gridY = 210;
    int cardW = 500;
    int cardH = 180;
    int gapX = 60;
    int gapY = 34;

    for (int i = 0; i < TUTORIAL_LESSON_COUNT; i++) {
        const TutorialLessonInfo& lesson = GetTutorialLessonInfo(i);
        int col = i % 3;
        int row = i / 3;
        int x = gridX + col * (cardW + gapX);
        int y = gridY + row * (cardH + gapY);
        bool selected = (G.tutorial.selectedLesson == i);
        float pulse = 0.78f + 0.22f * sinf(t * 3.f + i * 0.7f);
        Color border = selected ? AlphaOf(lesson.col, (int)(210 * pulse)) : AlphaOf(lesson.col, 86);
        Color fill = selected ? AlphaOf(lesson.col, 28) : AlphaOf(lesson.col, 12);

        DrawRoundBox((float)x, (float)y, (float)cardW, (float)cardH, 10.f, fill, border, selected ? 2.4f : 1.2f);
        char idx[8];
        snprintf(idx, 8, "%d", i + 1);
        DrawCircleV({(float)x + 34.f, (float)y + 34.f}, 17.f, AlphaOf(lesson.col, selected ? 190 : 112));
        DTC(idx, x + 34, y + 34, FS_TINY, BG);
        DTX(lesson.title, (float)x + 64.f, (float)y + 22.f, FS_MED, AlphaOf(lesson.col, 230));
        DTX(lesson.focus, (float)x + 22.f, (float)y + 64.f, FS_TINY, AlphaOf(COL_SENSOR, 180));
        DTX(lesson.enemy, (float)x + 220.f, (float)y + 64.f, FS_TINY, AlphaOf(COL_CANNON, 170));
        DTX(lesson.duration, (float)x + cardW - 96.f, (float)y + 64.f, FS_TINY, AlphaOf(WHITE, 120));
        DTX(lesson.desc, (float)x + 22.f, (float)y + 104.f, FS_TINY, AlphaOf(WHITE, 158));
        if (G.tutorial.completedLessons[i]) {
            DrawRoundBox((float)x + cardW - 126.f, (float)y + 134.f, 98.f, 24.f, 5.f,
                         AlphaOf(COL_PERC, 30), AlphaOf(COL_PERC, 135), 1.f);
            DTC("已完成", x + cardW - 77, y + 146, FS_TINY, AlphaOf(COL_PERC, 210));
        } else {
            DTX("未完成", (float)x + cardW - 102.f, (float)y + 140.f, FS_TINY, AlphaOf(WHITE, 72));
        }
        DTX("按數字鍵或點擊進入", (float)x + 22.f, (float)y + 140.f, FS_TINY, AlphaOf(WHITE, selected ? 150 : 80));
    }

    DTC("[ENTER] 進入選取章節   [ESC] 返回主選單   [H] 說明", cx, VIRT_H - 72, FS_MED, AlphaOf(WHITE, 118));
}

void DrawTutorialOverlay(Game& G) {
    if (!G.tutorial.active) return;
    DrawTutorialMapHints(G);

    const TutorialLessonInfo& lesson = GetTutorialLessonInfo((int)G.tutorial.lessonId);
    int stepCount = 0;
    const TutorialStep& step = CurrentStep(G, &stepCount);
    int x = PANEL_L + 26;
    int y = TOPBAR_H + 22;
    int w = 760;
    int h = 116;

    DrawRoundBox((float)x, (float)y, (float)w, (float)h, 8.f,
                 AlphaOf(BG, 218), AlphaOf(lesson.col, 122), 1.5f);
    DTX("TUTORIAL", (float)x + 18.f, (float)y + 12.f, FS_TINY, AlphaOf(lesson.col, 210));
    DTX(lesson.title, (float)x + 112.f, (float)y + 10.f, FS_SMALL, AlphaOf(lesson.col, 230));
    char progress[32];
    snprintf(progress, 32, "%d / %d", std::min(G.tutorial.stepIndex + 1, stepCount), stepCount);
    DTX(progress, (float)x + w - 68.f, (float)y + 13.f, FS_TINY, AlphaOf(WHITE, 138));

    if (G.tutorial.lessonComplete) {
        DTX("章節完成", (float)x + 18.f, (float)y + 42.f, FS_SMALL, AlphaOf(COL_PERC, 220));
        DTX("按 ESC 回章節選單，或按 R 重新練習這個章節。",
            (float)x + 18.f, (float)y + 72.f, FS_TINY, AlphaOf(WHITE, 148));
    } else {
        DTX(step.title, (float)x + 18.f, (float)y + 40.f, FS_SMALL, AlphaOf(WHITE, 210));
        DTX(step.body, (float)x + 18.f, (float)y + 66.f, FS_TINY, AlphaOf(WHITE, 158));
        DTX(step.hint, (float)x + 18.f, (float)y + 86.f, FS_TINY, AlphaOf(lesson.col, 176));
        const char* skip = step.allowSkip ? "   [N] 下一步" : "";
        char keys[96];
        snprintf(keys, 96, "[ESC] 退出   [R] 重開   [H] 說明%s", skip);
        DTX(keys, (float)x + 390.f, (float)y + 86.f, FS_TINY, AlphaOf(WHITE, 100));
    }

    if (G.tutorial.exitPromptOpen) {
        DrawRectangle(0, 0, VIRT_W, VIRT_H, AlphaOf(BLACK, 150));
        int bx = VIRT_W / 2 - 390;
        int by = VIRT_H / 2 - 138;
        int bw = 780;
        int bh = 300;
        DrawRoundBox((float)bx, (float)by, (float)bw, (float)bh, 12.f,
                     AlphaOf(BG, 238), AlphaOf(lesson.col, 150), 2.f);
        DTC("離開教學？", VIRT_W / 2, by + 58, FS_LARGE, AlphaOf(lesson.col, 230));
        DTC("目前章節進度尚未保存。你可以繼續、回章節選單，或回主選單。",
            VIRT_W / 2, by + 104, FS_TINY, AlphaOf(WHITE, 150));

        const char* labels[] = {"繼續", "章節選單", "主選單"};
        Color cols[] = {COL_PERC, COL_AI, COL_CANNON};
        int btnW = 180;
        int btnH = 48;
        int startX = VIRT_W / 2 - 300;
        for (int i = 0; i < 3; i++) {
            int px = startX + i * 210;
            bool sel = (G.tutorial.exitPromptChoice == i);
            DrawRoundBox((float)px, (float)(VIRT_H / 2 + 84), (float)btnW, (float)btnH, 8.f,
                         AlphaOf(cols[i], sel ? 36 : 16), AlphaOf(cols[i], sel ? 210 : 90), sel ? 2.2f : 1.2f);
            DTC(labels[i], px + btnW / 2, VIRT_H / 2 + 108, FS_MED, AlphaOf(cols[i], sel ? 240 : 170));
        }
        DTC("[Enter] 確認   [Esc] 取消   [方向鍵/Tab] 切換",
            VIRT_W / 2, by + bh - 42, FS_TINY, AlphaOf(WHITE, 110));
    }
}
