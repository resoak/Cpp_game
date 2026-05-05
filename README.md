# Logic Gate Defense

`Logic Gate Defense` 是一款以邏輯閘與神經網路為主題的 C++ 塔防遊戲。玩家需要在地圖上放置感測器、邏輯閘、感知器與砲塔，將訊號連成防禦網路，阻止病毒敵人突破並攻擊 CPU 核心。

遊戲使用 [raylib](https://www.raylib.com/) 繪製畫面與處理輸入，透過 CMake 建置。若系統沒有安裝 raylib，`CMakeLists.txt` 會使用 `FetchContent` 自動下載 raylib 5.0。

## 遊戲特色

- 邏輯閘塔防：用 `SENSOR -> AND/OR/XOR/NAND/PERCEPTRON -> CANNON` 建立可運作的訊號鏈。
- 感知器學習：感知器會在波次結束後根據戰鬥結果調整 `w1`、`w2` 與 `bias`。
- AI 敵情系統：敵人會統計兵種與路線存活率，後續波次會逐步適應玩家防線。
- 防禦建議 NN：右側資訊欄會顯示神經網路對防禦補強方向的建議。
- 威脅熱圖：擊殺位置會累積成熱圖，協助判斷火力集中區與薄弱區。
- 動態路線：每 3 波會輪換路線，Wave 8 起開啟雙路徑進攻。
- 波次事件：包含蜂群、裝甲洪流、霧戰、精英突擊、護衛 Boss、再生軍、神速衝鋒、通訊中斷、變異體與圍城艦。
- 主動技能：每種非 CPU 元件都有可觸發技能，例如 EMP、超頻連射、全域標記與破甲反轉。

## 系統需求

- CMake 3.16 以上
- 支援 C++17 的編譯器
  - Windows：MSVC、MinGW 或 Clang
  - Linux：GCC 或 Clang
  - macOS：Apple Clang
- Git 與網路連線：首次建置且本機沒有 raylib 時，CMake 需要下載 raylib

## 建置與執行

### Windows / Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

執行檔名稱為 `lgd`。

Linux 或 macOS：

```bash
./build/lgd
```

Windows 使用 MSVC 產生器時：

```powershell
.\build\Release\lgd.exe
```

Windows 使用單組態產生器，例如 MinGW 時：

```powershell
.\build\lgd.exe
```

## 操作方式

| 操作 | 功能 |
| --- | --- |
| `Enter` / `Space` | 在主選單開始遊戲；建造階段發動下一波 |
| 滑鼠點擊左側元件 | 選擇要放置的元件，再點擊地圖空格放置 |
| 滑鼠點擊已有元件 | 選取元件並查看資訊 |
| `C` | 選取元件後進入連線模式，再點擊目標元件建立或取消連線 |
| `U` | 升級選取元件 |
| `Q` | 發動選取元件的主動技能 |
| `Delete` | 移除選取元件，退還 50% 基礎費用 |
| `T` | 切換威脅熱圖 |
| `A` | 切換 AI 顧問提示 |
| `P` | 暫停或繼續 |
| `H` | 開啟或關閉說明 |
| `Esc` | 取消放置或連線；說明頁中可關閉說明 |
| `F11` | 切換無邊框全螢幕 |

TRAINING 階段可按 `1`、`2`、`3`，或點擊左側選項，選擇一項波次結束獎勵。若倒數結束前未選擇，遊戲會自動套用第 1 項。

## 元件說明

| 元件 | 代號 | 費用 | 作用 |
| --- | --- | ---: | --- |
| 感測器 | `SEN` | 40 CR | 偵測範圍內敵人，輸出 0 到 1 的訊號 |
| 感知器 | `PCT` | 90 CR | 以加權求和與 Sigmoid 輸出連續訊號，可跨波學習 |
| AND 閘 | `AND` | 60 CR | 全部輸入大於 0.5 才輸出，對裝甲敵人有加成 |
| OR 閘 | `OR` | 60 CR | 任一輸入大於 0.5 即輸出，適合大量雜兵 |
| XOR 閘 | `XOR` | 60 CR | 恰好一個輸入大於 0.5 時輸出，對精英敵人有效 |
| NAND 閘 | `NND` | 70 CR | 非 AND 邏輯，後期可用來反制裝甲 |
| 砲塔 | `GUN` | 80 CR | 訊號大於 0.5 時自動攻擊範圍內敵人 |
| CPU | `CPU` | 0 CR | 核心目標，敵人抵達後會扣生命與 CPU HP |

連線限制：

- 砲塔是終端節點，不能輸出。
- CPU 不能被連入或連出。
- 感測器不能被連入。
- 連線不能形成迴圈。
- 單一元件最多可輸出 8 條連線。

## 基本玩法建議

1. 先放置感測器覆蓋路線，再把感測器訊號接到邏輯閘或感知器。
2. 將邏輯閘或感知器輸出接到砲塔，砲塔收到大於 0.5 的訊號才會開火。
3. 波次結束後觀察右側的敵情分析、AI 建議與威脅熱圖，補強高壓路段。
4. Wave 8 後會出現雙路徑，避免所有火力只集中在單一路線。
5. 面對裝甲或圍城艦時，優先使用 AND / NAND 與破甲相關技能。

## 專案結構

| 檔案 | 說明 |
| --- | --- |
| `main.cpp` | 程式進入點、視窗初始化、主迴圈與縮放輸出 |
| `CMakeLists.txt` | CMake 專案設定、raylib 依賴與目標 `lgd` |
| `constants.h` | 解析度、地圖、顏色、列舉與塔資料宣告 |
| `tower_data.cpp` | 各元件的費用、描述、升級資料 |
| `structs.h` | 塔、敵人、子彈、粒子、AI 與學習資料結構 |
| `game.h` / `game.cpp` | 遊戲狀態、初始化、升級、訊息、AI 提示與工具函式 |
| `logic.h` / `logic.cpp` | 訊號傳播、戰鬥、波次、敵人生成、技能與更新邏輯 |
| `input.h` / `input.cpp` | 鍵盤與滑鼠輸入 |
| `path.h` / `path.cpp` | 路線預設、主副路徑建置與路徑查詢 |
| `draw.h` | 繪圖函式宣告與共用繪圖工具 |
| `draw_world.cpp` | 地圖、路徑、敵人、砲彈、粒子與世界物件繪製 |
| `draw_ui.cpp` | 左右側面板、上方狀態列與底部資訊列 |
| `draw_screens.cpp` | 主選單、說明頁與遊戲結束畫面 |
| `font.h` / `font.cpp` | 中文字型載入與文字繪製輔助 |
| `globals.h` / `globals.cpp` | 全域縮放、偏移與震動畫面狀態 |
| `font.ttf` | 內建中文字型資源 |

## 常見問題

### CMake 找不到 raylib

一般情況下不需要手動安裝 raylib，CMake 會自動下載。若下載失敗，請確認 Git 與網路連線可用，或先在系統中安裝 raylib。

### Linux 編譯時缺少 OpenGL / X11 相關函式庫

raylib 在 Linux 上通常需要 OpenGL、X11 與音訊相關開發套件。以 Debian / Ubuntu 為例，可先安裝常見依賴：

```bash
sudo apt update
sudo apt install build-essential cmake git libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
```

### 中文顯示異常

專案會優先載入資料夾內的 `font.ttf`。若字型檔遺失，程式會嘗試載入系統中文字型；仍失敗時會退回 raylib 預設字型，中文可能無法完整顯示。
