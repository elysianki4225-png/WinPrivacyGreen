add_library(SandboxieRecoveryIni STATIC src/ini.cpp)
target_include_directories(SandboxieRecoveryIni PUBLIC src)
target_compile_features(SandboxieRecoveryIni PUBLIC cxx_std_17)

add_executable(SandboxieRecoverySwitch WIN32
  src/main.cpp
  src/platform.cpp
  resources/app.rc
)
target_include_directories(SandboxieRecoverySwitch PRIVATE src resources)
target_compile_definitions(SandboxieRecoverySwitch PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(SandboxieRecoverySwitch PRIVATE
  SandboxieRecoveryIni shell32 comdlg32 comctl32 gdi32 user32 advapi32 ole32 uuid
)

if(MSVC)
  foreach(target SandboxieRecoveryIni SandboxieRecoverySwitch)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endforeach()
  target_link_options(SandboxieRecoverySwitch PRIVATE /MANIFEST:NO)
else()
  foreach(target SandboxieRecoveryIni SandboxieRecoverySwitch)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
  endforeach()
  target_link_options(SandboxieRecoverySwitch PRIVATE -municode -mwindows)
endif()

if(BUILD_TESTING)
  add_executable(SandboxieRecoveryIniTests tests/ini_tests.cpp)
  target_link_libraries(SandboxieRecoveryIniTests PRIVATE SandboxieRecoveryIni)
  if(MSVC)
    target_compile_options(SandboxieRecoveryIniTests PRIVATE /W4 /permissive- /utf-8)
    set_target_properties(SandboxieRecoveryIniTests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  add_test(NAME SandboxieRecoveryIniTests COMMAND SandboxieRecoveryIniTests)
  add_executable(SandboxieRecoveryFakeManager WIN32 tests/fake_sandman.cpp)
  if(MSVC)
    set_target_properties(SandboxieRecoveryFakeManager PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  else()
    target_link_options(SandboxieRecoveryFakeManager PRIVATE -municode -mwindows)
  endif()
endif()

install(TARGETS SandboxieRecoverySwitch RUNTIME DESTINATION .)
