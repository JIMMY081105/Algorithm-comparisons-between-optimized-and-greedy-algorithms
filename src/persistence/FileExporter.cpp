#include "persistence/FileExporter.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
std::tm buildLocalTimestamp(std::time_t now) {
    std::tm timestamp{};
#ifdef _WIN32
    localtime_s(&timestamp, &now);
#else
    localtime_r(&now, &timestamp);
#endif
    return timestamp;
}

std::ofstream openOutputFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("FileExporter: cannot open file " + filename);
    }
    return file;
}

const WasteNode* findNodeById(const WasteSystem& system, int nodeId) {
    const int nodeIndex = system.getGraph().findNodeIndex(nodeId);
    if (nodeIndex < 0) {
        return nullptr;
    }
    return &system.getGraph().getNode(nodeIndex);
}

void writeSimulationMetadata(std::ostream& output, const WasteSystem& system) {
    output << "Day: " << system.getDayNumber() << "\n";
    output << "Seed: " << system.getCurrentSeed() << "\n";
    output << "Collection Threshold: " << system.getCollectionThreshold() << "%\n";
}

void writeRouteMetrics(std::ostream& output, const RouteResult& result) {
    output << std::fixed << std::setprecision(2);
    output << "Total Distance:    " << result.totalDistance << " km\n";
    output << "Travel Time:       " << result.travelTime << " hours\n";
    output << "Fuel Cost:         RM " << result.fuelCost << "\n";
    output << "  Base Pay:        RM " << result.basePay << "\n";
    output << "  Per-km Bonus:    RM " << result.perKmBonus << "\n";
    output << "  Efficiency Bonus:RM " << result.efficiencyBonus << "\n";
    output << "  Wage Total:      RM " << result.wageCost << "\n";
    output << "Toll Cost:         RM " << result.tollCost << "\n";
    if (!result.tollsCrossed.empty()) {
        output << "  Tolls crossed: ";
        for (std::size_t i = 0; i < result.tollsCrossed.size(); ++i) {
            if (i > 0) output << ", ";
            output << result.tollsCrossed[i];
        }
        output << "\n";
    }
    output << "Total Cost:        RM " << result.totalCost << "\n";
    output << "Waste Collected:   " << result.wasteCollected << " kg\n";
    output << "Compute Time:      " << result.runtimeMs << " ms\n";
}

void writeRouteOrder(std::ostream& output,
                     const RouteResult& result,
                     const WasteSystem& system) {
    output << "Route Order:\n";
    for (std::size_t visitIndex = 0; visitIndex < result.visitOrder.size(); ++visitIndex) {
        const WasteNode* node = findNodeById(system, result.visitOrder[visitIndex]);
        if (!node) {
            continue;
        }

        output << "  " << (visitIndex + 1) << ". " << node->getName();
        if (!node->getIsHQ()) {
            output << " (" << node->getWasteLevel() << "%)";
        }
        output << "\n";
    }
}

void writeComparisonMetadata(std::ostream& output, const WasteSystem& system) {
    output << "\nDay," << system.getDayNumber() << "\n";
    output << "Seed," << system.getCurrentSeed() << "\n";
    output << "Threshold," << system.getCollectionThreshold() << "%\n";
}

void writeRouteSegments(std::ostream& output,
                        const RouteResult& result,
                        const WasteSystem& system) {
    output << "Step-by-step route:\n";
    output << "-------------------\n";
    for (std::size_t step = 0; step + 1 < result.visitOrder.size(); ++step) {
        const WasteNode* fromNode = findNodeById(system, result.visitOrder[step]);
        const WasteNode* toNode = findNodeById(system, result.visitOrder[step + 1]);
        if (!fromNode || !toNode) {
            continue;
        }

        const float segmentDistance =
            system.getGraph().getDistance(fromNode->getId(), toNode->getId());
        output << "  " << (step + 1) << ". "
               << fromNode->getName() << " -> " << toNode->getName()
               << "  [" << segmentDistance << " km]\n";
    }
}

void writeWasteLevels(std::ostream& output, const WasteSystem& system) {
    output << "\nWaste at each location:\n";
    output << "-----------------------\n";
    for (int nodeIndex = 0; nodeIndex < system.getGraph().getNodeCount(); ++nodeIndex) {
        const WasteNode& node = system.getGraph().getNode(nodeIndex);
        if (node.getIsHQ()) {
            continue;
        }

        output << "  " << node.getName() << ": "
               << node.getWasteLevel() << "% ("
               << node.getWasteAmount() << " kg)\n";
    }
}
} // namespace

FileExporter::FileExporter() : outputDirectory("output") {}

FileExporter::FileExporter(const std::string& outputDir)
    : outputDirectory(outputDir) {}

std::string FileExporter::generateFilename(const std::string& prefix,
                                           const std::string& extension) const {
    const std::tm timestamp = buildLocalTimestamp(std::time(nullptr));

    std::ostringstream filename;
    filename << outputDirectory << "/"
             << prefix << "_"
             << std::setfill('0')
             << (timestamp.tm_year + 1900)
             << std::setw(2) << (timestamp.tm_mon + 1)
             << std::setw(2) << timestamp.tm_mday << "_"
             << std::setw(2) << timestamp.tm_hour
             << std::setw(2) << timestamp.tm_min
             << std::setw(2) << timestamp.tm_sec
             << "." << extension;
    return filename.str();
}

void FileExporter::ensureDirectoryExists() const {
    try {
        std::filesystem::create_directories(outputDirectory);
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error(
            "FileExporter: cannot create output directory - " +
            std::string(e.what()));
    }
}

std::string FileExporter::exportSummaryTxt(const RouteResult& result,
                                           const WasteSystem& system) {
    ensureDirectoryExists();
    const std::string filename = generateFilename("summary", "txt");
    std::ofstream file = openOutputFile(filename);

    file << "============================================\n";
    file << "  Ocean Cleanup System - Route Summary\n";
    file << "  EcoRoute Solutions\n";
    file << "============================================\n\n";

    writeSimulationMetadata(file, system);
    file << "\nAlgorithm: " << result.algorithmName << "\n";
    file << "--------------------------------------------\n";

    writeRouteMetrics(file, result);
    file << "\n";
    writeRouteOrder(file, result, system);

    file << "\n============================================\n";
    file << "  Generated by Ocean Cleanup System\n";
    file << "============================================\n";

    return filename;
}

std::string FileExporter::exportComparisonCsv(const std::vector<RouteResult>& results,
                                              const WasteSystem& system) {
    ensureDirectoryExists();
    const std::string filename = generateFilename("comparison", "csv");
    std::ofstream file = openOutputFile(filename);

    file << "Algorithm,Distance (km),Travel Time (h),Fuel Cost (RM),"
         << "Base Pay (RM),Per-km Bonus (RM),Efficiency Bonus (RM),"
         << "Wage Total (RM),Toll Cost (RM),Total Cost (RM),"
         << "Waste Collected (kg),Runtime (ms)\n";

    file << std::fixed << std::setprecision(2);
    for (const RouteResult& result : results) {
        file << result.algorithmName << ","
             << result.totalDistance << ","
             << result.travelTime << ","
             << result.fuelCost << ","
             << result.basePay << ","
             << result.perKmBonus << ","
             << result.efficiencyBonus << ","
             << result.wageCost << ","
             << result.tollCost << ","
             << result.totalCost << ","
             << result.wasteCollected << ","
             << result.runtimeMs << "\n";
    }

    writeComparisonMetadata(file, system);

    return filename;
}

std::string FileExporter::exportRouteDetailsTxt(const RouteResult& result,
                                                const WasteSystem& system) {
    ensureDirectoryExists();
    const std::string filename = generateFilename("route_detail", "txt");
    std::ofstream file = openOutputFile(filename);

    file << "Route Details - " << result.algorithmName << "\n";
    file << "Day " << system.getDayNumber() << "\n\n";
    file << std::fixed << std::setprecision(2);

    writeRouteSegments(file, result, system);

    file << "\nTotal: " << result.totalDistance << " km\n";
    writeWasteLevels(file, system);

    return filename;
}

const std::string& FileExporter::getOutputDirectory() const {
    return outputDirectory;
}
