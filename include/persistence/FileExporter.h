#ifndef FILE_EXPORTER_H
#define FILE_EXPORTER_H

#include "core/RouteResult.h"
#include "core/WasteSystem.h"

#include <string>
#include <vector>

// Low-level export utility responsible only for writing simulation data to disk.
// Keeping file I/O here prevents the application and UI layers from knowing
// about filenames, directories, or output formatting details.
class FileExporter {
private:
    std::string outputDirectory;

    std::string generateFilename(const std::string& prefix,
                                 const std::string& extension) const;
    void ensureDirectoryExists() const;

public:
    FileExporter();
    explicit FileExporter(const std::string& outputDir);

    std::string exportSummaryTxt(const RouteResult& result,
                                 const WasteSystem& system);
    std::string exportComparisonCsv(const std::vector<RouteResult>& results,
                                    const WasteSystem& system);
    std::string exportRouteDetailsTxt(const RouteResult& result,
                                      const WasteSystem& system);
    const std::string& getOutputDirectory() const;
};

#endif // FILE_EXPORTER_H
