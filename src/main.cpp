#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <cmath>

// ---------------------------------------------------------------------------
// Network pipeline (Model → ViewModel)
// ---------------------------------------------------------------------------
#include "ctopp/model/network_model.hpp"
#include "ctopp/viewmodel/net_view_model.hpp"

#include <chrono>
#include <thread>
#include <atomic>

// File-scope network pipeline state
static ctopp::NetworkModel      g_net;
static ctopp::NetViewModel       g_vm;
static std::atomic<bool>        g_net_initialized{false};
static std::string              g_iface = "lo";
static std::string              g_net_error;
static bool                     g_net_paused = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Flatten a deque<float> to vector<float> (ImPlot consumes contiguous storage).
static void deque_to_arrays(
    const std::deque<float>& dq,
    std::vector<float>&      ys,
    std::vector<float>&      xs)
{
    ys.assign(dq.begin(), dq.end());
    xs.resize(ys.size());
    for (size_t i = 0; i < xs.size(); ++i)
        xs[i] = static_cast<float>(i);
}

// ---------------------------------------------------------------------------
// Tab: System Stats (placeholder — 同学 B finishes this independently)
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
// Tab: Network Traffic (connected to BPF → NetworkModel → NetViewModel)
// ---------------------------------------------------------------------------
static void render_net_traffic_tab() {
    if (!ImGui::BeginTabItem("Network Traffic"))
        return;

    // --- Status bar ---
    {
        ImGui::TextUnformatted("Interface:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", g_iface.c_str());

        ImGui::SameLine(0, 20);

        if (g_net_initialized) {
            if (g_net_paused) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "\xe2\x96\xb6  Paused");
                ImGui::SameLine();
                if (ImGui::SmallButton("Resume")) {
                    g_net.start();
                    g_net_paused = false;
                }
            } else {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "\xe2\x97\x86  Running");
                ImGui::SameLine();
                if (ImGui::SmallButton("Pause")) {
                    g_net.stop();
                    g_net_paused = true;
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "\xe2\x97\x87  Offline");
        }
    }

    // --- Error banner ---
    if (!g_net_initialized && !g_net_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0.3f,0.3f,1));
        ImGui::TextWrapped("Error: %s", g_net_error.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    if (g_net_initialized && !g_net_paused) {
        // Drive the ViewModel tick at 1 Hz from the UI thread
        static auto last_tick = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last_tick >= std::chrono::seconds(1)) {
            g_vm.tick();
            g_vm.set_dropped(g_net.dropped_packets());
            last_tick = now;
        }
    }

    // Read current view data (thread-safe, shared_lock inside)
    auto data = g_vm.get_data();

    // --- Dropped packet counter ---
    if (data.dropped_count > 0) {
        ImGui::SameLine(0, 20);
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                           "Dropped: %lu", (unsigned long)data.dropped_count);
    }

    ImGui::Separator();

    // ===================================================================
    // 1. Packet real-time table
    // ===================================================================
    {
        const float table_height = ImGui::GetTextLineHeightWithSpacing() * 18.0f;
        if (ImGui::BeginTable("PacketTable", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                ImVec2(-1, table_height)))
        {
            ImGui::TableSetupColumn("Time",     ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Source",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Dest",     ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Proto",    ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Length",   ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            // Use clipper for performance when the deque is large
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(data.recent_packets.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const auto& p = data.recent_packets[
                        data.recent_packets.size() - 1 - row];

                    ImGui::TableNextRow();

                    // Colour rows by protocol
                    ImVec4 row_col(1,1,1,1);
                    if      (p.protocol == "TCP")  row_col = ImVec4(0.3f,0.6f,1.0f,1.0f);
                    else if (p.protocol == "UDP")  row_col = ImVec4(0.3f,1.0f,0.4f,1.0f);
                    else if (p.protocol == "ICMP") row_col = ImVec4(1.0f,1.0f,0.3f,1.0f);

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(row_col, "%s", p.time.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(row_col, "%s", p.src.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextColored(row_col, "%s", p.dst.c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextColored(row_col, "%s", p.protocol.c_str());
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextColored(row_col, "%u", p.length);
                }
            }
            clipper.End();
            ImGui::EndTable();
        }
    }

    // ===================================================================
    // 2. Throughput line chart (Download / Upload)
    // ===================================================================
    if (!data.download_history.empty() || !data.upload_history.empty()) {
        ImGui::Text("Throughput");
        ImGui::SameLine();
        ImGui::TextDisabled("(last 60 s)");

        std::vector<float> dl_ys, dl_xs, ul_ys, ul_xs;
        deque_to_arrays(data.download_history, dl_ys, dl_xs);
        deque_to_arrays(data.upload_history,   ul_ys, ul_xs);

        if (ImPlot::BeginPlot("##ThroughputPlot", ImVec2(-1, 200),
                ImPlotFlags_NoTitle | ImPlotFlags_NoMenus))
        {
            ImPlot::SetupAxes("Time (s)", "KB/s");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, 60, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, ImGuiCond_Once);

            if (!dl_ys.empty())
                ImPlot::PlotLine("Download", dl_xs.data(), dl_ys.data(),
                                 (int)dl_ys.size());
            if (!ul_ys.empty())
                ImPlot::PlotLine("Upload",   ul_xs.data(), ul_ys.data(),
                                 (int)ul_ys.size());

            // Current values annotation
            if (!dl_ys.empty()) {
                char label[64];
                snprintf(label, sizeof(label), "DL: %.1f KB/s", data.download_kbps);
                ImPlot::Annotation(dl_xs.back(), dl_ys.back(),
                                   ImVec4(0.3f,0.6f,1.0f,1), ImVec2(0,0), false,
                                   "%s", label);
            }
            if (!ul_ys.empty()) {
                char label[64];
                snprintf(label, sizeof(label), "UL: %.1f KB/s", data.upload_kbps);
                ImPlot::Annotation(ul_xs.back(), ul_ys.back(),
                                   ImVec4(0.3f,1.0f,0.4f,1), ImVec2(0,0), false,
                                   "%s", label);
            }

            ImPlot::EndPlot();
        }
    } else {
        ImGui::TextDisabled("Throughput chart — waiting for data...");
    }

    // ===================================================================
    // 3. Top IP bar chart + Protocol pie chart (side by side)
    // ===================================================================
    {
        // Use the remaining horizontal space: half each
        float remaining = ImGui::GetContentRegionAvail().y;
        float chart_h   = std::max(remaining - 10.0f, 200.0f);
        float avail_w   = ImGui::GetContentRegionAvail().x;

        // --- Top IP bar chart (left half) ---
        if (ImPlot::BeginPlot("##TopTalkers", ImVec2(avail_w * 0.5f, chart_h),
                ImPlotFlags_NoTitle | ImPlotFlags_NoMenus))
        {
            ImPlot::SetupAxes("Bytes", "");

            if (!data.top_talkers.empty()) {
                // Build arrays from top_talkers (reverse order so #1 is at top)
                size_t n = data.top_talkers.size();
                std::vector<double> vals(n);
                std::vector<double> y_pos(n);
                std::vector<const char*> lbls(n);
                for (size_t i = 0; i < n; ++i) {
                    vals[n - 1 - i] = static_cast<double>(data.top_talkers[i].bytes);
                    y_pos[i] = static_cast<double>(i);
                    lbls[i]  = data.top_talkers[n - 1 - i].ip.c_str();
                }

                ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5, n - 0.5);
                ImPlot::SetupAxisTicks(ImAxis_Y1, y_pos.data(), (int)n, lbls.data());

                // Draw horizontal bars
                ImPlot::PlotBars("##ipbars", vals.data(), (int)n, 0.6, 0,
                                 ImPlotSpec(ImPlotProp_Flags,
                                            ImPlotBarsFlags_Horizontal));
            }
            ImPlot::EndPlot();
        }

        ImGui::SameLine();

        // --- Protocol pie chart (right half) ---
        if (ImPlot::BeginPlot("##ProtocolPie", ImVec2(avail_w * 0.5f, chart_h),
                ImPlotFlags_NoTitle | ImPlotFlags_NoMenus |
                ImPlotFlags_Equal))
        {
            ImPlot::SetupAxes(nullptr, nullptr);
            ImPlot::SetupAxisLimits(ImAxis_X1, -1, 1);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1);

            double pie_vals[3] = {data.tcp_pct, data.udp_pct, data.icmp_pct};
            const char* pie_lbls[3] = {"TCP", "UDP", "ICMP"};

            // Only draw if there's data
            if (pie_vals[0] + pie_vals[1] + pie_vals[2] > 0) {
                ImPlot::PlotPieChart(pie_lbls, pie_vals, 3, 0, 0, 0.85,
                                     "%.1f%%", 90,
                                     ImPlotSpec(ImPlotProp_Flags,
                                                ImPlotPieChartFlags_Normalize));
            }

            ImPlot::EndPlot();
        }
    }

    // --- Per-second throughput summary below charts ---
    {
        ImGui::Spacing();
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Packets/s: %-6lu  |  Download: %.1f KB/s  |  Upload: %.1f KB/s"
                 "  |  TCP: %.0f%%  UDP: %.0f%%  ICMP: %.0f%%",
                 (unsigned long)data.packet_count,
                 data.download_kbps,
                 data.upload_kbps,
                 data.tcp_pct, data.udp_pct, data.icmp_pct);
        ImGui::TextUnformatted(buf);
    }

    ImGui::EndTabItem();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Parse command-line: optional interface name
    if (argc > 1)
        g_iface = argv[1];

    // --- Initialise Network pipeline ---
    {
        g_net.set_callback([](const ctopp::PacketRecord& pkt) {
            g_vm.on_packet(pkt);
        });

        if (g_net.init(g_iface)) {
            g_net.start();
            g_net_initialized = true;
            g_net_paused = false;
            fprintf(stderr, "[main] Network pipeline OK (iface=%s)\n",
                    g_iface.c_str());
        } else {
            g_net_error = "BPF init failed — try: sudo ./ctopp <iface>";
            fprintf(stderr, "[main] Network pipeline FAILED (iface=%s)\n",
                    g_iface.c_str());
        }
    }

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

        // Root window fills the viewport
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("ctop++", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNav);

        ImGui::PopStyleVar(2);

        // Tab bar
        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            render_sys_stats_tab();
            render_net_traffic_tab();
            ImGui::EndTabBar();
        }

        ImGui::End(); // root window

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
    // Stop network pipeline first (before ImGui/GLFW teardown)
    if (g_net_initialized) {
        g_net.stop();
        fprintf(stderr, "[main] Network pipeline stopped\n");
    }

    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
