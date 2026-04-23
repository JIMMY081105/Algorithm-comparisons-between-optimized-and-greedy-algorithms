#include "app/Application.h"
#include "ApplicationInternal.h"

#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"
#include "environment/EnvironmentController.h"
#include "environment/SeasonProfile.h"
#include "persistence/ResultLogger.h"
#include "visualization/AnimationController.h"
#include "visualization/IsometricRenderer.h"

#include <exception>
#include <string>

void Application::handleCollectedNode(int collectedNodeId) {
    wasteSystem->markNodeCollected(collectedNodeId);

    const int nodeIndex = wasteSystem->getGraph().findNodeIndex(collectedNodeId);
    if (nodeIndex < 0) {
        return;
    }

    const WasteNode& node = wasteSystem->getGraph().getNode(nodeIndex);
    if (!node.getIsHQ()) {
        wasteSystem->getEventLog().addEvent(
            "Cleaned up garbage at " + node.getName());
    }
}

void Application::handleThemeChange(EnvironmentTheme theme) {
    if (!environmentController->setActiveTheme(theme, wasteSystem->getGraph())) {
        return;
    }

    resetMissionSession();
    wasteSystem->getEventLog().addEvent(
        std::string("Environment switched to ") + toDisplayString(theme));
}

void Application::handleCitySeasonChange(CitySeason season) {
    environmentController->setCitySeason(season, wasteSystem->getGraph());
    resetMissionSession();
    wasteSystem->getEventLog().addEvent(
        std::string("City season set to ") + toDisplayString(season));
}

void Application::refreshCityWeather() {
    if (environmentController->getActiveTheme() != EnvironmentTheme::City) {
        return;
    }

    environmentController->randomizeCityWeather(
        ApplicationInternal::buildWeatherSeed(*wasteSystem),
        wasteSystem->getGraph());
    refreshSessionAfterWeightChange();
    wasteSystem->getEventLog().addEvent("City weather refreshed");
}

void Application::generateNewDay() {
    wasteSystem->generateNewDay();
    environmentController->randomizeCityTraffic(
        ApplicationInternal::buildTrafficSeed(*wasteSystem),
        wasteSystem->getGraph());
    environmentController->randomizeCityWeather(
        ApplicationInternal::buildWeatherSeed(*wasteSystem),
        wasteSystem->getGraph());
    environmentController->applyActiveWeights(wasteSystem->getGraph());
    resetMissionSession();
}

void Application::runSelectedAlgorithm(int algorithmIndex) {
    try {
        loadMissionRoute(
            comparisonManager->runSingleAlgorithm(algorithmIndex, *wasteSystem),
            true);
    } catch (const std::exception& e) {
        logRuntimeError("Error: ", e);
    }
}

void Application::compareAllAlgorithms() {
    try {
        comparisonManager->runAllAlgorithms(*wasteSystem);

        const int bestIndex = comparisonManager->getBestAlgorithmIndex();
        if (bestIndex < 0) {
            return;
        }

        loadMissionRoute(comparisonManager->getResults()[bestIndex], false);
    } catch (const std::exception& e) {
        logRuntimeError("Comparison error: ", e);
    }
}

void Application::playOrRestartMission() {
    if (!currentResult.isValid()) {
        return;
    }

    if (animController->isFinished()) {
        replayCurrentMission();
        return;
    }

    wasteSystem->resetCollectionStatus();
    animController->play();
}

void Application::exportCurrentResult() {
    if (!currentResult.isValid()) {
        return;
    }

    try {
        const std::string filename =
            resultLogger->logCurrentResult(currentResult, *wasteSystem);
        wasteSystem->getEventLog().addEvent("Saved: " + filename);
    } catch (const std::exception& e) {
        logRuntimeError("Export error: ", e);
    }
}

void Application::exportComparisonResults() {
    try {
        const std::string filename =
            resultLogger->logComparison(*comparisonManager, *wasteSystem);
        if (!filename.empty()) {
            wasteSystem->getEventLog().addEvent("Saved: " + filename);
        }
    } catch (const std::exception& e) {
        logRuntimeError("Export error: ", e);
    }
}

void Application::resetMissionSession() {
    currentResult = RouteResult();
    currentMission = MissionPresentation();
    animController->stop();
    renderer->resetAnimation();
    comparisonManager->clearResults();
    wasteSystem->resetCollectionStatus();
}

void Application::refreshSessionAfterWeightChange() {
    if (!comparisonManager->getResults().empty()) {
        compareAllAlgorithms();
    } else {
        resetMissionSession();
    }
}

void Application::loadMissionRoute(const RouteResult& result, bool autoPlay) {
    currentResult = result;
    currentMission = environmentController->buildMissionPresentation(
        currentResult, wasteSystem->getGraph());
    animController->loadRoute(currentMission);
    wasteSystem->resetCollectionStatus();

    if (autoPlay) {
        animController->play();
    }
}

void Application::replayCurrentMission() {
    if (!currentMission.isValid()) {
        return;
    }

    wasteSystem->resetCollectionStatus();
    animController->replay();
}

void Application::logRuntimeError(const std::string& context,
                                  const std::exception& e) {
    wasteSystem->getEventLog().addEvent(context + e.what());
}

void Application::handleUIActions(const DashboardUIActions& actions) {
    if (actions.layerTogglesChanged) {
        environmentController->setLayerToggles(actions.layerToggles);
    }

    if (actions.changeTheme) {
        handleThemeChange(actions.selectedTheme);
    }

    if (actions.changeSeason) {
        handleCitySeasonChange(actions.selectedSeason);
    }

    if (actions.randomizeWeather) {
        refreshCityWeather();
    }

    if (actions.generateNewDay) {
        generateNewDay();
    }

    if (actions.roadEventsChanged) {
        environmentController->applyActiveWeights(wasteSystem->getGraph());
        refreshSessionAfterWeightChange();
    }

    if (actions.runSelectedAlgorithm && actions.algorithmToRun >= 0) {
        runSelectedAlgorithm(actions.algorithmToRun);
    }

    if (actions.compareAll) {
        compareAllAlgorithms();
    }

    if (actions.playPause) {
        playOrRestartMission();
    }

    if (actions.replay) {
        replayCurrentMission();
    }

    if (actions.exportResults) {
        exportCurrentResult();
    }

    if (actions.exportComparison) {
        exportComparisonResults();
    }
}
