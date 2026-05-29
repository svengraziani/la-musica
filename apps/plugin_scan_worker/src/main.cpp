#include "lamusica/plugin_host/PluginScanWorker.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view{argv[1]} == "--mock-probe") {
        return lamusica::plugin_host::runMockPluginScanWorkerMode(argv[2]);
    }

    std::cerr << "usage: lamusica_plugin_scan_worker --mock-probe <valid|invalid|crash|hang>\n";
    return 64;
}
