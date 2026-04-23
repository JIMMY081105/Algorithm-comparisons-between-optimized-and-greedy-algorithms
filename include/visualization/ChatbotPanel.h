#ifndef CHATBOT_PANEL_H
#define CHATBOT_PANEL_H

#include "ai/ChatbotService.h"

#include <string>
#include <vector>

class ChatbotPanel {
public:
    struct Actions {
        bool submitQuestion = false;
        bool requestBestRoute = false;
        std::string prompt;
    };

    ChatbotPanel();

    Actions render(ChatbotService& service, const std::string& snapshotText);

    void setLastReply(const std::string& text);
    void setLastRecommendationSummary(const std::string& summary);

private:
    struct ChatMessage {
        std::string text;
        bool isUser; // true = user bubble (right), false = AI bubble (left)
    };

    std::string inputText;
    std::string lastRecommendation;
    std::vector<ChatMessage> messages;
    bool expanded;
    bool scrollToBottom;

    void drawIcon(ChatbotService::Status status);
    void drawChatPanel(ChatbotService& service, Actions& actions);
};

#endif // CHATBOT_PANEL_H
