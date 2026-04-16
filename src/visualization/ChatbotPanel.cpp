#include "visualization/ChatbotPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPanelWidth  = 460.0f;
constexpr float kPanelHeight = 500.0f;
constexpr float kPanelMargin = 8.0f;
constexpr float kIconSize    = 48.0f;
constexpr float kIconBottomOffset = 60.0f; // how far above the very bottom
constexpr float kBubbleMaxWidth = 340.0f;
constexpr float kBubblePadding  = 8.0f;
constexpr float kBubbleRounding = 10.0f;
constexpr float kBubbleSpacing  = 6.0f;

// Icon sits bottom-left, raised by kIconBottomOffset.
ImVec2 computeIconPos() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    return ImVec2(vp->WorkPos.x + kPanelMargin,
                  vp->WorkPos.y + vp->WorkSize.y -
                      kIconSize - kIconBottomOffset);
}

// Chat panel sits directly above the icon.
ImVec2 computePanelPos() {
    const ImVec2 icon = computeIconPos();
    return ImVec2(icon.x,
                  icon.y - kPanelHeight - kPanelMargin);
}

const char* statusLabel(ChatbotService::Status s) {
    switch (s) {
        case ChatbotService::Status::Idle:    return "Idle";
        case ChatbotService::Status::Working: return "Thinking...";
        case ChatbotService::Status::Ready:   return "Ready";
        case ChatbotService::Status::Error:   return "Error";
    }
    return "Idle";
}

ImVec4 statusColor(ChatbotService::Status s) {
    switch (s) {
        case ChatbotService::Status::Working: return ImVec4(0.90f, 0.78f, 0.28f, 1.0f);
        case ChatbotService::Status::Ready:   return ImVec4(0.44f, 0.88f, 0.58f, 1.0f);
        case ChatbotService::Status::Error:   return ImVec4(0.94f, 0.42f, 0.38f, 1.0f);
        default:                              return ImVec4(0.55f, 0.60f, 0.68f, 1.0f);
    }
}

ImU32 toU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// 4-pointed Gemini sparkle.
void drawSparkle(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    const float pinch = r * 0.25f;
    constexpr float pi = 3.14159265f;
    for (int i = 0; i < 4; ++i) {
        float tipA = i * (pi * 0.5f) - pi * 0.5f;
        float midA = tipA + pi * 0.25f;
        dl->PathLineTo(ImVec2(c.x + std::cos(tipA) * r,
                              c.y + std::sin(tipA) * r));
        dl->PathLineTo(ImVec2(c.x + std::cos(midA) * pinch,
                              c.y + std::sin(midA) * pinch));
    }
    dl->PathFillConvex(col);
}

// Draw one chat bubble, returns the height consumed.
float drawBubble(ImDrawList* dl, const std::string& text, bool isUser,
                 float regionWidth, float cursorY, float scrollY,
                 ImVec2 regionScreenPos) {
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false,
                                                kBubbleMaxWidth - kBubblePadding * 2);
    const float bubbleW = textSize.x + kBubblePadding * 2;
    const float bubbleH = textSize.y + kBubblePadding * 2;

    float bubbleX;
    if (isUser) {
        bubbleX = regionScreenPos.x + regionWidth - bubbleW - 12.0f;
    } else {
        bubbleX = regionScreenPos.x + 12.0f;
    }
    const float bubbleY = regionScreenPos.y + cursorY - scrollY;

    ImU32 bgColor;
    ImU32 textColor;
    if (isUser) {
        bgColor  = toU32(ImVec4(0.10f, 0.32f, 0.50f, 0.92f));
        textColor = toU32(ImVec4(0.92f, 0.94f, 0.96f, 1.0f));
    } else {
        bgColor  = toU32(ImVec4(0.12f, 0.14f, 0.20f, 0.92f));
        textColor = toU32(ImVec4(0.82f, 0.86f, 0.90f, 1.0f));
    }

    ImVec2 bMin(bubbleX, bubbleY);
    ImVec2 bMax(bubbleX + bubbleW, bubbleY + bubbleH);

    dl->AddRectFilled(bMin, bMax, bgColor, kBubbleRounding);

    ImVec2 txtPos(bubbleX + kBubblePadding, bubbleY + kBubblePadding);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), txtPos, textColor,
                text.c_str(), nullptr, kBubbleMaxWidth - kBubblePadding * 2);

    return bubbleH + kBubbleSpacing;
}
} // namespace

// ---- ChatbotPanel implementation ----

ChatbotPanel::ChatbotPanel() : expanded(false), scrollToBottom(false) {
    inputBuffer[0] = '\0';
}

void ChatbotPanel::setLastReply(const std::string& text) {
    if (!text.empty()) {
        messages.push_back({text, false});
        scrollToBottom = true;
    }
}

void ChatbotPanel::setLastRecommendationSummary(const std::string& summary) {
    lastRecommendation = summary;
}

// Always draws the icon. Clicking it toggles expanded.
void ChatbotPanel::drawIcon(ChatbotService::Status status) {
    const ImVec2 pos = computeIconPos();
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kIconSize, kIconSize), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGui::Begin("##AIChatIcon", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove    | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar);

    const ImVec2 wMin = ImGui::GetWindowPos();
    const ImVec2 wMax(wMin.x + kIconSize, wMin.y + kIconSize);

    if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringRect(wMin, wMax)) {
        expanded = !expanded;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center(wMin.x + kIconSize * 0.5f, wMin.y + kIconSize * 0.5f);

    // Pulsing glow when thinking
    if (status == ChatbotService::Status::Working) {
        float t = static_cast<float>(ImGui::GetTime());
        float pulse = 0.5f + 0.5f * std::sin(t * 4.0f);
        dl->AddCircleFilled(center, 16.0f + pulse * 4.0f,
            toU32(ImVec4(0.90f, 0.78f, 0.28f, 0.25f + pulse * 0.15f)), 24);
    }

    drawSparkle(dl, center, 14.0f, toU32(statusColor(status)));
    drawSparkle(dl, center, 5.0f, toU32(ImVec4(1.0f, 1.0f, 1.0f, 0.6f)));

    if (ImGui::IsMouseHoveringRect(wMin, wMax)) {
        ImGui::SetTooltip("AI Assistant (%s)", statusLabel(status));
    }

    ImGui::End();
}

void ChatbotPanel::drawChatPanel(ChatbotService& service, Actions& actions) {
    const ChatbotService::Status status = service.status();
    const ImVec2 pos = computePanelPos();
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, kPanelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("AI Assistant", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove);

    // Header: status + recommendation
    ImGui::TextColored(statusColor(status), "%s", statusLabel(status));

    if (!service.hasApiKey()) {
        ImGui::TextColored(ImVec4(0.94f, 0.42f, 0.38f, 1.0f),
                           "No API key. Place your Gemini key in ai_key.txt.");
    }
    if (status == ChatbotService::Status::Error) {
        ImGui::TextColored(ImVec4(0.94f, 0.42f, 0.38f, 1.0f), "%s",
                           service.lastError().c_str());
    }
    if (!lastRecommendation.empty()) {
        ImGui::TextColored(ImVec4(0.44f, 0.88f, 0.58f, 1.0f),
                           "Route: %s", lastRecommendation.c_str());
    }

    ImGui::Separator();

    // ---- Chat area with bubbles ----
    const float inputAreaHeight = 58.0f;
    ImGui::BeginChild("##chatArea", ImVec2(0, -inputAreaHeight), true);

    if (messages.empty()) {
        ImGui::TextDisabled("Ask a question about the simulation,\n"
                            "or request the best route for today.");
    } else {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 regionPos = ImGui::GetCursorScreenPos();
        const float regionW = ImGui::GetContentRegionAvail().x;
        const float scrollY = ImGui::GetScrollY();

        // Calculate total content height to set up scrollable area
        float totalH = 0.0f;
        for (const auto& msg : messages) {
            ImVec2 ts = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false,
                                            kBubbleMaxWidth - kBubblePadding * 2);
            totalH += ts.y + kBubblePadding * 2 + kBubbleSpacing;
        }

        // Reserve space so ImGui scroll works
        ImGui::Dummy(ImVec2(0, totalH));

        // Draw bubbles
        float y = 0.0f;
        for (const auto& msg : messages) {
            y += drawBubble(dl, msg.text, msg.isUser,
                            regionW, y, scrollY, regionPos);
        }

        if (scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom = false;
        }
    }

    ImGui::EndChild();

    // ---- Input bar ----
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool entered = ImGui::InputText("##chatInput", inputBuffer,
                                          sizeof(inputBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

    const bool busy = status == ChatbotService::Status::Working;
    ImGui::BeginDisabled(busy || !service.hasApiKey());

    const bool sendClicked = ImGui::Button("Ask", ImVec2(80, 24));
    if (sendClicked || entered) {
        if (inputBuffer[0] != '\0') {
            actions.submitQuestion = true;
            actions.prompt = inputBuffer;
            messages.push_back({inputBuffer, true});
            scrollToBottom = true;
            inputBuffer[0] = '\0';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Recommend best route", ImVec2(-1, 24))) {
        actions.requestBestRoute = true;
        messages.push_back({"Recommend the best route", true});
        scrollToBottom = true;
    }

    ImGui::EndDisabled();
    ImGui::End();
}

ChatbotPanel::Actions ChatbotPanel::render(ChatbotService& service,
                                           const std::string& /*snapshotText*/) {
    Actions actions;
    const ChatbotService::Status status = service.status();

    // Icon is always visible — clicking toggles the chat panel
    drawIcon(status);

    if (expanded) {
        drawChatPanel(service, actions);
    }

    return actions;
}
