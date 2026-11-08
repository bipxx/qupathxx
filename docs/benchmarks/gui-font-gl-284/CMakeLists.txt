cmake_minimum_required(VERSION 3.24)
project(qpx_font_gl_storage LANGUAGES CXX)
set(root "C:/Users/HEQ/Projects/qupathxx")
set(source "${root}/build/ci/scientific-grid-159")
set(imgui "${root}/build/gui-fonts-282/inputs/imgui193")
set(glfw "${root}/build/cp312-cp312-win_amd64/_deps/glfw-src")
add_executable(font_gl_probe probe.cpp)
target_compile_features(font_gl_probe PRIVATE cxx_std_17)
target_compile_definitions(font_gl_probe PRIVATE IMGUI_USE_WCHAR32)
target_compile_options(font_gl_probe PRIVATE /utf-8)
target_include_directories(font_gl_probe PRIVATE "${source}/cpp" "${imgui}" "${imgui}/backends" "${glfw}/include")
target_link_libraries(font_gl_probe PRIVATE
  "${root}/build/gui-font-delivery-283/final-products/Release/qupathxx_gui.lib"
  "${root}/build/gui-font-delivery-283/final-products/Release/qupathxx_core.lib"
  "${root}/build/ci/gridfrontends167-package/_deps/glfw-build/src/Release/glfw3.lib"
  opengl32 user32 gdi32 shell32 psapi bcrypt)
add_custom_command(TARGET font_gl_probe POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:font_gl_probe>/fonts"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${root}/build/gui-font-delivery-283/final-products/Release/fonts/NotoSansCJKsc-Regular.otf"
    "${root}/build/gui-font-delivery-283/final-products/Release/fonts/NotoEmoji.ttf"
    "$<TARGET_FILE_DIR:font_gl_probe>/fonts")
