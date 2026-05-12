# 邏輯閘防禦戰（Logic Gate Defense）

一款使用 **C++17 + raylib** 製作的邏輯閘塔防遊戲。玩家需要部署 Sensor、AND、OR、XOR、NAND、Cannon、Perceptron 等元件，建立訊號網路、升級節點、抵擋敵人波次並守住 CPU 核心。

遊戲特色包含神經網路訊號傳播、感知器學習、Boss 狀態機、威脅熱圖、雙路徑進攻、波次事件與主動技能系統。

## 專案需求

請先安裝以下工具：

- C++17 編譯器
  - Windows：MinGW-w64、MSVC 或 clang
  - macOS：Apple Clang / Xcode Command Line Tools
  - Linux：GCC 或 Clang
- CMake 3.16 以上
- Git（用來下載專案）

本專案使用 [raylib](https://www.raylib.com/)；如果系統沒有預先安裝 raylib，CMake 會透過 `FetchContent` 自動下載 raylib 5.0。第一次編譯時可能需要網路連線。

## 下載專案

使用 Git 下載：

```bash
git clone <你的 GitHub 專案網址>
cd project
```

使用 Opencode（下載到桌面）：

1. 在 Opencode 輸入以下 prompt：

   ```text
   請使用 git clone 將 git@github.com:resoak/Cpp_game.git 下載到我的桌面資料夾，使用預設專案資料夾名稱即可。
   ```

2. 下載完成後，在終端機進入專案：

   - macOS / Linux：`cd ~/Desktop/<專案資料夾名稱>`
   - Windows（PowerShell）：`cd $env:USERPROFILE\Desktop\<專案資料夾名稱>`

如果你是直接從 GitHub 網頁下載 ZIP：

1. 點選 **Code** → **Download ZIP**。
2. 解壓縮 ZIP。
3. 在解壓縮後的資料夾中開啟終端機。

> 請保留根目錄的 `font.ttf`，遊戲會用它顯示中文文字。

## 編譯方式

在專案根目錄執行：

```bash
cmake -S . -B build
cmake --build build
```

Release 編譯可使用：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

> Windows 使用 Visual Studio 產生器時，`--config Release` 會決定輸出到 `build/Release/`；MinGW 或 Ninja 通常會直接輸出到 `build/`。

## 執行遊戲

依照你的平台與 CMake 產生器，執行檔位置可能不同。

Windows（Visual Studio / 多組態產生器）：

```powershell
.\build\Debug\lgd.exe
```

或 Release：

```powershell
.\build\Release\lgd.exe
```

Windows（MinGW / Ninja）可能是：

```powershell
.\build\lgd.exe
```

macOS / Linux：

```bash
./build/lgd
```

建置完成後，CMake 會自動把 `font.ttf` 複製到執行檔旁邊，方便直接執行。

## 操作說明

| 操作 | 功能 |
| --- | --- |
| `Enter` / `Space` | 開始遊戲、在建造階段發動下一波 |
| 滑鼠點擊左側按鈕 | 選擇要放置的元件 |
| 滑鼠點擊地圖空格 | 放置元件 |
| 滑鼠點擊已放置元件 | 選取元件 |
| `C` | 進入連線模式，再點擊目標建立/取消連線 |
| `U` | 升級選取的元件 |
| `Q` | 發動選取元件的主動技能 |
| `Delete` | 移除選取元件並退還 50% 成本 |
| `T` | 切換威脅熱圖 |
| `A` | 切換 AI 顧問提示 |
| `P` | 暫停 / 繼續 |
| `H` | 開啟 / 關閉說明頁 |
| `Esc` | 關閉說明或取消目前模式 |
| `F11` | 切換無邊框全螢幕 |
| `R` | 重新開始 |

## 遊戲玩法簡介

1. 在左側面板選擇元件，放到地圖空格上。
2. 使用 `C` 讓元件互相連線，建立訊號網路。
3. 使用 Cannon 等攻擊元件消滅敵人，避免敵人抵達 CPU。
4. 每波結束後可透過訓練獎勵強化防線。
5. 善用威脅熱圖、AI 顧問提示與 Perceptron 學習資訊調整策略。

## 專案結構

```text
.
├── CMakeLists.txt      # CMake 建置設定，輸出 lgd
├── main.cpp            # 程式入口、主迴圈、視窗與渲染流程
├── game.h / game.cpp   # Game 狀態、事件、升級、重置與工具函式
├── logic.h / logic.cpp # 戰鬥、波次、敵人、技能與更新邏輯
├── input.h / input.cpp # 鍵盤與滑鼠輸入
├── draw.h              # 繪圖函式宣告
├── draw_world.cpp      # 地圖、塔、敵人、子彈與特效繪製
├── draw_ui.cpp         # 左右面板、頂部與底部 UI
├── draw_screens.cpp    # 主選單、說明頁、結束畫面
├── path.h / path.cpp   # 路徑預設與路線建構
├── constants.h         # 解析度、版面、顏色、列舉與塔資料宣告
├── structs.h           # 塔、敵人、子彈、AI、熱圖等資料結構
├── tower_data.cpp      # 各類元件的名稱、價格、描述與升級資料
├── font.h / font.cpp   # 中文字型載入
└── font.ttf            # 中文顯示用字型資源
```

## 常見問題

### 找不到 `cmake`

請先安裝 CMake，並確認終端機能執行：

```bash
cmake --version
```

### CMake 下載 raylib 失敗

第一次建置需要網路下載 raylib。如果網路受限，可以改成先在系統安裝 raylib，讓 `find_package(raylib)` 找到本機版本。

### 中文顯示不正常

請確認 `font.ttf` 存在於專案根目錄，且執行檔旁邊也有一份 `font.ttf`。使用 CMake 建置時會自動複製。

### Windows 執行檔在哪裡？

如果你使用 Visual Studio 產生器，執行檔通常在：

```powershell
.\build\Debug\lgd.exe
.\build\Release\lgd.exe
```

如果你使用 MinGW 或 Ninja，通常在：

```powershell
.\build\lgd.exe
```

## 開發備註

- 目前專案沒有自動化測試；修改遊戲邏輯後建議至少重新編譯並手動開啟主選單確認。
- `build/`、IDE 設定與編譯產物已加入 `.gitignore`，不要把本機建置輸出提交到版本庫。
- 根目錄的 `main.exe` 屬於舊的本機編譯產物；之後建議改用 `build/` 目錄產生執行檔。
