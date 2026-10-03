#include "plugins/DialogueQuestPlugin.h"
#include <iostream>
#include <algorithm>

namespace Lab {

    DialogueQuestPlugin::DialogueQuestPlugin() {
        m_activeQuests.push_back("Infiltrate Facility Beta");
        m_activeQuests.push_back("Restore Emergency Power");
    }

    bool DialogueQuestPlugin::onLoad() {
        LabLog::info("[Plugin: DialogueQuestPlugin] Loaded v2.1.0 successfully.");
        return true;
    }

    void DialogueQuestPlugin::onUpdate(float dt) {
        (void)dt;
    }

    void DialogueQuestPlugin::onUnload() {
        LabLog::info("[Plugin: DialogueQuestPlugin] Unloaded.");
    }

    void DialogueQuestPlugin::startDialogue(const std::string& npcName) {
        m_activeNpc = npcName.empty() ? "Station Commander" : npcName;
        m_currentLine = "Welcome to Sector 7. The main power conduits have overloaded.";
        m_activeChoices = {
            "1. Where can I find the backup keycard?",
            "2. Give me weapons and ammunition.",
            "3. I'll secure the perimeter immediately."
        };
    }

    void DialogueQuestPlugin::chooseOption(int optionIndex) {
        if (optionIndex == 0) {
            m_currentLine = "The security keycard is locked inside the control bridge.";
            m_activeChoices = { "1. Acknowledged.", "2. Any hostiles nearby?" };
        } else if (optionIndex == 1) {
            m_currentLine = "Supply depot is stocked. Check the armory lockers down the hall.";
            m_activeChoices = { "1. Understood." };
        } else {
            m_currentLine = "Stay alert. Motion sensors detect anomalous readings in the lower shaft.";
            m_activeChoices.clear();
        }
    }

    void DialogueQuestPlugin::addQuest(const std::string& questName) {
        if (!questName.empty()) m_activeQuests.push_back(questName);
    }

    void DialogueQuestPlugin::completeQuest(const std::string& questName) {
        auto it = std::find(m_activeQuests.begin(), m_activeQuests.end(), questName);
        if (it != m_activeQuests.end()) {
            m_activeQuests.erase(it);
        }
    }

} // namespace Lab
