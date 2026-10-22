cmake_minimum_required(VERSION 3.15)
project(DAFlash LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler options for MinGW
if(MINGW)
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type -O2)
    # Statically link libgcc and libstdc++ so the output binaries are standalone
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -mwindows")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/include
)

# MinHook submodule
add_subdirectory(third_party/minhook)

# Target 1: DAFlashHook (64-bit DLL injected into WeaselServer.exe)
add_library(DAFlashHook SHARED
    src/hook/HookMain.cpp
    src/hook/WeaselHook.cpp
)
target_link_libraries(DAFlashHook PRIVATE
    minhook
    user32
    gdi32
    d2d1
    dwrite
    ole32
    shlwapi
)
set_target_properties(DAFlashHook PROPERTIES PREFIX "" OUTPUT_NAME "DAFlashHook")

# Target 2: DAFlash (Overlay window, physics particle engine, injector, tray UI)
add_executable(DAFlash
    src/app/main.cpp
    src/app/OverlayWindow.cpp
    src/app/D2DRenderer.cpp
    src/app/ParticleManager.cpp
    src/app/Injector.cpp
    src/app/InputMonitor.cpp
)
target_link_libraries(DAFlash PRIVATE
    user32
    gdi32
    d2d1
    dwrite
    ole32
    oleaut32
    uuid
    winmm
    shell32
    dwmapi
)
set_target_properties(DAFlash PROPERTIES OUTPUT_NAME "DAFlash")
