# CMakeList.txt : fluid_renderer の CMake プロジェクト。ソースを含めて、次を定義します:
# プロジェクト専用ロジックはこちらです。
#

# ソースをこのプロジェクトの実行可能ファイルに追加します。
add_executable (fluid_renderer "main.cpp" "main.h" "sph.h" "sph.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET fluid_renderer PROPERTY CXX_STANDARD 20)
endif()

# MSVC on Japanese Windows reads source files as CP932 by default.
# /utf-8 tells the compiler to treat both source and execution charset as UTF-8.
if (MSVC)
  target_compile_options(fluid_renderer PRIVATE /utf-8)
endif()

# TODO: テストを追加し、必要な場合は、ターゲットをインストールします。
