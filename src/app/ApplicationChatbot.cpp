#include "app/Application.h"

#include "ai/ChatbotService.h"
#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"
#include "environment/EnvironmentController.h"
#include "visualization/ChatbotPanel.h"

#include <sstream>
#include <unordered_set>
#include <vector>

namespace {

std::string invalidRecommendationSummary(const std::string& reason) {
    return "route ignored (" + reason + ")";
}

std::string validateRecommendedRoute(const std::vector<int>& route,
                                     const WasteSystem& system) {
    const MapGraph& graph = system.getGraph();
    const int depotNodeId = graph.getHQNode().getId();

    if (route.size() < 2) {
        return "too short";
    }
    if (route.front() != depotNodeId || route.back() != depotNodeId) {
        return "must start and end at HQ";
    }

    const std::vector<int> eligibleNodes = system.getEligibleNodes();
    std::unordered_set<int> eligibleSet(eligibleNodes.begin(), eligibleNodes.end());
    std::unordered_set<int> visitedEligibleNodes;

    for (std::size_t index = 0; index < route.size(); ++index) {
        const int nodeId = route[index];
        if (graph.findNodeIndex(nodeId) < 0) {
            return "contains invalid node ids";
        }

        if (nodeId == depotNodeId) {
            if (index != 0 && index + 1 != route.size()) {
                return "revisits HQ before finishing";
            }
            continue;
        }

        if (eligibleSet.count(nodeId) == 0) {
            return "contains nodes below the threshold";
        }

        if (!visitedEligibleNodes.insert(nodeId).second) {
            return "repeats collection nodes";
        }
    }

    if (visitedEligibleNodes.size() != eligibleSet.size()) {
        return "skips required nodes";
    }

    return {};
}

int countPickupStops(const std::vector<int>& route, int depotNodeId) {
    int stopCount = 0;
    for (int nodeId : route) {
        if (nodeId != depotNodeId) {
            ++stopCount;
        }
    }
    return stopCount;
}

} // namespace

std::string Application::buildChatbotContext() const {
    // Give the AI assistant a compact, authoritative view of the current day so
    // it can reason about the map without hallucinating coordinates or waste.
    std::ostringstream out;
    const ThemeDashboardInfo info = environmentController->getDashboardInfo();
    const MapGraph& graph = wasteSystem->getGraph();

    out << "You are the in-game AI assistant for the EcoRoute Smart Waste "
           "Clearance simulation. Only answer questions about this simulation "
           "or its current state. If asked something unrelated, politely "
           "decline.\n\n";
    out << "CURRENT STATE\n";
    out << "Environment: " << info.themeLabel << " (" << info.subtitle << ")\n";
    if (info.supportsSeasons) {
        out << "Season: " << info.seasonLabel << "\n";
    }
    if (info.supportsWeather) {
        out << "Weather: " << info.weatherLabel << "\n";
    }
    out << "Atmosphere: " << info.atmosphereLabel << "\n";
    out << "Congestion: " << info.congestionLevel
        << "  Incidents: " << info.incidentCount << "\n";
    out << "Day: " << wasteSystem->getDayNumber()
        << "  Seed: " << wasteSystem->getCurrentSeed() << "\n";
    out << "Collection threshold (percent): "
        << wasteSystem->getCollectionThreshold() << "\n";
    out << "HQ node id: " << graph.getHQNode().getId()
        << " (" << graph.getHQNode().getName() << ")\n\n";

    out << "NODES (id | name | x | y | waste%)\n";
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        const WasteNode& node = graph.getNode(i);
        out << node.getId() << " | " << node.getName()
            << " | " << node.getWorldX()
            << " | " << node.getWorldY()
            << " | ";
        if (node.getIsHQ()) {
            out << "HQ";
        } else {
            out << node.getWasteLevel() << "%";
        }
        out << "\n";
    }

    const std::vector<int> eligible = wasteSystem->getEligibleNodes();
    out << "\nELIGIBLE NODE IDS (waste >= threshold): ";
    for (std::size_t i = 0; i < eligible.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << eligible[i];
    }
    out << "\n";

    const auto& results = comparisonManager->getResults();
    if (!results.empty()) {
        out << "\nRECENT ALGORITHM RESULTS\n";
        for (const RouteResult& result : results) {
            if (!result.isValid()) {
                continue;
            }
            out << result.algorithmName
                << ": distance=" << result.totalDistance << "km"
                << ", cost=RM" << result.totalCost
                << ", order=";
            for (std::size_t i = 0; i < result.visitOrder.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                out << result.visitOrder[i];
            }
            out << "\n";
        }
    }

    out << "\nRESPONSE RULES\n";
    out << "- Keep answers under 150 words.\n";
    out << "- When recommending the best route, the route MUST visit EVERY "
           "node in the ELIGIBLE list above - no node may be skipped. "
           "Optimise the visiting order to minimise total travel distance "
           "while accounting for weather/season conditions.\n";
    out << "- The route MUST start and end at the HQ node id "
        << graph.getHQNode().getId() << ".\n";
    out << "- Only visit nodes from the ELIGIBLE list above, but you MUST "
           "visit ALL of them.\n";
    out << "- Always end your reply with ONE line in this exact format:\n"
           "  ROUTE: id1,id2,id3,...,0\n"
           "  where the list includes ALL eligible node IDs.\n"
           "  If the user did not ask for a route, write: ROUTE: NONE\n";
    return out.str();
}

void Application::renderChatbotPanel() {
    const std::string context = buildChatbotContext();
    ChatbotPanel::Actions actions = chatbotPanel->render(*chatbotService, context);

    if (actions.submitQuestion && !actions.prompt.empty()) {
        chatbotService->submit(actions.prompt, context, false);
    } else if (actions.requestBestRoute) {
        const std::string prompt =
            "Recommend the optimal pickup route for today. The route MUST "
            "visit ALL eligible nodes to collect all garbage - do not skip "
            "any. Find the best ordering to minimise total travel distance. "
            "Explain briefly why this ordering is good given the weather "
            "and node positions. End with the ROUTE: line containing every "
            "eligible node ID.";
        chatbotService->submit(prompt, context, true);
    }
}

void Application::pollChatbotResponse() {
    ChatbotService::Response response;
    if (!chatbotService->tryConsumeResponse(response)) {
        return;
    }

    chatbotPanel->setLastReply(response.text);

    if (response.isRecommendation) {
        if (!response.recommendedRoute.empty()) {
            applyRecommendedRoute(response.recommendedRoute);
        } else {
            chatbotPanel->setLastRecommendationSummary("no route applied");
        }
    } else {
        chatbotPanel->setLastRecommendationSummary("");
    }
}

void Application::applyRecommendedRoute(const std::vector<int>& order) {
    const std::string validationError =
        validateRecommendedRoute(order, *wasteSystem);
    if (!validationError.empty()) {
        const std::string summary = invalidRecommendationSummary(validationError);
        chatbotPanel->setLastRecommendationSummary(summary);
        wasteSystem->getEventLog().addEvent("AI recommended route rejected: " +
                                            validationError);
        return;
    }

    const MapGraph& graph = wasteSystem->getGraph();
    const int depotNodeId = graph.getHQNode().getId();

    RouteResult aiResult;
    aiResult.algorithmName = "AI Assistant";
    aiResult.visitOrder = order;
    aiResult.runtimeMs = 0.0;
    wasteSystem->populateCosts(aiResult);

    std::ostringstream summary;
    summary << countPickupStops(order, depotNodeId) << " pickups, "
            << aiResult.totalDistance << " km, RM "
            << aiResult.totalCost;
    chatbotPanel->setLastRecommendationSummary(summary.str());

    wasteSystem->getEventLog().addEvent(
        "AI recommended route applied: " + summary.str());

    loadMissionRoute(aiResult, true);
}
