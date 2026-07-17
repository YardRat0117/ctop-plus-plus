#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

// Model layer
#include "ctopp/model/sys_stats_model.hpp"
#include "ctopp/model/network_model.hpp"

// ViewModel layer
#include "ctopp/viewmodel/sys_view_model.hpp"
#include "ctopp/viewmodel/net_view_model.hpp"

int main(int argc, char** argv) {
    // =====================================================================
    // Display availability check
    // =====================================================================
    bool has_display = (std::getenv("DISPLAY") != nullptr)
                    || (std::getenv("WAYLAND_DISPLAY") != nullptr);
    bool is_root    = (geteuid() == 0);

    if (!has_display) {
        // No display server at all — use offscreen to avoid QGuiApplication abort.
        std::fprintf(stderr,
            "[main] WARNING: No display server detected (DISPLAY unset).\n"
            "[main] Falling back to offscreen mode — GUI will not be visible.\n");
        setenv("QT_QPA_PLATFORM", "offscreen", 0);
    } else if (is_root) {
        // Root often lacks X11 authority when launched via bare sudo.
        // Suggest sudo -E and set offscreen fallback to prevent abort.
        std::fprintf(stderr,
            "[main] Running as root. To show the GUI window, use:\n"
            "[main]   sudo -E %s eth0\n"
            "[main] or run: xhost +SI:localuser:root   (once per session)\n"
            "[main] Falling back to offscreen mode.\n", argv[0]);
        // If Qt fails to open the display, the offscreen fallback
        // will be used instead of crashing.
        setenv("QT_QPA_PLATFORM", "xcb;offscreen", 0);
    }

    QGuiApplication app(argc, argv);

    // Parse command-line: optional interface name
    std::string iface = "lo";
    if (argc > 1)
        iface = argv[1];

    // =====================================================================
    // Model + ViewModel instances (owned here, referenced by QML)
    // =====================================================================
    ctopp::SysStatsModel  sys_model;
    ctopp::SysViewModel   sys_vm;
    ctopp::NetworkModel   net_model;
    ctopp::NetViewModel   net_vm;

    // --- System Stats pipeline ---
    {
        sys_model.set_callback([&](const ctopp::SysStatsSnapshot& snap) {
            sys_vm.on_snapshot(snap);
        });
        sys_model.start();
        std::fprintf(stderr, "[main] System stats pipeline started\n");
    }

    // --- Network pipeline ---
    bool net_ok = false;
    {
        net_model.set_callback([&](const ctopp::PacketRecord& pkt) {
            net_vm.on_packet(pkt);
        });

        if (net_model.init(iface)) {
            net_model.start();
            net_ok = true;
            std::fprintf(stderr, "[main] Network pipeline OK (iface=%s)\n",
                         iface.c_str());
        } else {
            std::fprintf(stderr, "[main] Network pipeline FAILED (iface=%s)\n",
                         iface.c_str());
        }
    }

    // --- 1 Hz tick for NetViewModel ---
    QTimer tick_timer;
    tick_timer.callOnTimeout([&]() {
        net_vm.tick();
        if (net_ok)
            net_vm.set_dropped(net_model.dropped_packets());
    });
    tick_timer.start(1000);

    // =====================================================================
    // QML engine
    // =====================================================================
    QQmlApplicationEngine engine;

    // Expose ViewModels and state to QML (MVVM: QML = View, C++ = ViewModel)
    engine.rootContext()->setContextProperty("sysVM",     &sys_vm);
    engine.rootContext()->setContextProperty("netVM",     &net_vm);
    engine.rootContext()->setContextProperty("ifaceName", QString::fromStdString(iface));
    engine.rootContext()->setContextProperty("netInitDone", net_ok);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "[main] Failed to load QML\n");
        return 1;
    }

    int ret = app.exec();

    // =====================================================================
    // Cleanup (stop background threads before static destruction)
    // =====================================================================
    if (net_ok)
        net_model.stop();
    sys_model.stop();
    std::fprintf(stderr, "[main] Cleanup complete\n");

    return ret;
}
