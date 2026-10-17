#include "LabChat.h"
#include <algorithm>
#include <cmath>

namespace Lab {

    void LabChat::addMessage(const std::string& sender, const std::string& text,
                             const Vec3& senderCol, const Vec3& textCol) {
        ChatMessage msg;
        msg.sender = sender;
        msg.text = text;
        msg.senderColor = senderCol;
        msg.textColor = textCol;
        msg.lifetime = 0.0f;
        msg.maxLifetime = 7.5f;

        history.push_back(msg);
        if (history.size() > 30) {
            history.erase(history.begin());
        }
    }

    bool LabChat::onKey(int key, int action, std::string& outSentMessage) {
        if (!isOpen) return false;
        if (action != 1 && action != 2) return false; // GLFW_PRESS or GLFW_REPEAT

        if (key == 257) { // GLFW_KEY_ENTER
            if (!currentInput.empty()) {
                outSentMessage = currentInput;
                addMessage("[YOU] Player", currentInput, Vec3(1.0f, 0.82f, 0.2f), Vec3(1.0f, 1.0f, 1.0f));
                close();
                return true;
            } else {
                close();
                return false;
            }
        }

        if (key == 256) { // GLFW_KEY_ESCAPE
            close();
            return false;
        }

        if (key == 259) { // GLFW_KEY_BACKSPACE
            if (!currentInput.empty()) {
                currentInput.pop_back();
            }
            return false;
        }

        return false;
    }

    void LabChat::onChar(unsigned int codepoint) {
        if (!isOpen) return;
        if (codepoint >= 32 && codepoint < 127) {
            if (currentInput.length() < 64) {
                currentInput.push_back(static_cast<char>(codepoint));
            }
        }
    }

    void LabChat::update(float dt) {
        cursorBlink += dt * 3.5f;
        for (auto& msg : history) {
            msg.lifetime += dt;
        }
    }

    void LabChat::render(int screenW, int screenH) {
        // Collect messages to display
        std::vector<const ChatMessage*> displayMsgs;
        for (const auto& msg : history) {
            if (isOpen || !msg.isExpired()) {
                displayMsgs.push_back(&msg);
            }
        }

        if (displayMsgs.empty() && !isOpen) return;

        Renderer::beginUI(screenW, screenH);

        float chatX = 40.0f;
        float chatW = 460.0f;
        float lineH = 22.0f;
        int maxLines = isOpen ? 7 : 5;
        int startIdx = (int)displayMsgs.size() > maxLines ? (int)displayMsgs.size() - maxLines : 0;

        float bottomY = (float)screenH - 125.0f;
        if (isOpen) bottomY -= 36.0f; // Leave space for input bar

        // Render message history
        int lineCount = (int)displayMsgs.size() - startIdx;
        float totalMsgH = (float)lineCount * lineH;
        float topY = bottomY - totalMsgH;

        if (isOpen && lineCount > 0) {
            Renderer::drawRoundedRect(
                chatX - 8.0f, topY - 4.0f, chatW + 16.0f, totalMsgH + 8.0f,
                Vec3(0.03f, 0.04f, 0.06f), 0.32f, 4.0f, 0.0f
            );
        }

        float currY = topY;
        for (int i = startIdx; i < (int)displayMsgs.size(); ++i) {
            const auto* msg = displayMsgs[i];
            float alpha = 1.0f;
            if (!isOpen && msg->lifetime > msg->maxLifetime - 1.5f) {
                alpha = std::clamp((msg->maxLifetime - msg->lifetime) / 1.5f, 0.0f, 1.0f);
            }

            // Subtle drop shadow + sender prefix
            std::string prefix = msg->sender + ": ";
            LabFont::drawText(chatX + 1.0f, currY + 4.0f, prefix, 1.45f, Vec3(0, 0, 0), LabFontType::GeoSans, alpha * 0.5f);
            LabFont::drawText(chatX, currY + 3.0f, prefix, 1.45f, msg->senderColor, LabFontType::GeoSans, alpha);

            // Compute exact offset for text using font metrics
            float prefixWidth = LabFont::getTextWidth(prefix, 1.45f, LabFontType::GeoSans) + 4.0f;
            LabFont::drawText(chatX + prefixWidth + 1.0f, currY + 4.0f, msg->text, 1.45f, Vec3(0, 0, 0), LabFontType::GeoSans, alpha * 0.5f);
            LabFont::drawText(chatX + prefixWidth, currY + 3.0f, msg->text, 1.45f, msg->textColor, LabFontType::GeoSans, alpha);

            currY += lineH;
        }

        // Render active input bar if open
        if (isOpen) {
            float barY = bottomY + 4.0f;
            float barH = 26.0f;

            // Sleek borderless translucent input pill
            Renderer::drawRoundedRect(
                chatX - 8.0f, barY, chatW + 16.0f, barH,
                Vec3(0.04f, 0.05f, 0.07f), 0.45f, 4.0f, 0.0f
            );

            // Prompt label & typed text
            std::string prompt = "SAY: " + currentInput + (std::fmod(cursorBlink, 2.0f) < 1.0f ? "_" : "");
            LabFont::drawText(chatX + 4.0f, barY + 6.0f, prompt, 1.45f, Vec3(1.0f, 0.82f, 0.22f), LabFontType::GeoSans, 0.95f);
        }

        Renderer::endUI();
    }

} // namespace Lab
