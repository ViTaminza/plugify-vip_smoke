# vip_smoke — Plugify C++ plugin: port of the Pisex VIP module "[VIP] Colored Smoke".
#
#   plugify-vip_smoke  the plugin shared library (entry "bin/vip_smoke" -> bin/libvip_smoke.so)

set(VIP_SMOKE_VERSION "1.0.2")

# Binding globals the plugify module fills from the plugins declared as dependencies:
#   __s2sdk_*   (s2sdk_bindings)
#   __vip_*     (vip consumer bindings, generated into plugins/vip/include/external/plugify/include)
if(NOT TARGET vip_bindings)
    message(FATAL_ERROR "vip_smoke needs the generated vip consumer bindings (plugins/vip/CMakeLists.txt, target vip_bindings)")
endif()

add_library(plugify-vip_smoke SHARED
        src/plugin.cpp
        $<TARGET_OBJECTS:s2sdk_bindings>
        $<TARGET_OBJECTS:vip_bindings>
)
target_include_directories(plugify-vip_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# SYSTEM: warnings from the generated vip/s2sdk/plg headers must not fail -Werror on our sources.
target_include_directories(plugify-vip_smoke SYSTEM PRIVATE
        "${CMAKE_SOURCE_DIR}/plugins/vip/include"
        "${CMAKE_SOURCE_DIR}/plugins/vip/include/external/plugify/include"
        "${CMAKE_SOURCE_DIR}/sdk/include")
target_link_libraries(plugify-vip_smoke PRIVATE
        pisexcompat
        -static-libstdc++ -static-libgcc
)
target_compile_options(plugify-vip_smoke PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(plugify-vip_smoke PRIVATE
        _GLIBCXX_USE_CXX11_ABI=1
        PLUGIFY_MODULE="vip_smoke"
        PLUGIFY_IS_DEBUG=0
        PLUGIFY_IS_RELEASE=1
)
set_target_properties(plugify-vip_smoke PROPERTIES OUTPUT_NAME vip_smoke)

if(UNIX AND NOT APPLE)
    target_link_options(plugify-vip_smoke PRIVATE
            "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/sym/version_script.lds")
endif()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/vip_smoke.pplugin.in"
        "${CMAKE_CURRENT_BINARY_DIR}/vip_smoke.pplugin"
)

# --- tests -------------------------------------------------------------------------
# The "smoke_color" parsing (src/smoke_color.hpp) is the only engine-free logic of this module;
# the runtime lands in build/tests so ./test.sh finds it.
add_executable(vip_smoke_tests tests/vip_smoke_tests.cpp)
target_include_directories(vip_smoke_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(vip_smoke_tests PRIVATE -Wall -Wextra -Werror)
set_target_properties(vip_smoke_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
