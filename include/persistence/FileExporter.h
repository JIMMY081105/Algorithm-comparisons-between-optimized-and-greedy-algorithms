#ifndef FILE_EXPORTER_H
#define FILE_EXPORTER_H

#include <string>
#include <vector>
#include "core/RouteResult.h"
#include "core/WasteSystem.h"

// Handles writing simulation results to text and CSV files.
// All file output goes through this class to keep I/O logic
// centralized and the rest of the codebase free from file concerns.
class FileExporter {
private:
    std::string outputDirectory;

    // Generate a timestamped filename like "summary_20260406_153042.txt"
    std::string generateFilename(const std::string& prefix,
                                  const std::string& extension) const;

    // Ensure the output directory exists
    void ensureDirectoryExists() const;

public:
    FileExporter();
    explicit FileExporter(const std::string& outputDir);

    // Export a single route result as a human-readable TXT summary
    std::string exportSummaryTxt(const RouteResult& result,
                                  const WasteSystem& system);

    // Export multiple algorithm results as a CSV comparison table
    std::string exportComparisonCsv(const std::vector<RouteResult>& results,
                                     const WasteSystem& system);

    // Export detailed route information for one algorithm
    std::string exportRouteDetailsTxt(const RouteResult& result,
                                       const WasteSystem& system);

    // Get the output directory path
    std::string getOutputDirectory() const;
};

#endif // FILE_EXPORTER_H
