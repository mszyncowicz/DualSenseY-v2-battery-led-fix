cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 20)
set(EXE_SOURCES "main.cpp" "cycle.cpp" "DualSense.h")
set(EXE_HEADERS "cycle.hpp" "DualSense.h")
SET(CMAKE_CXX_FLAGS_RELEASE "/Od /Zi")
SET(CMAKE_CXX_FLAGS_DEBUG "-O0")
SET(CMAKE_C_FLAGS_DEBUG "-O0")
STRING(REPLACE "-O2" "-Od" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
add_executable("DualSenseY" ${EXE_SOURCES} ${EXE_HEADERS} "DualSense.h" "miniaudio.h"    "ControllerEmulation.h" "IMMNotificationClient.h" "UDP.h" "Settings.h"  "Config.h" "MyUtils.h" "MyUtils.cpp" )
set_property(TARGET "DualSenseY" PROPERTY VS_DPI_AWARE "PerMonitor")
target_link_libraries("DualSenseY" PRIVATE fmt::fmt-header-only)
target_link_libraries("DualSenseY" PRIVATE glfw)
target_link_libraries("DualSenseY" PRIVATE OpenGL::GL)
target_link_libraries("DualSenseY" PRIVATE imgui::imgui)
target_link_libraries("DualSenseY" PRIVATE implot::implot)
target_link_libraries("DualSenseY" PRIVATE "hidapi_winapi")
target_link_libraries("DualSenseY" PRIVATE setupapi cfgmgr32)
target_link_libraries("DualSenseY" PRIVATE setupapi ViGEmClient)
target_link_libraries("DualSenseY" PRIVATE nlohmann_json)


target_set_warnings(
    TARGET
    "DualSenseY"
    ENABLE
    ${ENABLE_WARNINGS}
    AS_ERRORS
    ${ENABLE_WARNINGS_AS_ERRORS})

if(${ENABLE_LTO})
    target_enable_lto("DualSenseY" optimized)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_clang_tidy_to_target("DualSenseY")
endif()
