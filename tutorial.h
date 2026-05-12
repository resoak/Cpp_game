#pragma once
// ================================================================
//  tutorial.h — 教學關卡資料與 Phase 1 入口
// ================================================================
#include "constants.h"
#include <array>

struct Game;

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

struct TutorialLessonInfo {
    TutorialLessonId id;
    const char*      title;
    const char*      focus;
    const char*      enemy;
    const char*      duration;
    const char*      desc;
    Color            col;
};

struct TutorialObjective {
    TutorialObjectiveKind kind{TutorialObjectiveKind::NONE};
    TType                 towerType{TType::NONE};
    EType                 enemyType{EType::BASIC};
    int                   requiredCount{0};
    float                 requiredSeconds{0.f};
};

struct TutorialStep {
    const char*       title{""};
    const char*       body{""};
    const char*       hint{""};
    TutorialObjective objective{};
    bool              allowSkip{false};
};

struct TutorialRuntime {
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
