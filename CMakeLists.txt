cmake_minimum_required(VERSION 3.20)
project(ctopp VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Dependencies (vcpkg preferred, system fallback)
# ---------------------------------------------------------------------------
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# libbpf — opt-in via system pkg-config (not vcpkg).
# HAS_LIBBPF is only defined when BOTH libbpf and bpftool are present
# (bpftool is required at build time for skeleton generation).
find_package(PkgConfig)
pkg_check_modules(LIBBPF libbpf IMPORTED_TARGET)
find_program(BPFTOOL bpftool)
if(NOT BPFTOOL)
    find_program(BPFTOOL bpftool PATHS /usr/sbin /sbin)
endif()
if(NOT LIBBPF_FOUND)
    message(WARNING "libbpf not found (pkg-config) — NetworkModel BPF loading disabled")
elseif(NOT BPFTOOL)
    message(WARNING "bpftool not found — skeleton generation disabled")
endif()

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
set(CTOPP_SOURCES
    src/main.cpp
    src/model/sys_stats_model.cpp
    src/model/sys_stats_reader.cpp
    src/model/network_model.cpp
    src/viewmodel/sys_view_model.cpp
    src/viewmodel/net_view_model.cpp
)

add_executable(ctopp ${CTOPP_SOURCES})

target_include_directories(ctopp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}        # for generated traffic_monitor.skel.h
)

target_link_libraries(ctopp PRIVATE
    imgui::imgui
    implot::implot
    glfw
    OpenGL::GL
    Threads::Threads
)

if(LIBBPF_FOUND AND BPFTOOL)
    target_compile_definitions(ctopp PRIVATE HAS_LIBBPF)
    target_link_libraries(ctopp PRIVATE PkgConfig::LIBBPF)
endif()

# ---------------------------------------------------------------------------
# BPF program (requires clang targeting bpf)
# ---------------------------------------------------------------------------
find_program(CLANG clang)
if(CLANG)
    set(BPF_SOURCE  ${CMAKE_SOURCE_DIR}/bpf/traffic_monitor.bpf.c)
    set(BPF_OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/traffic_monitor.bpf.o)

    # Multiarch include path for <asm/types.h> (e.g. /usr/include/x86_64-linux-gnu).
    set(BPF_ARCH_INCLUDE /usr/include/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu)
    if(EXISTS "${BPF_ARCH_INCLUDE}")
        set(BPF_ARCH_FLAG -I${BPF_ARCH_INCLUDE})
    else()
        set(BPF_ARCH_FLAG "")
    endif()

    add_custom_command(
        OUTPUT  ${BPF_OUTPUT}
        COMMAND ${CLANG} -target bpf -O2 -g -Wall
                -I${CMAKE_SOURCE_DIR}/include
                ${BPF_ARCH_FLAG}
                -c ${BPF_SOURCE} -o ${BPF_OUTPUT}
        DEPENDS ${BPF_SOURCE}
        COMMENT "Compiling eBPF program → traffic_monitor.bpf.o"
    )

    add_custom_target(bpf ALL DEPENDS ${BPF_OUTPUT})
else()
    message(WARNING "clang not found — eBPF program will not be compiled")
endif()

# ---------------------------------------------------------------------------
# BPF skeleton header (requires bpftool + clang)
# ---------------------------------------------------------------------------
if(BPFTOOL AND CLANG)
    set(SKEL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/traffic_monitor.skel.h)

    add_custom_command(
        OUTPUT  ${SKEL_HEADER}
        COMMAND ${BPFTOOL} gen skeleton ${BPF_OUTPUT} > ${SKEL_HEADER}
        DEPENDS ${BPF_OUTPUT}
        COMMENT "Generating BPF skeleton header → traffic_monitor.skel.h"
    )

    add_custom_target(skel ALL DEPENDS ${SKEL_HEADER})
    add_dependencies(skel bpf)
    add_dependencies(ctopp skel)
endif()

# ---------------------------------------------------------------------------
# Pipeline smoke test — validates BPF → NetworkModel → NetViewModel chain.
# Requires libbpf (system pkg-config).  Not built when libbpf is absent.
# ---------------------------------------------------------------------------
if(LIBBPF_FOUND AND BPFTOOL AND CLANG)
    add_executable(pipe_test
        test/pipeline_smoke.cpp
        src/model/network_model.cpp
        src/viewmodel/net_view_model.cpp
    )
    target_include_directories(pipe_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}        # for traffic_monitor.skel.h
    )
    target_compile_definitions(pipe_test PRIVATE HAS_LIBBPF)
    target_link_libraries(pipe_test PRIVATE
        PkgConfig::LIBBPF
        Threads::Threads
    )
    add_dependencies(pipe_test skel)
endif()
