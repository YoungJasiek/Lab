#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstdint>
#include <string_view>
#include "LabExport.h"
#include "LabMath.h"

namespace Lab {

    enum ConVarFlag : uint32_t {
        FCVAR_NONE                = 0,
        FCVAR_ARCHIVE             = (1 << 0),  // Saved to config.cfg
        FCVAR_CHEAT               = (1 << 1),  // Requires sv_cheats 1 to modify
        FCVAR_REPLICATED          = (1 << 2),  // Synchronized to clients
        FCVAR_USERINFO            = (1 << 3),  // Userinfo
        FCVAR_NOTIFY              = (1 << 4),  // Server notifies players on change
        FCVAR_SERVER_CAN_EXECUTE  = (1 << 5),  // Server can execute on client
        FCVAR_CLIENTCMD_CAN_EXECUTE = (1 << 6),// Client can execute
        FCVAR_PROTECTED           = (1 << 7),  // Sensitive / password
        FCVAR_PRINTABLEONLY       = (1 << 8),  // Printable characters only
        FCVAR_DEVELOPMENTONLY     = (1 << 9),  // Development only
        FCVAR_MOD                 = (1 << 10), // Registered by modder / addon
        FCVAR_CLIENTDLL           = (1 << 11), // Client subsystem
        FCVAR_GAMEDLL             = (1 << 12)  // Server / Game subsystem
    };

    class ConVar;

    using ConCommandCallback = std::function<void(const std::vector<std::string>& args)>;
    using ConVarChangeCallback = std::function<void(ConVar* cvar, const std::string& oldVal, const std::string& newVal)>;

    struct LAB_CORE_API ConCommand {
        std::string name;
        ConCommandCallback callback;
        std::string helpText;
        uint32_t flags = FCVAR_NONE;
    };

    class LAB_CORE_API ConVar {
    public:
        ConVar(const std::string& name, const std::string& defaultValue, uint32_t flags,
               const std::string& helpText, bool hasMin = false, float minVal = 0.0f,
               bool hasMax = false, float maxVal = 0.0f, ConVarChangeCallback onChange = nullptr);

        const std::string& getName() const { return _name; }
        const std::string& getString() const { return _value; }
        float getFloat() const { return _floatValue; }
        int getInt() const { return _intValue; }
        bool getBool() const { return _boolValue; }
        const std::string& getDefault() const { return _defaultValue; }
        const std::string& getHelpText() const { return _helpText; }
        uint32_t getFlags() const { return _flags; }
        bool hasMin() const { return _hasMin; }
        float getMin() const { return _minVal; }
        bool hasMax() const { return _hasMax; }
        float getMax() const { return _maxVal; }

        void setValue(const std::string& value);
        void setValue(float value);
        void setValue(int value);
        void resetToDefault();

        bool isCheat() const { return (_flags & FCVAR_CHEAT) != 0; }
        bool isArchive() const { return (_flags & FCVAR_ARCHIVE) != 0; }
        bool isReplicated() const { return (_flags & FCVAR_REPLICATED) != 0; }
        bool isMod() const { return (_flags & FCVAR_MOD) != 0; }
        bool isProtected() const { return (_flags & FCVAR_PROTECTED) != 0; }
        bool hasFlag(uint32_t flag) const { return (_flags & flag) != 0; }

        std::string getFlagsString() const;

    private:
        void updateParsedValues();

        std::string _name;
        std::string _value;
        std::string _defaultValue;
        std::string _helpText;
        uint32_t _flags = FCVAR_NONE;
        float _floatValue = 0.0f;
        int _intValue = 0;
        bool _boolValue = false;
        bool _hasMin = false;
        float _minVal = 0.0f;
        bool _hasMax = false;
        float _maxVal = 0.0f;
        ConVarChangeCallback _onChange;
    };

    class LAB_CORE_API ConVarRegistry {
    public:
        static ConVarRegistry& get();

        ConVarRegistry(const ConVarRegistry&) = delete;
        ConVarRegistry& operator=(const ConVarRegistry&) = delete;
        ConVarRegistry(ConVarRegistry&&) = delete;
        ConVarRegistry& operator=(ConVarRegistry&&) = delete;

        // Modder & Engine Registration API
        ConVar* registerConVar(const std::string& name, const std::string& defaultValue, uint32_t flags,
                               const std::string& helpText, bool hasMin = false, float minVal = 0.0f,
                               bool hasMax = false, float maxVal = 0.0f, ConVarChangeCallback onChange = nullptr);

        bool registerConCommand(const std::string& name, ConCommandCallback callback, uint32_t flags,
                                const std::string& helpText);

        bool unregister(const std::string& name);

        // Lookup
        ConVar* findConVar(const std::string& name);
        const ConVar* findConVar(const std::string& name) const;
        const ConCommand* findConCommand(const std::string& name) const;

        bool hasCommandOrVar(const std::string& name) const;
        size_t getTotalCommandCount() const;

        // Collections & Introspection
        const std::unordered_map<std::string, std::unique_ptr<ConVar>>& getConVars() const { return _convars; }
        const std::unordered_map<std::string, ConCommand>& getCommands() const { return _commands; }
        std::vector<std::string> getAllNames() const;
        std::vector<std::string> getAutocompleteSuggestions(const std::string& prefix, size_t maxCount = 10) const;

        // Execution & Scripting
        using PrintSink = std::function<void(const std::string& output, const Vec3& color)>;
        void print(const std::string& text, const Vec3& color = Vec3(0.9f, 0.9f, 0.9f));
        void setActivePrintSink(PrintSink sink);
        PrintSink getActivePrintSink() const { return _activePrintSink; }
        bool execute(const std::string& cmdLine, std::function<void(const std::string& output, const Vec3& color)> printFn = nullptr);

        // Meta commands
        std::vector<std::string> getCvarList(const std::string& filter = "") const;
        std::vector<std::string> getDifferences() const;
        std::string writeConfigFile() const;
        bool loadConfigFile(const std::string& filepath);

        // Cheats global state
        bool areCheatsEnabled() const;
        void setCheatsEnabled(bool enabled);

        // Reset to initial
        void resetAllToDefault();

    private:
        ConVarRegistry();
        ~ConVarRegistry() = default;

        std::unordered_map<std::string, std::unique_ptr<ConVar>> _convars;
        std::unordered_map<std::string, ConCommand> _commands;
        std::unordered_map<std::string, std::string> _aliases;
        bool _cheatsEnabled = false;
        PrintSink _activePrintSink = nullptr;

        void populateDefaultSourceCommands();
    };

    // Global convenient Modder API functions
    LAB_CORE_API ConVar* RegisterConVar(const std::string& name, const std::string& defaultValue, uint32_t flags,
                                        const std::string& helpText, ConVarChangeCallback onChange = nullptr);
    LAB_CORE_API bool RegisterConCommand(const std::string& name, ConCommandCallback callback, uint32_t flags,
                                         const std::string& helpText);
    LAB_CORE_API ConVar* FindConVar(const std::string& name);
    LAB_CORE_API void ConPrint(const std::string& text, const Vec3& color = Vec3(0.9f, 0.9f, 0.9f));

} // namespace Lab
