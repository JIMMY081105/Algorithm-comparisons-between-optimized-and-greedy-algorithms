#include "visualization/ChatbotPanel.h"

#include <imgui.h>

namespace {
constexpr float kPanelWidth = 500.0f;
constexpr float kPanelHeight = 480.0f;
constexpr float kPanelMargin = 8.0f;

ImVec2 computePanelPos() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float x = viewport->WorkPos.x + kPanelMargin;
    const float y = viewport->WorkPos.y + viewport->WorkSize.y -
                    kPanelHeight - kPanelMargin;
    return ImVec2(x, y);
}

const char* statusLabel(ChatbotService::Status status) {
    switch (status) {
        case ChatbotService::Status::Idle:    return "Idle";
        case ChatbotService::Status::Working: return "Thinking...";
        case ChatbotService::Status::Ready:   return "Ready";
        case ChatbotService::Status::Error:   return "Error";
    }
    return "Idle";
}

ImVec4 statusColor(ChatbotService::Status status) {
    switch (status) {
        case ChatbotService::Status::Working: return ImVec4(0.90f, 0.78f, 0.28f, 1.0f);
        case ChatbotService::Status::Ready:   return ImVec4(0.44f, 0.88f, 0.58f, 1.0f);
        case ChatbotService::Status::Error:   return ImVec4(0.94f, 0.42f, 0.38f, 1.0f);
        case ChatbotService::Status::Idle:
        default:                              return ImVec4(0.55f, 0.60f, 0.68f, 1.0f);
    }
}
} // namespace

ChatbotPanel::ChatbotPanel() {
    inputBuffer[0] = '\0';
}

void ChatbotPanel::setLastReply(const std::string& text) {
    lastReply = text;
}

void ChatbotPanel::setLastRecommendationSummary(const std::string& summary) {
    lastRecommendation = summary;
}

ChatbotPanel::Actions ChatbotPanel::render(ChatbotService& service,
                                           const std::string& /*snapshotText*/) {
    Actions actions;

    const ImVec2 pos = computePanelPos();
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, kPanelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("AI Assistant", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove);

    const ChatbotService::Status status = service.status();
    ImGui::TextColored(statusColor(status), "%s", statusLabel(status));

    if (!service.hasApiKey()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.94f, 0.42f, 0.38f, 1.0f),
                           "No API key. Place your Gemini key in ai_key.txt.");
    }

    if (status == ChatbotService::Status::Error) {
        ImGui::TextColored(ImVec4(0.94f, 0.42f, 0.38f, 1.0f), "%s",
                           service.lastError().c_str());
    }

    ImGui::Separator();

    if (!lastRecommendation.empty()) {
        ImGui::TextColored(ImVec4(0.44f, 0.88f, 0.58f, 1.0f),
                           "Recommended: %s", lastRecommendation.c_str());
    }

    ImGui::BeginChild("##chatReply", ImVec2(0, -60), true);
    if (lastReply.empty()) {
        ImGui::TextDisabled("Ask a question about the simulation, or request\n"
                            "the best route for the current day.");
    } else {
        ImGui::TextWrapped("%s", lastReply.c_str());
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(-1);
    const bool entered = ImGui::InputText("##chatInput", inputBuffer,
                                          sizeof(inputBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

    const bool busy = status == ChatbotService::Status::Working;
    ImGui::BeginDisabled(busy || !service.hasApiKey());

    const bool sendClicked = ImGui::Button("Ask", ImVec2(80, 22));
    if (sendClicked || entered) {
        if (inputBuffer[0] != '\0') {
            actions.submitQuestion = true;
            actions.prompt = inputBuffer;
            inputBuffer[0] = '\0';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Recommend best route", ImVec2(-1, 22))) {
        actions.requestBestRoute = true;
    }

    ImGui::EndDisabled();

    ImGui::End();
    return actions;
}
