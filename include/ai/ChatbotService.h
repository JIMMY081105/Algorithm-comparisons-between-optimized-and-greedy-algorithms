#ifndef CHATBOT_SERVICE_H
#define CHATBOT_SERVICE_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Async Gemini 2.5 Flash client.
// ChatbotService owns a single worker thread that serializes one request at a
// time. The UI layer submits prompts + a project snapshot and polls for the
// response on the render thread. The service knows nothing about ImGui or the
// simulation internals — callers pass in the text they want to send.
class ChatbotService {
public:
    enum class Status {
        Idle,
        Working,
        Ready,
        Error
    };

    // Full reply + any parsed "ROUTE: id1,id2,..." recommendation.
    struct Response {
        std::string text;                  // cleaned assistant reply (route line stripped)
        std::string raw;                   // unmodified assistant reply
        std::vector<int> recommendedRoute; // node IDs, empty if no route
        bool isRecommendation = false;     // true when the user requested a route
    };

    ChatbotService();
    ~ChatbotService();

    ChatbotService(const ChatbotService&) = delete;
    ChatbotService& operator=(const ChatbotService&) = delete;

    // Loads the API key from ai_key.txt (next to the executable). Returns false
    // if the file is missing or empty.
    bool loadKeyFromFile(const std::string& filename = "ai_key.txt");
    bool saveKeyToFile(const std::string& key,
                       const std::string& filename = "ai_key.txt");
    void setApiKey(const std::string& key);
    bool hasApiKey() const;

    // prompt is the full user-facing text. context is extra system-style data
    // (project snapshot) that gets prepended before prompt. isRecommendation is
    // stored on the Response so the UI knows to auto-animate the parsed route.
    void submit(const std::string& prompt,
                const std::string& context,
                bool isRecommendation);

    Status status() const;
    std::string lastError() const;

    // If a response is pending, copy it into out and clear the pending flag.
    // Returns false when no new response is available.
    bool tryConsumeResponse(Response& out);

private:
    struct Request {
        std::string prompt;
        std::string context;
        bool isRecommendation = false;
    };

    mutable std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> shuttingDown;
    std::atomic<Status> currentStatus;
    std::string apiKey;
    std::string errorMessage;

    std::atomic<bool> hasPendingRequest;
    std::atomic<bool> hasPendingResponse;
    Request pendingRequest;
    Response pendingResponse;

    std::unique_ptr<std::thread> worker;

    void workerLoop();
    void processRequest(const Request& request);
    static std::string buildRequestBody(const std::string& prompt,
                                        const std::string& context);
    static std::string extractReplyText(const std::string& json);
    static std::vector<int> parseRouteLine(std::string& text);
    static bool httpsPost(const std::string& host,
                          const std::string& path,
                          const std::string& body,
                          std::string& responseBody,
                          std::string& errorOut);
};

#endif // CHATBOT_SERVICE_H
