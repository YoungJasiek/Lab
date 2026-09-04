#pragma once
#include <string>
#include <vector>
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabFont.h"

namespace Lab {

    struct ChatMessage {
        std::string sender;
        std::string text;
        Vec3 senderColor{ 1.0f, 0.8f, 0.2f };
        Vec3 textColor{ 0.95f, 0.95f, 0.95f };
        float lifetime = 0.0f;
        float maxLifetime = 7.5f;

        bool isExpired() const { return lifetime >= maxLifetime; }
    };

    class LabChat {
    public:
        bool isOpen = false;
        std::string currentInput = "";
        float cursorBlink = 0.0f;
        std::vector<ChatMessage> history;

        void addMessage(const std::string& sender, const std::string& text,
                        const Vec3& senderCol = Vec3(1.0f, 0.8f, 0.2f),
                        const Vec3& textCol = Vec3(0.95f, 0.95f, 0.95f));

        void open() {
            isOpen = true;
            cursorBlink = 0.0f;
        }

        void close() {
            isOpen = false;
            currentInput.clear();
        }

        void toggle() {
            if (isOpen) close();
            else open();
        }

        // Returns true if message was sent (user pressed Enter)
        bool onKey(int key, int action, std::string& outSentMessage);
        void onChar(unsigned int codepoint);
        void update(float dt);
        void render(int screenW, int screenH);
    };

} // namespace Lab
