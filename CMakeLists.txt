cmake_minimum_required(VERSION 3.20)
project(WinPrivacyGreen VERSION 1.0.0 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "WinPrivacyGreen is a Windows application; build it on Windows (or use build/build.sh with Zig on Linux).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(WinPrivacyGreen WIN32
  src/main.cpp
  src/ui.cpp
  src/ui_views.cpp
  src/ui_draw.cpp
  src/color.cpp
  src/json.cpp
  src/sys.cpp
  src/items.cpp
  src/backup.cpp
  src/config.cpp
  resources/app.rc
)

target_include_directories(WinPrivacyGreen PRIVATE src resources)
target_compile_definitions(WinPrivacyGreen PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(WinPrivacyGreen PRIVATE
  gdiplus dwmapi shell32 ole32 uuid oleaut32 gdi32 user32 advapi32 shlwapi comdlg32
)

if(MSVC)
  # The manifest is embedded from resources/app.rc, so stop the linker from
  # generating a second one.
  target_link_options(WinPrivacyGreen PRIVATE /MANIFEST:NO)
  target_compile_options(WinPrivacyGreen PRIVATE /W4 /permissive- /utf-8)
  set_target_properties(WinPrivacyGreen PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
else()
  target_compile_options(WinPrivacyGreen PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_link_options(WinPrivacyGreen PRIVATE -municode -mwindows -s)
endif()

install(TARGETS WinPrivacyGreen RUNTIME DESTINATION .)

# Independent portable helper for Sandboxie Plus.
include(CTest)
add_subdirectory(sandboxie-recovery)
