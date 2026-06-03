// ================================================================
//  highscore.cpp — 高分系統實作
//
//  涵蓋技術要求：
//    1. 物件導向（class + inheritance + polymorphism 完整示範）
//    2. 檔案 I/O（std::ofstream / std::ifstream + 文字格式）
//    3. 資料結構（std::vector + std::map 並用）
//    4. 錯誤處理（try / catch / throw + 自訂例外類別）
//    5. GUI 整合（DrawLeaderboard 用 raylib 繪製）
// ================================================================
#include "highscore.h"
#include "game.h"
#include "font.h"
#include "constants.h"
#include "raylib.h"

#include <iostream>
#include <sstream>
#include <cstdio>
#include <limits>

// ══════════════════════════════════════════════════════════════════
//  ScoreEntry  —  基類實作
// ══════════════════════════════════════════════════════════════════
ScoreEntry::ScoreEntry(const std::string& name, int wave)
    : playerName(name)
    , waveReached(wave)
    , timestamp(static_cast<long>(std::time(nullptr)))
{}

void ScoreEntry::Save(std::ofstream& out) const {
    // 簡單的文字格式：name wave timestamp\n
    // 寫入失敗會設 badbit，由呼叫端的 try/catch 統一處理
    if (!(out << playerName << '\n'
              << waveReached << '\n'
              << timestamp << '\n')) {
        throw HighScoreException("ScoreEntry::Save 寫入失敗");
    }
}

void ScoreEntry::Load(std::ifstream& in) {
    if (!(std::getline(in, playerName)
          && (in >> waveReached)
          && (in >> timestamp))) {
        throw HighScoreException("ScoreEntry::Load 讀取失敗（檔案可能損毀）");
    }
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // 吃掉換行
}

int ScoreEntry::Compare(const ScoreEntry& other) const {
    // 預設：以 waveReached 比較（衍生類別可覆寫）
    return waveReached - other.waveReached;
}

// ══════════════════════════════════════════════════════════════════
//  TowerDefenseScore  —  衍生類別實作（多型示範）
// ══════════════════════════════════════════════════════════════════
TowerDefenseScore::TowerDefenseScore(const std::string& name, int wave, int k, float acc)
    : ScoreEntry(name, wave)   // 呼叫基底建構子
    , kills(k)
    , accuracy(acc)
{}

void TowerDefenseScore::Save(std::ofstream& out) const {
    // 先存基底欄位（展示基底與衍生合作）
    ScoreEntry::Save(out);
    // 再存衍生欄位
    if (!(out << kills << '\n'
              << accuracy << '\n')) {
        throw HighScoreException("TowerDefenseScore::Save 寫入失敗");
    }
}

void TowerDefenseScore::Load(std::ifstream& in) {
    ScoreEntry::Load(in);
    if (!(in >> kills >> accuracy)) {
        throw HighScoreException("TowerDefenseScore::Load 讀取失敗");
    }
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string TowerDefenseScore::Format() const {
    // 純虛擬函式的實作（多型的具體表現）
    std::ostringstream oss;
    oss << playerName
        << " | Wave " << waveReached
        << " | Kills " << kills
        << " | Acc " << static_cast<int>(accuracy * 100.f) << "%";
    return oss.str();
}

// ══════════════════════════════════════════════════════════════════
//  HighScoreManager  —  資料結構 + 錯誤處理示範
// ══════════════════════════════════════════════════════════════════
void HighScoreManager::AddScore(const TowerDefenseScore& score) {
    // ── 資料結構：std::vector 累積 ──
    entries.push_back(score);

    // ── 資料結構：std::map 建索引 ──
    //    用 nameIndex 加速「同玩家是否已存在」查詢
    auto it = nameIndex.find(score.playerName);
    if (it == nameIndex.end()) {
        nameIndex[score.playerName] = static_cast<int>(entries.size()) - 1;
    }

    // 重新排序並截斷
    SortDescending();
    if (static_cast<int>(entries.size()) > MAX_ENTRIES) {
        entries.resize(MAX_ENTRIES);
    }

    // map 也跟著更新（因為排序後索引可能變動）
    nameIndex.clear();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        nameIndex[entries[i].playerName] = static_cast<int>(i);
    }
}

bool HighScoreManager::SaveToFile() noexcept(false) {
    // ── 錯誤處理：try / catch 包覆所有可能丟例外的操作 ──
    try {
        std::ofstream out(FILE_PATH, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            throw HighScoreException(std::string("無法開啟 ") + FILE_PATH + " 進行寫入");
        }

        // 第一行：總筆數
        out << entries.size() << '\n';
        if (out.fail()) {
            throw HighScoreException("寫入總筆數失敗");
        }

        // 逐筆寫入（多型：呼叫實際型別的 Save）
        for (const auto& s : entries) {
            s.Save(out);   // ← 動態分派到 TowerDefenseScore::Save
        }

        out.flush();
        if (out.fail()) {
            throw HighScoreException("檔案 flush 失敗");
        }

        return true;
    }
    catch (const HighScoreException& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "[HighScore] 未預期的例外: " << e.what() << std::endl;
        return false;
    }
    // 注意：catch (...) 可以再補一層，但本模組已涵蓋常見案例
}

bool HighScoreManager::LoadFromFile() noexcept(false) {
    try {
        std::ifstream in(FILE_PATH, std::ios::in);
        if (!in.is_open()) {
            // 第一次玩沒有檔案不算錯，直接回 false
            return false;
        }

        entries.clear();
        nameIndex.clear();

        std::size_t count = 0;
        if (!(in >> count)) {
            throw HighScoreException("無法讀取總筆數");
        }
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // 防止惡意/損毀檔案：限制上限
        if (count > MAX_ENTRIES * 4) {
            throw HighScoreException("檔案筆數異常大，已拒絕讀取");
        }

        // 逐筆讀入（多型：建構 TowerDefenseScore 物件）
        for (std::size_t i = 0; i < count; ++i) {
            TowerDefenseScore s;
            s.Load(in);   // ← 動態分派
            entries.push_back(s);
        }

        SortDescending();
        if (static_cast<int>(entries.size()) > MAX_ENTRIES) {
            entries.resize(MAX_ENTRIES);
        }

        // 重建索引（資料結構）
        for (std::size_t i = 0; i < entries.size(); ++i) {
            nameIndex[entries[i].playerName] = static_cast<int>(i);
        }

        return true;
    }
    catch (const HighScoreException& e) {
        std::cerr << e.what() << std::endl;
        entries.clear();
        nameIndex.clear();
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "[HighScore] Load 未預期的例外: " << e.what() << std::endl;
        entries.clear();
        nameIndex.clear();
        return false;
    }
}

void HighScoreManager::Clear() {
    entries.clear();
    nameIndex.clear();
}

const TowerDefenseScore* HighScoreManager::GetBest() const {
    if (entries.empty()) return nullptr;
    return &entries.front();   // 已排序
}

const TowerDefenseScore* HighScoreManager::FindByName(const std::string& name) const {
    auto it = nameIndex.find(name);
    if (it == nameIndex.end()) return nullptr;
    return &entries[it->second];
}

void HighScoreManager::SortDescending() {
    std::sort(entries.begin(), entries.end(),
        [](const TowerDefenseScore& a, const TowerDefenseScore& b) {
            return a.Compare(b) > 0;   // 大者在前
        });
}

// ══════════════════════════════════════════════════════════════════
//  DrawLeaderboard  —  GUI 整合（raylib 繪製）
// ══════════════════════════════════════════════════════════════════
void HighScoreManager::DrawLeaderboard(const Game& G, int x, int y, int w, int h) const {
    // 半透明背景框
    DrawRectangle(x, y, w, h, Color{ 6, 10, 22, 220 });
    DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 1.4f,
                          AlphaOf(COL_STAR, 180));

    // 標題
    char title[64];
    std::snprintf(title, sizeof(title), "★ 歷史最高分排行榜 ★");
    DTC(title, x + w / 2, y + 8, FS_SMALL, COL_STAR);

    // 表頭
    int rowY = y + 56;
    DTX("排名",  (float)(x + 14),     (float)rowY, FS_TINY, AlphaOf(WHITE, 160));
    DTX("玩家",  (float)(x + 70),     (float)rowY, FS_TINY, AlphaOf(WHITE, 160));
    DTX("波次",  (float)(x + 220),    (float)rowY, FS_TINY, AlphaOf(WHITE, 160));
    DTX("擊殺",  (float)(x + 320),    (float)rowY, FS_TINY, AlphaOf(WHITE, 160));
    DTX("命中",  (float)(x + 420),    (float)rowY, FS_TINY, AlphaOf(WHITE, 160));
    DrawLine(x + 10, rowY + 28, x + w - 10, rowY + 28, AlphaOf(WHITE, 60));

    if (entries.empty()) {
        // 沒有紀錄時的提示
        DTC("目前尚無紀錄", x + w / 2, y + h / 2, FS_MED, AlphaOf(WHITE, 120));
        return;
    }

    // 逐列繪製（多型示範：用基底類別參考呼叫 Format()）
    int maxShow = std::min<std::size_t>(entries.size(), MAX_ENTRIES);
    int baseY = rowY + 36;
    for (int i = 0; i < (int)maxShow; ++i) {
        const ScoreEntry& baseRef = entries[i];         // ← 基底類別參考
        // 上面這行展示「多型」：即使變數型別是 ScoreEntry&，
        // 呼叫 Format() 會動態分派到 TowerDefenseScore::Format()
        const TowerDefenseScore& s = entries[i];        // 取具體值

        int rowBase = baseY + i * 28;
        Color rowCol = (i == 0) ? COL_STAR : AlphaOf(WHITE, 200);

        char rank[8];  std::snprintf(rank,  sizeof(rank),  "%d.",  i + 1);
        char wave[8];  std::snprintf(wave,  sizeof(wave),  "%d",   s.waveReached);
        char kill[16]; std::snprintf(kill,  sizeof(kill),  "%d",   s.kills);
        char acc[16];  std::snprintf(acc,   sizeof(acc),   "%d%%", (int)(s.accuracy * 100.f));

        DTX(rank,             (float)(x + 14),  (float)rowBase, FS_TINY, rowCol);
        DTX(s.playerName.c_str(), (float)(x + 70),  (float)rowBase, FS_TINY, rowCol);
        DTX(wave,             (float)(x + 220), (float)rowBase, FS_TINY, rowCol);
        DTX(kill,             (float)(x + 320), (float)rowBase, FS_TINY, rowCol);
        DTX(acc,              (float)(x + 420), (float)rowBase, FS_TINY, rowCol);

        // 抑制未使用變數警告
        (void)baseRef;
    }

    // 底部資訊
    char footer[64];
    std::snprintf(footer, sizeof(footer), "目前局分數: %d", G.score);
    DTC(footer, x + w / 2, y + h - 26, FS_TINY, AlphaOf(COL_AI, 180));
}
