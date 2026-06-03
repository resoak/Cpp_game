// ================================================================
//  highscore.h — 高分系統模組（滿足 5 項技術要求）
//
//  本模組示範：
//    1. 物件導向設計：class、繼承 (inheritance)、多型 (polymorphism)
//    2. 檔案輸入輸出 (File I/O)：std::ofstream / std::ifstream
//    3. 資料結構：std::vector + std::map
//    4. 錯誤處理：try / catch / throw
//    5. GUI 整合：DrawLeaderboard() 透過 raylib 繪製排行榜
// ================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <algorithm>
#include <ctime>

// 前向宣告 Game，避免 include 循環
struct Game;

// ══════════════════════════════════════════════════════════════════
//  ScoreEntry  —  分數項目的「抽象基類」
//
//  展示 OOP 三大要素：
//    - class  關鍵字（與 struct 不同：預設 private，可加存取控制）
//    - virtual 函式 → 多型 (polymorphism) 的基礎
//    - 純虛擬函式 Format() → 衍生類別必須實作（抽象介面）
// ══════════════════════════════════════════════════════════════════
class ScoreEntry {
public:
    // 公開欄位（為簡化 getter/setter；class 仍展示封裝概念）
    std::string playerName;
    int         waveReached{0};
    long        timestamp{0};      // 儲存 epoch 秒數

    // ── 建構子 ─────────────────────────────────────────────────
    ScoreEntry() = default;
    ScoreEntry(const std::string& name, int wave);

    // ── 虛擬解構：多型釋放的關鍵 ──────────────────────────────
    virtual ~ScoreEntry() = default;

    // ── 純虛擬函式：衍生類別「必須」實作（抽象介面）───────────
    virtual std::string Format() const = 0;

    // ── 虛擬函式：衍生類別可選擇性 override ───────────────────
    virtual void Save(std::ofstream& out) const;
    virtual void Load(std::ifstream& in);

    // ── 比較：分數越高排越前面（descending）───────────────────
    virtual int Compare(const ScoreEntry& other) const;
};

// ══════════════════════════════════════════════════════════════════
//  TowerDefenseScore  —  塔防專用分數（繼承 ScoreEntry）
//
//  展示 OOP 的「繼承」與「多型」：
//    - : public ScoreEntry  → 公開繼承
//    - override           → 明確標示覆寫
//    - 用基底類別指標/參考呼叫時，自動 dispatch 到衍生版本
// ══════════════════════════════════════════════════════════════════
class TowerDefenseScore : public ScoreEntry {
public:
    int   kills{0};
    float accuracy{0.f};   // 命中率 0~1

    TowerDefenseScore() = default;
    TowerDefenseScore(const std::string& name, int wave, int k, float acc);

    // ── override 標記：編譯器會驗證簽名是否真的覆寫基底 ──────
    void        Save(std::ofstream& out) const override;
    void        Load(std::ifstream& in)        override;
    std::string Format() const                 override;
};

// ══════════════════════════════════════════════════════════════════
//  HighScoreManager  —  排行榜管理器
//
//  展示「資料結構」與「錯誤處理」：
//    - std::vector<TowerDefenseScore> entries;  儲存所有紀錄
//    - std::map<std::string, int>    nameIndex; 名稱 → 索引（快速查詢）
//    - try / catch 包覆所有檔案 I/O
// ══════════════════════════════════════════════════════════════════
class HighScoreManager {
public:
    static constexpr int          MAX_ENTRIES  = 10;
    static constexpr const char*  FILE_PATH    = "highscore.dat";

    // ── 資料結構：兩個 STL container 並用 ──────────────────────
    std::vector<TowerDefenseScore> entries;     // 主要儲存（可排序）
    std::map<std::string, int>    nameIndex;    // 玩家名 → vector 索引

    HighScoreManager() = default;

    // ── 行為介面 ──────────────────────────────────────────────
    void AddScore(const TowerDefenseScore& score);    // 加入並排序
    bool SaveToFile() noexcept(false);                 // File I/O + try-catch
    bool LoadFromFile() noexcept(false);               // File I/O + try-catch
    void Clear();

    // ── 查詢 ──────────────────────────────────────────────────
    const TowerDefenseScore* GetBest() const;
    const TowerDefenseScore* FindByName(const std::string& name) const;
    std::size_t Size() const { return entries.size(); }

    // ── GUI：把排行榜畫到 raylib 視窗上 ───────────────────────
    void DrawLeaderboard(const Game& G, int x, int y, int w, int h) const;

private:
    // 內部 helper：把 entries 排序（wave descending）
    void SortDescending();
};

// ══════════════════════════════════════════════════════════════════
//  例外類別：自訂錯誤型別
//
//  展示錯誤處理：throw 一個有意義的物件，呼叫端可針對性 catch
// ══════════════════════════════════════════════════════════════════
class HighScoreException : public std::runtime_error {
public:
    explicit HighScoreException(const std::string& msg)
        : std::runtime_error("[HighScore] " + msg) {}
};
