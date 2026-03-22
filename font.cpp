// ================================================================
//  font.cpp — 中文字型載入實作
// ================================================================
#include "font.h"
#include <vector>
#include <cstdio>

Font gFont;
bool gFontLoaded = false;

// ══════════════════════════════════════════════════════════════════
//  LoadChineseFont
// ══════════════════════════════════════════════════════════════════
void LoadChineseFont() {
    // 建立字碼點清單（ASCII + 中文 + 標點 + 符號）
    static std::vector<int> cp;
    cp.clear();
    for (int i = 32;     i < 127;     i++) cp.push_back(i);
    for (int i = 0x4E00; i <= 0x9FFF; i++) cp.push_back(i);
    for (int i = 0x3000; i <= 0x303F; i++) cp.push_back(i);
    for (int i = 0xFF01; i <= 0xFF60; i++) cp.push_back(i);
    for (int i = 0x2000; i <= 0x206F; i++) cp.push_back(i);
    for (int i = 0x2190; i <= 0x21FF; i++) cp.push_back(i);
    for (int i = 0x25A0; i <= 0x25FF; i++) cp.push_back(i);

    // 按優先順序嘗試各作業系統的字型路徑
    const char* paths[] = {
        "font.ttf", "font.ttc", "font.otf",
        "C:/Windows/Fonts/msjh.ttc",
        "C:/Windows/Fonts/kaiu.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        nullptr
    };

    for (int i = 0; paths[i]; i++) {
        FILE* f = fopen(paths[i], "rb");
        if (!f) continue;
        fclose(f);

        gFont = LoadFontEx(paths[i], 60, cp.data(), (int)cp.size());
        if (gFont.texture.id != 0) {
            gFontLoaded = true;
            SetTextureFilter(gFont.texture, TEXTURE_FILTER_BILINEAR);
            break;
        }
    }

    if (!gFontLoaded) {
        gFont = GetFontDefault();
    }
}
