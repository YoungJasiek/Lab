#pragma once

#include "LabPlugin.h"
#include <string>
#include <vector>

namespace Lab {

    /// @brief 1 kod 1 plugin: Standalone Branching Dialogue & Quest System Plugin
    class LAB_CORE_API DialogueQuestPlugin : public ILabPlugin {
    public:
        DialogueQuestPlugin();
        virtual ~DialogueQuestPlugin() override = default;

        virtual const char* getName() const override { return "Dialogue & Quest System"; }
        virtual const char* getVersion() const override { return "2.1.0"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        void startDialogue(const std::string& npcName);
        void chooseOption(int optionIndex);
        const std::string& getActiveNpc() const { return m_activeNpc; }
        const std::string& getCurrentLine() const { return m_currentLine; }
        const std::vector<std::string>& getActiveChoices() const { return m_activeChoices; }

        void addQuest(const std::string& questName);
        void completeQuest(const std::string& questName);
        const std::vector<std::string>& getActiveQuests() const { return m_activeQuests; }

    private:
        std::string m_activeNpc;
        std::string m_currentLine;
        std::vector<std::string> m_activeChoices;
        std::vector<std::string> m_activeQuests;
    };

} // namespace Lab
