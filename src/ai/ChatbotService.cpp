#include "ai/ChatbotService.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#endif

namespace {
constexpr const char* kGeminiHost = "generativelanguage.googleapis.com";
constexpr const char* kGeminiPath = "/v1beta/models/gemini-2.5-flash:generateContent?key=";

std::string trim(const std::string& s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    const auto start = std::find_if(s.begin(), s.end(), notSpace);
    const auto end = std::find_if(s.rbegin(), s.rend(), notSpace).base();
    return (start < end) ? std::string(start, end) : std::string();
}

std::string escapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string decodeJsonString(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            switch (next) {
                case '"':  out += '"';  i += 1; break;
                case '\\': out += '\\'; i += 1; break;
                case '/':  out += '/';  i += 1; break;
                case 'b':  out += '\b'; i += 1; break;
                case 'f':  out += '\f'; i += 1; break;
                case 'n':  out += '\n'; i += 1; break;
                case 'r':  out += '\r'; i += 1; break;
                case 't':  out += '\t'; i += 1; break;
                case 'u': {
                    if (i + 5 < raw.size()) {
                        unsigned int code = 0;
                        std::stringstream ss;
                        ss << std::hex << raw.substr(i + 2, 4);
                        ss >> code;
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        i += 5;
                    }
                    break;
                }
                default:
                    out += next;
                    i += 1;
            }
        } else {
            out += c;
        }
    }
    return out;
}
} // namespace

ChatbotService::ChatbotService()
    : shuttingDown(false),
      currentStatus(Status::Idle),
      hasPendingRequest(false),
      hasPendingResponse(false) {
    worker = std::make_unique<std::thread>(&ChatbotService::workerLoop, this);
}

ChatbotService::~ChatbotService() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        shuttingDown.store(true);
    }
    cv.notify_all();
    if (worker && worker->joinable()) {
        worker->join();
    }
}

bool ChatbotService::loadKeyFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string trimmed = trim(buffer.str());
    if (trimmed.empty()) return false;
    setApiKey(trimmed);
    return true;
}

bool ChatbotService::saveKeyToFile(const std::string& key,
                                   const std::string& filename) {
    std::ofstream file(filename, std::ios::trunc);
    if (!file) return false;
    file << key;
    setApiKey(key);
    return true;
}

void ChatbotService::setApiKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    apiKey = trim(key);
}

bool ChatbotService::hasApiKey() const {
    std::lock_guard<std::mutex> lock(mutex);
    return !apiKey.empty();
}

void ChatbotService::submit(const std::string& prompt,
                            const std::string& context,
                            bool isRecommendation) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (apiKey.empty()) {
            errorMessage = "API key not configured.";
            currentStatus = Status::Error;
            return;
        }
        pendingRequest.prompt = prompt;
        pendingRequest.context = context;
        pendingRequest.isRecommendation = isRecommendation;
        hasPendingRequest.store(true);
        hasPendingResponse.store(false);
        errorMessage.clear();
        currentStatus = Status::Working;
    }
    cv.notify_all();
}

ChatbotService::Status ChatbotService::status() const {
    return currentStatus.load();
}

std::string ChatbotService::lastError() const {
    std::lock_guard<std::mutex> lock(mutex);
    return errorMessage;
}

bool ChatbotService::tryConsumeResponse(Response& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!hasPendingResponse.load()) return false;
    out = pendingResponse;
    hasPendingResponse.store(false);
    pendingResponse = Response{};
    return true;
}

void ChatbotService::workerLoop() {
    while (true) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] {
                return shuttingDown.load() || hasPendingRequest.load();
            });
            if (shuttingDown.load()) return;
            request = pendingRequest;
            hasPendingRequest.store(false);
        }
        processRequest(request);
    }
}

void ChatbotService::processRequest(const Request& request) {
    std::string key;
    {
        std::lock_guard<std::mutex> lock(mutex);
        key = apiKey;
    }

    const std::string body = buildRequestBody(request.prompt, request.context);
    std::string responseBody;
    std::string httpError;
    const std::string path = std::string(kGeminiPath) + key;
    const bool ok = httpsPost(kGeminiHost, path, body, responseBody, httpError);

    if (!ok) {
        std::lock_guard<std::mutex> lock(mutex);
        errorMessage = httpError.empty() ? "HTTP request failed." : httpError;
        currentStatus = Status::Error;
        return;
    }

    std::string reply = extractReplyText(responseBody);
    if (reply.empty()) {
        std::lock_guard<std::mutex> lock(mutex);
        errorMessage = "Empty or malformed response: " +
                       responseBody.substr(0, 300);
        currentStatus = Status::Error;
        return;
    }

    const std::string raw = reply;
    std::string cleaned = reply;
    const std::vector<int> route = parseRouteLine(cleaned);

    Response response;
    response.raw = raw;
    response.text = trim(cleaned);
    response.recommendedRoute = route;
    response.isRecommendation = request.isRecommendation;

    {
        std::lock_guard<std::mutex> lock(mutex);
        pendingResponse = response;
        hasPendingResponse.store(true);
        currentStatus = Status::Ready;
    }
}

std::string ChatbotService::buildRequestBody(const std::string& prompt,
                                             const std::string& context) {
    std::string combined;
    if (!context.empty()) {
        combined += context;
        combined += "\n\n";
    }
    combined += prompt;

    std::string body = "{\"contents\":[{\"parts\":[{\"text\":\"";
    body += escapeJsonString(combined);
    body += "\"}]}]}";
    return body;
}

std::string ChatbotService::extractReplyText(const std::string& json) {
    // Look for candidates[0].content.parts[0].text.
    const std::size_t candidates = json.find("\"candidates\"");
    if (candidates == std::string::npos) return std::string();

    const std::size_t text = json.find("\"text\"", candidates);
    if (text == std::string::npos) return std::string();

    std::size_t colon = json.find(':', text);
    if (colon == std::string::npos) return std::string();

    std::size_t quote = json.find('"', colon);
    if (quote == std::string::npos) return std::string();

    std::string raw;
    for (std::size_t i = quote + 1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            raw += c;
            raw += json[i + 1];
            ++i;
            continue;
        }
        if (c == '"') break;
        raw += c;
    }
    return decodeJsonString(raw);
}

std::vector<int> ChatbotService::parseRouteLine(std::string& text) {
    std::vector<int> result;
    const std::size_t marker = text.find("ROUTE:");
    if (marker == std::string::npos) return result;

    const std::size_t lineEnd = text.find_first_of("\r\n", marker);
    const std::size_t endPos =
        (lineEnd == std::string::npos) ? text.size() : lineEnd;
    std::string line = text.substr(marker + 6, endPos - (marker + 6));

    // Strip the line from the displayed text.
    std::size_t eraseEnd = endPos;
    if (eraseEnd < text.size() && (text[eraseEnd] == '\r' || text[eraseEnd] == '\n')) {
        ++eraseEnd;
        if (eraseEnd < text.size() && text[eraseEnd - 1] == '\r' &&
            text[eraseEnd] == '\n') {
            ++eraseEnd;
        }
    }
    text.erase(marker, eraseEnd - marker);

    if (line.find("NONE") != std::string::npos) return result;

    std::string current;
    for (char c : line) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            current += c;
        } else {
            if (!current.empty()) {
                try { result.push_back(std::stoi(current)); } catch (...) {}
                current.clear();
            }
        }
    }
    if (!current.empty()) {
        try { result.push_back(std::stoi(current)); } catch (...) {}
    }
    return result;
}

#ifdef _WIN32
bool ChatbotService::httpsPost(const std::string& host,
                               const std::string& path,
                               const std::string& body,
                               std::string& responseBody,
                               std::string& errorOut) {
    HINTERNET session = InternetOpenA("EcoRouteChatbot/1.0",
                                      INTERNET_OPEN_TYPE_PRECONFIG,
                                      nullptr, nullptr, 0);
    if (!session) {
        errorOut = "InternetOpen failed.";
        return false;
    }

    HINTERNET connection = InternetConnectA(session, host.c_str(),
                                            INTERNET_DEFAULT_HTTPS_PORT,
                                            nullptr, nullptr,
                                            INTERNET_SERVICE_HTTP, 0, 0);
    if (!connection) {
        errorOut = "InternetConnect failed.";
        InternetCloseHandle(session);
        return false;
    }

    const char* acceptTypes[] = {"application/json", nullptr};
    const DWORD requestFlags = INTERNET_FLAG_SECURE |
                               INTERNET_FLAG_NO_CACHE_WRITE |
                               INTERNET_FLAG_RELOAD |
                               INTERNET_FLAG_KEEP_CONNECTION;

    HINTERNET request = HttpOpenRequestA(connection, "POST", path.c_str(),
                                         "HTTP/1.1", nullptr, acceptTypes,
                                         requestFlags, 0);
    if (!request) {
        errorOut = "HttpOpenRequest failed.";
        InternetCloseHandle(connection);
        InternetCloseHandle(session);
        return false;
    }

    const std::string headers = "Content-Type: application/json\r\n";
    std::vector<char> requestBody(body.begin(), body.end());
    BOOL sent = HttpSendRequestA(request,
                                 headers.c_str(),
                                 static_cast<DWORD>(headers.size()),
                                 requestBody.empty() ? nullptr : requestBody.data(),
                                 static_cast<DWORD>(body.size()));
    if (!sent) {
        errorOut = "HttpSendRequest failed (code " +
                   std::to_string(GetLastError()) + ").";
        InternetCloseHandle(request);
        InternetCloseHandle(connection);
        InternetCloseHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusLen = sizeof(statusCode);
    HttpQueryInfoA(request,
                   HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                   &statusCode, &statusLen, nullptr);

    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) &&
           bytesRead > 0) {
        responseBody.append(buffer, bytesRead);
    }

    InternetCloseHandle(request);
    InternetCloseHandle(connection);
    InternetCloseHandle(session);

    if (statusCode < 200 || statusCode >= 300) {
        errorOut = "HTTP " + std::to_string(statusCode) + ": " +
                   responseBody.substr(0, 300);
        return false;
    }
    return true;
}
#else
bool ChatbotService::httpsPost(const std::string&, const std::string&,
                               const std::string&, std::string&,
                               std::string& errorOut) {
    errorOut = "ChatbotService only supports Windows (WinINet) in this build.";
    return false;
}
#endif
