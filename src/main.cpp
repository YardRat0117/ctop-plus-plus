#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Tab: System Stats (placeholder — 同学 B fills real UI here)
// ---------------------------------------------------------------------------
static void render_sys_stats_tab() {
    if (ImGui::BeginTabItem("System Stats")) {
        ImGui::Text("System Stats panel (CPU / Memory / Disk / Network)");
        ImGui::Separator();

        // Placeholder: CPU progress bar
        static float cpu_fake = 42.0f;
        ImGui::Text("CPU Usage");
        ImGui::SameLine();
        ImGui::ProgressBar(cpu_fake / 100.0f, ImVec2(-1, 0), "42%%");

        // Placeholder: Memory progress bar
        static float mem_fake = 68.0f;
        ImGui::Text("Memory   ");
        ImGui::SameLine();
        ImGui::ProgressBar(mem_fake / 100.0f, ImVec2(-1, 0), "68%%");

        ImGui::Separator();
        ImGui::Text("CPU History (line chart placeholder)");
        // TODO: ImPlot::BeginPlot("CPU History")...

        ImGui::EndTabItem();
    }
}

// ---------------------------------------------------------------------------
// Tab: Network Traffic (placeholder — 同学 B fills real UI here)
// ---------------------------------------------------------------------------
static void render_net_traffic_tab() {
    if (ImGui::BeginTabItem("Network Traffic")) {
        ImGui::Text("Network Traffic panel (Packet table / Throughput / Top IP)");
        ImGui::Separator();

        // Placeholder: packet table
        if (ImGui::BeginTable("PacketTable", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Time");
            ImGui::TableSetupColumn("Source → Dest");
            ImGui::TableSetupColumn("Proto");
            ImGui::TableSetupColumn("Length");
            ImGui::TableHeadersRow();

            for (int i = 0; i < 10; i++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("12:34:56.%03d", i);
                ImGui::TableSetColumnIndex(1); ImGui::Text("192.168.1.%d → 10.0.0.%d", i, i+1);
                ImGui::TableSetColumnIndex(2); ImGui::Text("TCP");
                ImGui::TableSetColumnIndex(3); ImGui::Text("%d", 64 + i * 100);
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text("Throughput line chart / Top IP bar chart / Protocol pie (placeholder)");

        ImGui::EndTabItem();
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    // --- GLFW ---
    if (!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "ctop++", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "GLFW window creation failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // --- ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- ImPlot ---
    ImPlot::CreateContext();

    // --- Main loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Tab bar
        if (ImGui::BeginTabBar("MainTabs")) {
            render_sys_stats_tab();
            render_net_traffic_tab();
            ImGui::EndTabBar();
        }

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
