#include "visualization/ChatbotPanel.h"

#include <imgui.h>

#include <cmath>

namespace {
constexpr float kPanelWidth = 500.0f;
constexpr float kPanelHeight = 480.0f;
constexpr float kPanelMargin = 8.0f;
constexpr float kIconSize = 48.0f;

ImVec2 computeIconPos() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float x = viewport->WorkPos.x + kPanelMargin;
    const float y = viewport->WorkPos.y + viewport->WorkSize.y -
                    kIconSize - kPanelMargin;
    return ImVec2(x, y);
}

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

ImU32 statusColorU32(ChatbotService::Status status) {
    const ImVec4 c = statusColor(status);
    return ImGui::ColorConvertFloat4ToU32(c);
}

// Draw a 4-pointed Gemini sparkle shape.
void drawSparkle(ImDrawList* dl, ImVec2 center, float radius, ImU32 color) {
    // Four tips: top, right, bottom, left
    // Pinch points between them create the star shape
    const float pinch = radius * 0.25f;
    const int segments = 4;
    for (int i = 0; i < segments; ++i) {
        const float tipAngle = static_cast<float>(i) * (3.14159265f * 0.5f) -
                               3.14159265f * 0.5f; // start at top
        const float midAngle = tipAngle + 3.14159265f * 0.25f;

        ImVec2 tip(center.x + std::cos(tipAngle) * radius,
                   center.y + std::sin(tipAngle) * radius);
        ImVec2 mid(center.x + std::cos(midAngle) * pinch,
                   center.y + std::sin(midAngle) * pinch);

        dl->PathLineTo(tip);
        dl->PathLineTo(mid);
    }
    dl->PathFillConvex(color);
}
} // namespace

ChatbotPanel::ChatbotPanel() : expanded(false) {
    inputBuffer[0] = '\0';
}

void ChatbotPanel::setLastReply(const std::string& text) {
    lastReply = text;
}

void ChatbotPanel::setLastRecommendationSummary(const std::string& summary) {
    lastRecommendation = summary;
}

bool ChatbotPanel::drawIcon(ChatbotService::Status status) {
    const ImVec2 pos = computeIconPos();
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kIconSize, kIconSize), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGui::Begin("##AIChatIcon", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar);

    // Make the whole window area clickable
    const ImVec2 wMin = ImGui::GetWindowPos();
    const ImVec2 wMax(wMin.x + kIconSize, wMin.y + kIconSize);
    const bool clicked = ImGui::IsMouseClicked(0) &&
                         ImGui::IsMouseHoveringRect(wMin, wMax);

    // Draw the sparkle in the center
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center(wMin.x + kIconSize * 0.5f,
                        wMin.y + kIconSize * 0.5f);

    // Pulsing glow when AI is thinking
    if (status == ChatbotService::Status::Working) {
        const float t = static_cast<float>(ImGui::GetTime());
        const float pulse = 0.5f + 0.5f * std::sin(t * 4.0f);
        const float glowRadius = 16.0f + pulse * 4.0f;
        ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.90f, 0.78f, 0.28f, 0.25f + pulse * 0.15f));
        dl->AddCircleFilled(center, glowRadius, glowColor, 24);
    }

    drawSparkle(dl, center, 14.0f, statusColorU32(status));

    // Small inner sparkle for depth
    ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.6f));
    drawSparkle(dl, center, 5.0f, white);

    // Tooltip on hover
    if (ImGui::IsMouseHoveringRect(wMin, wMax)) {
        ImGui::SetTooltip("AI Assistant (%s)", statusLabel(status));
    }

    ImGui::End();
    return clicked;
}

ChatbotPanel::Actions ChatbotPanel::render(ChatbotService& service,
                                           const std::string& /*snapshotText*/) {
    Actions actions;
    const ChatbotService::Status status = service.status();

    if (!expanded) {
        if (drawIcon(status)) {
            expanded = true;
        }
        return actions;
    }

    // --- Expanded panel ---
    const ImVec2 pos = computePanelPos();
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, kPanelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("AI Assistant", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove);

    // Collapse button (sparkle icon) + status on the same line
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 sparkleCenter(cursor.x + 10.0f, cursor.y + 8.0f);

        // Pulsing glow when thinking
        if (status == ChatbotService::Status::Working) {
            const float t = static_cast<float>(ImGui::GetTime());
            const float pulse = 0.5f + 0.5f * std::sin(t * 4.0f);
            ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(0.90f, 0.78f, 0.28f, 0.20f + pulse * 0.12f));
            dl->AddCircleFilled(sparkleCenter, 11.0f + pulse * 2.0f,
                                glowColor, 20);
        }

        drawSparkle(dl, sparkleCenter, 8.0f, statusColorU32(status));
        drawSparkle(dl, sparkleCenter, 3.0f,
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(1.0f, 1.0f, 1.0f, 0.5f)));

        // Invisible button over the sparkle area to handle click-to-collapse
        ImGui::SetCursorScreenPos(ImVec2(cursor.x - 2.0f, cursor.y - 4.0f));
        if (ImGui::InvisibleButton("##collapseBtn", ImVec2(22.0f, 22.0f))) {
            expanded = false;
            ImGui::End();
            return actions;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Minimize");
        }

        ImGui::SameLine();
        ImGui::TextColored(statusColor(status), "%s", statusLabel(status));
    }

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
