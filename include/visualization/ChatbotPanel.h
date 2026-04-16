#ifndef CHATBOT_PANEL_H
#define CHATBOT_PANEL_H

#include "ai/ChatbotService.h"

#include <string>

// Renders the bottom-left AI assistant panel. The panel owns its input buffer
// and display state but defers all transport work to ChatbotService. Emitting a
// "play recommended route" action is handled by the Application polling the
// service's consumed response on its own schedule.
class ChatbotPanel {
public:
    struct Actions {
        bool submitQuestion = false;
        bool requestBestRoute = false;
        std::string prompt; // full user question to send (when submitQuestion)
    };

    ChatbotPanel();

    // Draw the panel. snapshotText is the system context the Application
    // prepared for the current frame (used only when submitting a new request).
    Actions render(ChatbotService& service, const std::string& snapshotText);

    // Record the latest parsed reply so the panel can show it on subsequent
    // frames. Empty values clear the display.
    void setLastReply(const std::string& text);
    void setLastRecommendationSummary(const std::string& summary);

private:
    char inputBuffer[512];
    std::string lastReply;
    std::string lastRecommendation;
};

#endif // CHATBOT_PANEL_H
