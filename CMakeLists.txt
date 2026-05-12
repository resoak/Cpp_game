cmake_minimum_required(VERSION 3.16)
project(LogicGateDefense CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LGD_DEBUG_WAVE_JUMP "Enable removable debug hotkeys for route expansion wave tests" OFF)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Allow raylib 5.0 FetchContent with CMake 4.x")
endif()

# ── 查找 raylib ──────────────────────────────────────────────────
find_package(raylib QUIET)
if (NOT raylib_FOUND)
    # 若系統未安裝 raylib，嘗試從 FetchContent 下載
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

# ── 所有源碼檔案 ─────────────────────────────────────────────────
set(SOURCES
    main.cpp
    font.cpp
    globals.cpp
    path.cpp
    tower_data.cpp
    game.cpp
    tutorial.cpp
    logic.cpp
    draw_world.cpp
    draw_ui.cpp
    draw_screens.cpp
    input.cpp
)

add_executable(lgd ${SOURCES})

target_include_directories(lgd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lgd PRIVATE raylib)
if (LGD_DEBUG_WAVE_JUMP)
    target_compile_definitions(lgd PRIVATE LGD_DEBUG_WAVE_JUMP=1)
endif()

# ── 執行時資源 ────────────────────────────────────────────────────
# 遊戲需要字型檔顯示中文；建置後自動複製到執行檔旁，方便直接執行。
add_custom_command(TARGET lgd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/font.ttf
        $<TARGET_FILE_DIR:lgd>/font.ttf
)

install(TARGETS lgd RUNTIME DESTINATION bin)
install(FILES font.ttf DESTINATION bin)

# ── 平台特定連結 ──────────────────────────────────────────────────
if (UNIX AND NOT APPLE)
    target_link_libraries(lgd PRIVATE m pthread dl)
endif()
if (APPLE)
    target_link_libraries(lgd PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()
