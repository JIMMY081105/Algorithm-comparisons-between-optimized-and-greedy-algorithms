#include "app/Application.h"

#include <exception>
#include <iostream>

// Entry point for the ocean-cleanup route planning simulation.
// main() stays intentionally small so startup, runtime, and shutdown concerns
// remain owned by the Application class.
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
