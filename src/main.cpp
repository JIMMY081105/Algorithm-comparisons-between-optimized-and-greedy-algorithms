// Smart Waste Clearance and Management System
// EcoRoute Solutions Sdn Bhd — Simulation & Decision-Support Tool
//
// This application simulates waste collection route planning for a
// fictitious Malaysian waste management company. It provides an isometric
// 3D-style dashboard for visualizing and comparing four different routing
// algorithms: Regular, Greedy, MST, and TSP.
//
// Built with C++17, OpenGL, GLFW, and Dear ImGui.

#include "app/Application.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        Application app;

        if (!app.init()) {
            std::cerr << "Failed to initialize the application." << std::endl;
            return 1;
        }

        app.run();
        app.shutdown();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred." << std::endl;
        return 1;
    }

    return 0;
}
