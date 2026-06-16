#pragma once
// ================================================================
//  tutorial.h — 教學關卡資料與 Phase 1 入口
// ================================================================
#include "constants.h"
#include <array>

class Game;

enum class TutorialLessonId {
    BASIC_DEFENSE,
    SENSOR,
    CANNON,
    AND_GATE,
    OR_GATE,
    XOR_GATE,
    NAND_GATE,
    PERCEPTRON,
    MULTI_LANE,
    COUNT
};

enum class TutorialExitTarget {
    SELECT,
    MENU
};

enum class TutorialObjectiveKind {
    NONE,
    PLACE_TOWER,
    SELECT_TOWER,
    CONNECT_TOWERS,
    START_WAVE,
    KILL_ENEMY,
    SURVIVE,
    USE_SKILL,
    WAIT_SECONDS
};

constexpr int TUTORIAL_LESSON_COUNT = (int)TutorialLessonId::COUNT;
constexpr int TUTORIAL_SELECT_COLS = 3;
constexpr int TUTORIAL_SELECT_CARD_W = 398;
constexpr int TUTORIAL_SELECT_CARD_H = 188;
constexpr int TUTORIAL_SELECT_GAP_X = 34;
constexpr int TUTORIAL_SELECT_GAP_Y = 30;
constexpr int TUTORIAL_SELECT_GRID_W =
    TUTORIAL_SELECT_COLS * TUTORIAL_SELECT_CARD_W +
    (TUTORIAL_SELECT_COLS - 1) * TUTORIAL_SELECT_GAP_X;
constexpr int TUTORIAL_SELECT_GRID_X = (VIRT_W - TUTORIAL_SELECT_GRID_W) / 2;
constexpr int TUTORIAL_SELECT_GRID_Y = 250;
constexpr int TUTORIAL_EXIT_PANEL_W = 860;
constexpr int TUTORIAL_EXIT_PANEL_H = 340;
constexpr int TUTORIAL_EXIT_BUTTON_W = 208;
constexpr int TUTORIAL_EXIT_BUTTON_H = 60;
constexpr int TUTORIAL_EXIT_BUTTON_GAP = 26;
constexpr int TUTORIAL_EXIT_BUTTON_Y_OFFSET = 206;

inline Rectangle TutorialLessonCardRect(int index) {
    int col = index % TUTORIAL_SELECT_COLS;
    int row = index / TUTORIAL_SELECT_COLS;
    return {
        (float)(TUTORIAL_SELECT_GRID_X + col * (TUTORIAL_SELECT_CARD_W + TUTORIAL_SELECT_GAP_X)),
        (float)(TUTORIAL_SELECT_GRID_Y + row * (TUTORIAL_SELECT_CARD_H + TUTORIAL_SELECT_GAP_Y)),
        (float)TUTORIAL_SELECT_CARD_W,
        (float)TUTORIAL_SELECT_CARD_H
    };
}

inline Rectangle TutorialExitPromptPanelRect() {
    return {
        (float)(VIRT_W / 2 - TUTORIAL_EXIT_PANEL_W / 2),
        (float)(VIRT_H / 2 - TUTORIAL_EXIT_PANEL_H / 2),
        (float)TUTORIAL_EXIT_PANEL_W,
        (float)TUTORIAL_EXIT_PANEL_H
    };
}

inline Rectangle TutorialExitPromptButtonRect(int index) {
    index = std::max(0, std::min(2, index));
    Rectangle panel = TutorialExitPromptPanelRect();
    float totalW =
        3.f * (float)TUTORIAL_EXIT_BUTTON_W +
        2.f * (float)TUTORIAL_EXIT_BUTTON_GAP;
    float startX = panel.x + (panel.width - totalW) * 0.5f;
    return {
        startX + index * (TUTORIAL_EXIT_BUTTON_W + TUTORIAL_EXIT_BUTTON_GAP),
        panel.y + (float)TUTORIAL_EXIT_BUTTON_Y_OFFSET,
        (float)TUTORIAL_EXIT_BUTTON_W,
        (float)TUTORIAL_EXIT_BUTTON_H
    };
}

class TutorialLessonInfo {
public:
    TutorialLessonId id;
    const char*      title;
    const char*      focus;
    const char*      enemy;
    const char*      duration;
    const char*      desc;
    Color            col;
};

class TutorialObjective {
public:
    TutorialObjectiveKind kind{TutorialObjectiveKind::NONE};
    TType                 towerType{TType::NONE};
    EType                 enemyType{EType::BASIC};
    int                   requiredCount{0};
    float                 requiredSeconds{0.f};
};

class TutorialStep {
public:
    const char*       title{""};
    const char*       body{""};
    const char*       hint{""};
    TutorialObjective objective{};
    bool              allowSkip{false};
};

class TutorialRuntime {
public:
    bool             active{false};
    TutorialLessonId lessonId{TutorialLessonId::BASIC_DEFENSE};
    int              selectedLesson{0};
    int              stepIndex{0};
    float            stepTimer{0.f};
    bool             exitPromptOpen{false};
    int              exitPromptChoice{0};
    TutorialExitTarget returnToSelectOnExit{TutorialExitTarget::SELECT};
    bool             lessonComplete{false};
    TType            lastPlacedType{TType::NONE};
    TType            lastSelectedType{TType::NONE};
    TType            lastSkillType{TType::NONE};
    TType            lastConnSrcType{TType::NONE};
    TType            lastConnDstType{TType::NONE};
    int              placedCount{0};
    int              selectedCount{0};
    int              connectionCount{0};
    int              waveStartCount{0};
    int              skillUseCount{0};
    int              survivalCount{0};
    std::array<int, 5> killCountByType{};
    std::array<bool, TUTORIAL_LESSON_COUNT> completedLessons{};

    void ResetObjectiveCounters() {
        lastPlacedType = TType::NONE;
        lastSelectedType = TType::NONE;
        lastSkillType = TType::NONE;
        lastConnSrcType = TType::NONE;
        lastConnDstType = TType::NONE;
        placedCount = 0;
        selectedCount = 0;
        connectionCount = 0;
        waveStartCount = 0;
        skillUseCount = 0;
        survivalCount = 0;
        killCountByType.fill(0);
    }

    void ResetTransient() {
        active = false;
        lessonId = TutorialLessonId::BASIC_DEFENSE;
        stepIndex = 0;
        stepTimer = 0.f;
        exitPromptOpen = false;
        exitPromptChoice = 0;
        returnToSelectOnExit = TutorialExitTarget::SELECT;
        lessonComplete = false;
        ResetObjectiveCounters();
    }
};

const TutorialLessonInfo& GetTutorialLessonInfo(int index);
int  ClampTutorialLessonIndex(int index);

void EnterTutorialSelect(Game& G);
void StartTutorial(Game& G, TutorialLessonId lessonId);
void RestartTutorialLesson(Game& G);
void ExitTutorial(Game& G, TutorialExitTarget target);
bool HandleTutorialInput(Game& G);
void UpdateTutorial(Game& G, float dt);
void BuildTutorialScenario(Game& G, TutorialLessonId lessonId);
void SpawnTutorialEnemy(Game& G, EType type, int laneSlot, float pathPos = 0.f);
bool StartTutorialWave(Game& G);
bool IsTutorialTowerAllowed(Game& G, TType type);
bool IsTutorialWaveStartAllowed(Game& G);
const char* TutorialCurrentStepTitle(Game& G);
const char* TutorialCurrentStepHint(Game& G);
void AdvanceTutorialStep(Game& G);
void CompleteTutorialLesson(Game& G);
void RecordTutorialTowerPlaced(Game& G, TType type);
void RecordTutorialTowerSelected(Game& G, TType type);
void RecordTutorialConnection(Game& G, TType srcType, TType dstType);
void RecordTutorialWaveStarted(Game& G);
void RecordTutorialSkillUsed(Game& G, TType type);
void RecordTutorialEnemyKilled(Game& G, EType type);
void RecordTutorialSurvived(Game& G);
void DrawTutorialSelect(Game& G);
void DrawTutorialOverlay(Game& G);
