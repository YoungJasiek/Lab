#include "LabConVar.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>

namespace Lab {

    ConVar::ConVar(const std::string& name, const std::string& defaultValue, uint32_t flags,
                   const std::string& helpText, bool hasMin, float minVal,
                   bool hasMax, float maxVal, ConVarChangeCallback onChange)
        : _name(name), _value(defaultValue), _defaultValue(defaultValue),
          _helpText(helpText), _flags(flags), _hasMin(hasMin), _minVal(minVal),
          _hasMax(hasMax), _maxVal(maxVal), _onChange(std::move(onChange)) {
        updateParsedValues();
    }

    void ConVar::updateParsedValues() {
        try {
            _floatValue = std::stof(_value);
            _intValue = std::stoi(_value);
        } catch (...) {
            _floatValue = 0.0f;
            _intValue = 0;
        }

        std::string lowerVal = _value;
        std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        _boolValue = (lowerVal == "1" || lowerVal == "true" || lowerVal == "yes" || lowerVal == "on");
    }

    void ConVar::setValue(const std::string& value) {
        std::string oldVal = _value;
        _value = value;
        updateParsedValues();

        if (_hasMin && _floatValue < _minVal) {
            _floatValue = _minVal;
            _intValue = static_cast<int>(_minVal);
            _value = std::to_string(_minVal);
        }
        if (_hasMax && _floatValue > _maxVal) {
            _floatValue = _maxVal;
            _intValue = static_cast<int>(_maxVal);
            _value = std::to_string(_maxVal);
        }

        if (_onChange && oldVal != _value) {
            _onChange(this, oldVal, _value);
        }
    }

    void ConVar::setValue(float value) {
        if (_hasMin && value < _minVal) value = _minVal;
        if (_hasMax && value > _maxVal) value = _maxVal;
        setValue(std::to_string(value));
    }

    void ConVar::setValue(int value) {
        if (_hasMin && static_cast<float>(value) < _minVal) value = static_cast<int>(_minVal);
        if (_hasMax && static_cast<float>(value) > _maxVal) value = static_cast<int>(_maxVal);
        setValue(std::to_string(value));
    }

    void ConVar::resetToDefault() {
        setValue(_defaultValue);
    }

    std::string ConVar::getFlagsString() const {
        std::string str;
        if (_flags & FCVAR_ARCHIVE) str += " a";
        if (_flags & FCVAR_CHEAT) str += " cheat";
        if (_flags & FCVAR_REPLICATED) str += " rep";
        if (_flags & FCVAR_USERINFO) str += " user";
        if (_flags & FCVAR_NOTIFY) str += " nf";
        if (_flags & FCVAR_SERVER_CAN_EXECUTE) str += " server_can_execute";
        if (_flags & FCVAR_CLIENTCMD_CAN_EXECUTE) str += " clientcmd_can_execute";
        if (_flags & FCVAR_PROTECTED) str += " prot";
        if (_flags & FCVAR_PRINTABLEONLY) str += " print";
        if (_flags & FCVAR_DEVELOPMENTONLY) str += " devonly";
        if (_flags & FCVAR_MOD) str += " mod";
        if (_flags & FCVAR_CLIENTDLL) str += " cl";
        if (_flags & FCVAR_GAMEDLL) str += " sv";
        if (!str.empty() && str.front() == ' ') str.erase(0, 1);
        return str;
    }

    // =========================================================================
    // ConVarRegistry Singleton & Management
    // =========================================================================

    ConVarRegistry& ConVarRegistry::get() {
        static ConVarRegistry instance;
        return instance;
    }

    ConVarRegistry::ConVarRegistry() {
        populateDefaultSourceCommands();
    }

    ConVar* ConVarRegistry::registerConVar(const std::string& name, const std::string& defaultValue, uint32_t flags,
                                          const std::string& helpText, bool hasMin, float minVal,
                                          bool hasMax, float maxVal, ConVarChangeCallback onChange) {
        auto it = _convars.find(name);
        if (it != _convars.end()) {
            return it->second.get();
        }

        auto cv = std::make_unique<ConVar>(name, defaultValue, flags, helpText, hasMin, minVal, hasMax, maxVal, onChange);
        ConVar* ptr = cv.get();
        _convars[name] = std::move(cv);
        return ptr;
    }

    bool ConVarRegistry::registerConCommand(const std::string& name, ConCommandCallback callback, uint32_t flags,
                                           const std::string& helpText) {
        _commands[name] = ConCommand{ name, std::move(callback), helpText, flags };
        return true;
    }

    bool ConVarRegistry::unregister(const std::string& name) {
        auto itCv = _convars.find(name);
        if (itCv != _convars.end()) {
            _convars.erase(itCv);
            return true;
        }
        auto itCmd = _commands.find(name);
        if (itCmd != _commands.end()) {
            _commands.erase(itCmd);
            return true;
        }
        return false;
    }

    ConVar* ConVarRegistry::findConVar(const std::string& name) {
        auto it = _convars.find(name);
        return it != _convars.end() ? it->second.get() : nullptr;
    }

    const ConVar* ConVarRegistry::findConVar(const std::string& name) const {
        auto it = _convars.find(name);
        return it != _convars.end() ? it->second.get() : nullptr;
    }

    const ConCommand* ConVarRegistry::findConCommand(const std::string& name) const {
        auto it = _commands.find(name);
        return it != _commands.end() ? &it->second : nullptr;
    }

    bool ConVarRegistry::hasCommandOrVar(const std::string& name) const {
        return _convars.find(name) != _convars.end() || _commands.find(name) != _commands.end();
    }

    size_t ConVarRegistry::getTotalCommandCount() const {
        return _convars.size() + _commands.size();
    }

    std::vector<std::string> ConVarRegistry::getAllNames() const {
        std::vector<std::string> names;
        names.reserve(_convars.size() + _commands.size());
        for (const auto& [name, _] : _convars) names.push_back(name);
        for (const auto& [name, _] : _commands) names.push_back(name);
        std::sort(names.begin(), names.end());
        return names;
    }

    std::vector<std::string> ConVarRegistry::getAutocompleteSuggestions(const std::string& prefix, size_t maxCount) const {
        std::vector<std::string> res;
        if (prefix.empty()) return res;

        std::string query = prefix;
        std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        auto all = getAllNames();
        for (const auto& name : all) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            if (lowerName.find(query) == 0) {
                res.push_back(name);
                if (res.size() >= maxCount) break;
            }
        }
        return res;
    }

    bool ConVarRegistry::areCheatsEnabled() const {
        const auto* cv = findConVar("sv_cheats");
        if (cv) return cv->getBool();
        return _cheatsEnabled;
    }

    void ConVarRegistry::setCheatsEnabled(bool enabled) {
        _cheatsEnabled = enabled;
        auto* cv = findConVar("sv_cheats");
        if (cv) cv->setValue(enabled ? "1" : "0");
        auto* spCv = findConVar("sp_cheats");
        if (spCv) spCv->setValue(enabled ? "1" : "0");
    }

    void ConVarRegistry::resetAllToDefault() {
        for (auto& [_, cv] : _convars) {
            cv->resetToDefault();
        }
    }

    static std::vector<std::string> tokenizeCmdLine(const std::string& cmdLine) {
        std::vector<std::string> tokens;
        std::string cur;
        bool inQuotes = false;

        for (size_t i = 0; i < cmdLine.size(); ++i) {
            char c = cmdLine[i];
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (std::isspace(static_cast<unsigned char>(c)) && !inQuotes) {
                if (!cur.empty()) {
                    tokens.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) {
            tokens.push_back(cur);
        }
        return tokens;
    }

    bool ConVarRegistry::execute(const std::string& cmdLine, std::function<void(const std::string&, const Vec3&)> printFn) {
        auto print = [&](const std::string& text, const Vec3& col = Vec3(0.9f, 0.9f, 0.9f)) {
            if (printFn) printFn(text, col);
        };

        if (cmdLine.empty()) return false;

        std::stringstream ss(cmdLine);
        std::string segment;
        while (std::getline(ss, segment, ';')) {
            size_t start = segment.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            size_t end = segment.find_last_not_of(" \t\r\n");
            std::string subCmd = segment.substr(start, end - start + 1);

            auto tokens = tokenizeCmdLine(subCmd);
            if (tokens.empty()) continue;

            std::string cmdName = tokens[0];
            std::transform(cmdName.begin(), cmdName.end(), cmdName.begin(), [](unsigned char c){ return (char)std::tolower(c); });

            // Alias
            auto itAlias = _aliases.find(cmdName);
            if (itAlias != _aliases.end()) {
                execute(itAlias->second, printFn);
                continue;
            }

            // Builtin meta commands
            if (cmdName == "echo") {
                std::string msg;
                for (size_t i = 1; i < tokens.size(); ++i) {
                    if (i > 1) msg += " ";
                    msg += tokens[i];
                }
                print(msg, Vec3(0.95f, 0.95f, 0.95f));
                continue;
            }

            if (cmdName == "alias") {
                if (tokens.size() >= 3) {
                    _aliases[tokens[1]] = tokens[2];
                    print("Alias '" + tokens[1] + "' set to: " + tokens[2], Vec3(0.5f, 0.9f, 0.5f));
                } else if (tokens.size() == 2) {
                    auto itA = _aliases.find(tokens[1]);
                    if (itA != _aliases.end()) print("Current alias '" + tokens[1] + "': " + itA->second);
                    else print("Alias '" + tokens[1] + "' not found.", Vec3(1.0f, 0.4f, 0.4f));
                }
                continue;
            }

            if (cmdName == "unalias") {
                if (tokens.size() >= 2) {
                    _aliases.erase(tokens[1]);
                    print("Unaliased '" + tokens[1] + "'");
                }
                continue;
            }

            if (cmdName == "toggle") {
                if (tokens.size() >= 2) {
                    ConVar* cv = findConVar(tokens[1]);
                    if (cv) {
                        if (cv->isCheat() && !areCheatsEnabled()) {
                            print("Can't toggle cheat ConVar '" + cv->getName() + "' without sv_cheats 1.", Vec3(1.0f, 0.35f, 0.35f));
                            continue;
                        }
                        if (tokens.size() == 2) {
                            cv->setValue(cv->getBool() ? "0" : "1");
                        } else {
                            // Cycle through values
                            std::string cur = cv->getString();
                            size_t nextIdx = 2;
                            for (size_t i = 2; i < tokens.size(); ++i) {
                                if (tokens[i] == cur) {
                                    nextIdx = (i + 1 < tokens.size()) ? (i + 1) : 2;
                                    break;
                                }
                            }
                            cv->setValue(tokens[nextIdx]);
                        }
                        print(cv->getName() + " toggled to " + cv->getString(), Vec3(0.4f, 0.9f, 0.5f));
                    }
                }
                continue;
            }

            if (cmdName == "incrementvar") {
                if (tokens.size() >= 5) {
                    ConVar* cv = findConVar(tokens[1]);
                    if (cv) {
                        if (cv->isCheat() && !areCheatsEnabled()) {
                            print("Can't increment cheat ConVar '" + cv->getName() + "' without sv_cheats 1.", Vec3(1.0f, 0.35f, 0.35f));
                            continue;
                        }
                        float minV = std::stof(tokens[2]);
                        float maxV = std::stof(tokens[3]);
                        float delta = std::stof(tokens[4]);
                        float cur = cv->getFloat() + delta;
                        if (cur > maxV) cur = minV;
                        if (cur < minV) cur = minV;
                        cv->setValue(cur);
                        print(cv->getName() + " incremented to " + cv->getString(), Vec3(0.4f, 0.9f, 0.5f));
                    }
                }
                continue;
            }

            if (cmdName == "cvarlist") {
                std::string filter = tokens.size() > 1 ? tokens[1] : "";
                auto list = getCvarList(filter);
                print("---------- CVAR / COMMAND LIST (" + std::to_string(list.size()) + " matches) ----------", Vec3(1.0f, 0.85f, 0.2f));
                for (const auto& line : list) {
                    print(line, Vec3(0.85f, 0.85f, 0.85f));
                }
                print("--------------------------------------------------", Vec3(1.0f, 0.85f, 0.2f));
                continue;
            }

            if (cmdName == "find") {
                if (tokens.size() > 1) {
                    auto list = getCvarList(tokens[1]);
                    print("Found " + std::to_string(list.size()) + " matching commands/convars for '" + tokens[1] + "':", Vec3(1.0f, 0.85f, 0.2f));
                    for (const auto& line : list) print(line);
                }
                continue;
            }

            if (cmdName == "help") {
                if (tokens.size() > 1) {
                    const auto* cv = findConVar(tokens[1]);
                    if (cv) {
                        print("ConVar: " + cv->getName(), Vec3(1.0f, 0.84f, 0.22f));
                        print("Value: " + cv->getString() + " (Default: " + cv->getDefault() + ")");
                        if (cv->hasMin() || cv->hasMax()) print("Bounds: [" + std::to_string(cv->getMin()) + " .. " + std::to_string(cv->getMax()) + "]");
                        print("Flags: [" + cv->getFlagsString() + "]");
                        print("Description: " + cv->getHelpText());
                    } else {
                        const auto* cmd = findConCommand(tokens[1]);
                        if (cmd) {
                            print("Command: " + cmd->name, Vec3(1.0f, 0.84f, 0.22f));
                            print("Description: " + cmd->helpText);
                        } else {
                            print("Unknown command or convar '" + tokens[1] + "'.", Vec3(1.0f, 0.35f, 0.35f));
                        }
                    }
                }
                continue;
            }

            if (cmdName == "differences") {
                auto diffs = getDifferences();
                print("ConVars modified from default (" + std::to_string(diffs.size()) + "):", Vec3(1.0f, 0.85f, 0.2f));
                for (const auto& d : diffs) print("  " + d);
                continue;
            }

            if (cmdName == "host_writeconfig") {
                std::string fname = tokens.size() > 1 ? tokens[1] : "config.cfg";
                std::ofstream out(fname);
                if (out.is_open()) {
                    out << writeConfigFile();
                    print("Wrote configuration to: " + fname, Vec3(0.4f, 0.9f, 0.5f));
                } else {
                    print("Failed to open " + fname + " for writing.", Vec3(1.0f, 0.35f, 0.35f));
                }
                continue;
            }

            // 1. ConCommand execution
            auto itCmd = _commands.find(cmdName);
            if (itCmd != _commands.end()) {
                if ((itCmd->second.flags & FCVAR_CHEAT) && !areCheatsEnabled()) {
                    print("Can't use cheat command '" + cmdName + "' in multiplayer without sv_cheats 1.", Vec3(1.0f, 0.35f, 0.35f));
                    continue;
                }
                itCmd->second.callback(tokens);
                continue;
            }

            // 2. ConVar query or assignment
            auto itCv = _convars.find(cmdName);
            if (itCv != _convars.end()) {
                ConVar* cv = itCv->second.get();
                if (tokens.size() == 1) {
                    std::string info = "\"" + cv->getName() + "\" = \"" + cv->getString() + "\"";
                    if (cv->getString() != cv->getDefault()) {
                        info += " ( def. \"" + cv->getDefault() + "\" )";
                    }
                    if (cv->hasMin() || cv->hasMax()) {
                        info += " min. " + std::to_string(cv->getMin()) + " max. " + std::to_string(cv->getMax());
                    }
                    std::string flags = cv->getFlagsString();
                    if (!flags.empty()) {
                        info += " [" + flags + "]";
                    }
                    print(info, Vec3(0.85f, 0.90f, 0.95f));
                    if (!cv->getHelpText().empty()) {
                        print(" - " + cv->getHelpText(), Vec3(0.65f, 0.70f, 0.75f));
                    }
                } else {
                    if (cv->isCheat() && !areCheatsEnabled()) {
                        print("Can't change cheat ConVar '" + cmdName + "' without sv_cheats 1.", Vec3(1.0f, 0.35f, 0.35f));
                        continue;
                    }
                    cv->setValue(tokens[1]);
                    print(cv->getName() + " set to " + cv->getString(), Vec3(0.4f, 0.9f, 0.5f));
                }
                continue;
            }

            print("Unknown command or convar: '" + tokens[0] + "'. Type 'cvarlist' or 'help' for full command index.", Vec3(1.0f, 0.45f, 0.45f));
        }

        return true;
    }

    std::vector<std::string> ConVarRegistry::getCvarList(const std::string& filter) const {
        std::vector<std::string> res;
        std::string fLower = filter;
        std::transform(fLower.begin(), fLower.end(), fLower.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        auto all = getAllNames();
        for (const auto& name : all) {
            if (!fLower.empty()) {
                std::string nLower = name;
                std::transform(nLower.begin(), nLower.end(), nLower.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (nLower.find(fLower) == std::string::npos) continue;
            }

            const auto* cv = findConVar(name);
            if (cv) {
                std::string line = cv->getName() + " : " + cv->getString();
                std::string flags = cv->getFlagsString();
                if (!flags.empty()) line += " [" + flags + "]";
                if (!cv->getHelpText().empty()) line += " : " + cv->getHelpText();
                res.push_back(line);
            } else {
                const auto* cmd = findConCommand(name);
                if (cmd) {
                    std::string line = cmd->name + " : cmd";
                    if (cmd->flags & FCVAR_CHEAT) line += " [cheat]";
                    if (cmd->flags & FCVAR_MOD) line += " [mod]";
                    if (!cmd->helpText.empty()) line += " : " + cmd->helpText;
                    res.push_back(line);
                }
            }
        }
        return res;
    }

    std::vector<std::string> ConVarRegistry::getDifferences() const {
        std::vector<std::string> diffs;
        for (const auto& [name, cv] : _convars) {
            if (cv->getString() != cv->getDefault()) {
                diffs.push_back(name + " is \"" + cv->getString() + "\" (default: \"" + cv->getDefault() + "\")");
            }
        }
        return diffs;
    }

    std::string ConVarRegistry::writeConfigFile() const {
        std::stringstream ss;
        ss << "// Lab Engine Configuration File (Generated by host_writeconfig)\n";
        ss << "unbindall\n";

        for (const auto& [name, cv] : _convars) {
            if (cv->isArchive()) {
                ss << name << " \"" << cv->getString() << "\"\n";
            }
        }
        return ss.str();
    }

bool ConVarRegistry::loadConfigFile(const std::string& filepath) {
        std::ifstream f(filepath);
        if (!f.is_open()) return false;

        std::string line;
        while (std::getline(f, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos || line[start] == '/' || line[start] == '#') continue;
            execute(line);
        }
        return true;
    }

    ConVar* RegisterConVar(const std::string& name, const std::string& defaultValue, uint32_t flags,
                           const std::string& helpText, ConVarChangeCallback onChange) {
        return ConVarRegistry::get().registerConVar(name, defaultValue, flags, helpText, false, 0.0f, false, 0.0f, onChange);
    }

    bool RegisterConCommand(const std::string& name, ConCommandCallback callback, uint32_t flags,
                            const std::string& helpText) {
        return ConVarRegistry::get().registerConCommand(name, std::move(callback), flags, helpText);
    }

    ConVar* FindConVar(const std::string& name) {
        return ConVarRegistry::get().findConVar(name);
    }

    void ConVarRegistry::populateDefaultSourceCommands() {
        // =====================================================================
        // Built-in Source Engine Database (1435 commands and ConVars)
        // =====================================================================

        // 1. ConCommands
        registerConCommand("clear", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Clear all developer console output text");
        registerConCommand("cls", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Clear all developer console output text");
        registerConCommand("echo", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Echo text to console");
        registerConCommand("echoln", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Echo text to console followed by newline");
        registerConCommand("exec", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Execute a script file of console commands");
        registerConCommand("status", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Display host server and client network status");
        registerConCommand("version", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Print engine build version, commit, protocol and date");
        registerConCommand("alias", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Alias a command script");
        registerConCommand("unalias", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Remove an aliased command");
        registerConCommand("bind", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_ARCHIVE, "Bind a key to an executable command string");
        registerConCommand("unbind", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_ARCHIVE, "Unbind a key");
        registerConCommand("unbindall", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_ARCHIVE, "Unbind all keys");
        registerConCommand("BindToggle", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_ARCHIVE, "Bind a key to toggle a boolean cvar");
        registerConCommand("quit", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Exit the engine");
        registerConCommand("exit", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Exit the engine");
        registerConCommand("cvarlist", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Show the list of convars and concommands with flags and values");
        registerConCommand("find", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Find convars/concommands matching a substring");
        registerConCommand("findflags", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Find convars/concommands with specified flags");
        registerConCommand("help", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Display help and description for a console command or variable");
        registerConCommand("differences", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Show all convars whose values differ from default");
        registerConCommand("toggle", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Toggle a cvar between values or 0/1");
        registerConCommand("incrementvar", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Increment a cvar value between min and max");
        registerConCommand("multvar", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Multiply a cvar value");
        registerConCommand("wait", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Wait a single frame before continuing script");
        registerConCommand("condump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Dump console buffer to text file");
        registerConCommand("copylog", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Copy entire console log to system clipboard");
        registerConCommand("cmd", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Send a command string directly to server");
        registerConCommand("stuffcmds", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Execute commandline arguments from engine boot");
        registerConCommand("reset", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Reset a cvar to its default value");
        registerConCommand("host_writeconfig", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Store current archived convars to config.cfg");
        registerConCommand("god", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Toggle invincible god mode");
        registerConCommand("noclip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Toggle noclip flythrough mode");
        registerConCommand("notarget", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Toggle enemies ignoring player");
        registerConCommand("buddha", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Player takes damage but health never drops below 1");
        registerConCommand("give", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Give weapon or item to player");
        registerConCommand("givecurrentammo", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Give max ammo for currently active weapon");
        registerConCommand("impulse", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Execute game impulse script (e.g. 101 for all weapons)");
        registerConCommand("kill", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CLIENTCMD_CAN_EXECUTE, "Commit suicide");
        registerConCommand("hurtme", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Inflict specified damage on player");
        registerConCommand("drop", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CLIENTCMD_CAN_EXECUTE, "Drop active weapon");
        registerConCommand("killserver", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Shutdown current game server session");
        registerConCommand("suicide", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CLIENTCMD_CAN_EXECUTE, "Commit suicide");
        registerConCommand("respawn", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CLIENTCMD_CAN_EXECUTE, "Respawn player at valid spawn point");
        registerConCommand("map", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Load a game sector map");
        registerConCommand("changelevel", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Change current level map");
        registerConCommand("restart", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Restart current map round");
        registerConCommand("connect", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Connect to a remote multiplayer server IP:Port");
        registerConCommand("disconnect", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Disconnect from current multiplayer session");
        registerConCommand("retry", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Retry connecting to the most recent server");
        registerConCommand("ping", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Query current round-trip time to host server");
        registerConCommand("say", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CLIENTCMD_CAN_EXECUTE, "Display player chat message to all players");
        registerConCommand("say_team", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CLIENTCMD_CAN_EXECUTE, "Display player chat message to team only");
        registerConCommand("tell", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CLIENTCMD_CAN_EXECUTE, "Send private message to a player");
        registerConCommand("kick", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Kick a client by name or ID");
        registerConCommand("kickid", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Kick a client by UserID");
        registerConCommand("banid", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Ban a client by SteamID / PlayerID");
        registerConCommand("banip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Ban a client by IP address");
        registerConCommand("addip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Add an IP to server banlist");
        registerConCommand("removeip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Remove an IP from server banlist");
        registerConCommand("listip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "List all IP bans on server");
        registerConCommand("writeip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Save IP banlist to banned_ip.cfg");
        registerConCommand("listid", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "List all ID bans on server");
        registerConCommand("writeid", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_SERVER_CAN_EXECUTE, "Save ID banlist to banned_user.cfg");
        registerConCommand("stopsound", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Stop all currently playing sound effects");
        registerConCommand("soundinfo", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Print sound device, audio buffer and channel telemetry");
        registerConCommand("soundlist", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Print list of cached and active sound samples");
        registerConCommand("displaysoundlist", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Toggle live sound sample list overlay");
        registerConCommand("snd_restart", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Restart the entire audio subsystem");
        registerConCommand("play", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Play a wav/ogg/mp3 sound file");
        registerConCommand("playgamesound", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Play a game soundscape sound entry");
        registerConCommand("soundscape_flush", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Flush and reload ambient soundscapes");
        registerConCommand("vprof", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Toggle VProf hierarchical CPU profiler");
        registerConCommand("vprof_on", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Turn on VProf profiler");
        registerConCommand("vprof_off", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Turn off VProf profiler");
        registerConCommand("vprof_reset", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Reset VProf profiler samples");
        registerConCommand("vprof_generate_report", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Generate hierarchical CPU profiler report");
        registerConCommand("vprof_dump_spikes", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Dump frame spikes exceeding threshold");
        registerConCommand("picker", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Toggle HUD crosshair entity picker inspection");
        registerConCommand("ent_create", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Spawn entity of specified class at aim position");
        registerConCommand("ent_fire", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Fire logic input into entity (ent_fire <target> <input> [param])");
        registerConCommand("ent_dump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Dump all internal state variables of targeted entity");
        registerConCommand("ent_bbox", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Display bounding box of targeted entity");
        registerConCommand("ent_absbox", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Display absolute axis-aligned box of targeted entity");
        registerConCommand("ent_text", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Display text overlay above entities");
        registerConCommand("ent_remove", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Remove targeted entity from world");
        registerConCommand("ent_remove_all", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Remove all entities of specified class");
        registerConCommand("ent_teleport", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Teleport targeted entity to aim position");
        registerConCommand("respawn_entities", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Reset and respawn all map entities to initial state");
        registerConCommand("showtriggers_toggle", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Toggle trigger brush wireframe overlay");
        registerConCommand("timerefresh", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Profile rendering speed by spinning camera 360 degrees");
        registerConCommand("mem_dump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Dump current memory allocations to file");
        registerConCommand("mem_dumpstats", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Print current heap and working set memory usage");
        registerConCommand("mem_compact", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Compact and defragment heap memory");
        registerConCommand("mem_force_flush", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Flush all unlocked assets and cache buffers");
        registerConCommand("sys_info", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Display hardware, CPU architecture, GPU, OS and RAM info");
        registerConCommand("mod_list", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "List all mounted addons, scripts and modifications");
        registerConCommand("mod_reload", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Reload all active mod scripts and VFS packages");
        registerConCommand("addon_reload", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Reload mounted addons directory");
        registerConCommand("lua_run", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Execute arbitrary Lua string in game runtime");
        registerConCommand("lua_reload", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Reload main Lua game mechanics script");
        registerConCommand("lua_openscript", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Open and execute a Lua script file on server");
        registerConCommand("lua_openscript_cl", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Open and execute a Lua script file on client");
        registerConCommand("+attack", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Fire primary weapon");
        registerConCommand("-attack", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release primary weapon");
        registerConCommand("+attack2", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Fire secondary weapon action");
        registerConCommand("-attack2", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release secondary weapon action");
        registerConCommand("+attack3", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Special weapon action");
        registerConCommand("-attack3", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release special weapon action");
        registerConCommand("+jump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Jump / Hop");
        registerConCommand("-jump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release jump");
        registerConCommand("+duck", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Crouch down");
        registerConCommand("-duck", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release crouch");
        registerConCommand("+forward", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Move forward");
        registerConCommand("-forward", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop moving forward");
        registerConCommand("+back", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Move backward");
        registerConCommand("-back", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop moving backward");
        registerConCommand("+left", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Turn left");
        registerConCommand("-left", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop turning left");
        registerConCommand("+right", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Turn right");
        registerConCommand("-right", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop turning right");
        registerConCommand("+moveleft", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Strafe left");
        registerConCommand("-moveleft", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop strafing left");
        registerConCommand("+moveright", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Strafe right");
        registerConCommand("-moveright", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop strafing right");
        registerConCommand("+speed", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Walk / Sneak");
        registerConCommand("-speed", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop walking");
        registerConCommand("+sprint", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Sprint / Boost");
        registerConCommand("-sprint", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Stop sprinting");
        registerConCommand("+use", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Interact / Use object or door");
        registerConCommand("-use", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release interact");
        registerConCommand("+reload", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Reload active weapon");
        registerConCommand("-reload", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release reload");
        registerConCommand("+score", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Show match scoreboard");
        registerConCommand("-score", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Hide match scoreboard");
        registerConCommand("+showscores", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Show match scoreboard");
        registerConCommand("-showscores", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Hide match scoreboard");
        registerConCommand("+voicerecord", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Activate voice communication");
        registerConCommand("-voicerecord", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Deactivate voice communication");
        registerConCommand("+zoom", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Optical zoom view");
        registerConCommand("-zoom", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_NONE, "Release optical zoom view");
        registerConCommand("nav_add_to_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_add_to_selected_set");
        registerConCommand("nav_analyze", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_analyze");
        registerConCommand("nav_begin_area", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_begin_area");
        registerConCommand("nav_begin_deselecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_begin_deselecting");
        registerConCommand("nav_begin_drag_deselecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_begin_drag_deselecting");
        registerConCommand("nav_begin_drag_selecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_begin_drag_selecting");
        registerConCommand("nav_begin_selecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_begin_selecting");
        registerConCommand("nav_build_ladder", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_build_ladder");
        registerConCommand("nav_check_file_consistency", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_check_file_consistency");
        registerConCommand("nav_check_floor", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_check_floor");
        registerConCommand("nav_check_stairs", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_check_stairs");
        registerConCommand("nav_chop_selected", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_chop_selected");
        registerConCommand("nav_clear_attribute_cache", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_clear_attribute_cache");
        registerConCommand("nav_clear_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_clear_selected_set");
        registerConCommand("nav_clear_walkable_seeds", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_clear_walkable_seeds");
        registerConCommand("nav_compress_id", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_compress_id");
        registerConCommand("nav_connect", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_connect");
        registerConCommand("nav_coplanar_slope_limit", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_coplanar_slope_limit");
        registerConCommand("nav_coplanar_slope_limit_displacement", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_coplanar_slope_limit_displacement");
        registerConCommand("nav_corner_adjust_morton", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_corner_adjust_morton");
        registerConCommand("nav_corner_lower", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_corner_lower");
        registerConCommand("nav_corner_place_object", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_corner_place_object");
        registerConCommand("nav_corner_raise", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_corner_raise");
        registerConCommand("nav_corner_select", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_corner_select");
        registerConCommand("nav_create_area_at_feet", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_create_area_at_feet");
        registerConCommand("nav_create_place_on_ground", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_create_place_on_ground");
        registerConCommand("nav_crouch", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_crouch");
        registerConCommand("nav_crouch_mark", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_crouch_mark");
        registerConCommand("nav_debug_blocked", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_debug_blocked");
        registerConCommand("nav_delete", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_delete");
        registerConCommand("nav_delete_marked", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_delete_marked");
        registerConCommand("nav_disconnect", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_disconnect");
        registerConCommand("nav_disconnect_outgoing_oneway", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_disconnect_outgoing_oneway");
        registerConCommand("nav_displacement_test", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_displacement_test");
        registerConCommand("nav_dont_hide", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_dont_hide");
        registerConCommand("nav_drag_selection_volume_z_max_offset", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_drag_selection_volume_z_max_offset");
        registerConCommand("nav_drag_selection_volume_z_min_offset", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_drag_selection_volume_z_min_offset");
        registerConCommand("nav_draw_limit", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_draw_limit");
        registerConCommand("nav_edit", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_edit");
        registerConCommand("nav_end_area", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_end_area");
        registerConCommand("nav_end_deselecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_end_deselecting");
        registerConCommand("nav_end_drag_deselecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_end_drag_deselecting");
        registerConCommand("nav_end_drag_selecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_end_drag_selecting");
        registerConCommand("nav_end_selecting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_end_selecting");
        registerConCommand("nav_flood_select", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_flood_select");
        registerConCommand("nav_gen_cliffs_approx", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_gen_cliffs_approx");
        registerConCommand("nav_generate", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_generate");
        registerConCommand("nav_generate_fencetops", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_generate_fencetops");
        registerConCommand("nav_generate_fixup_jump_areas", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_generate_fixup_jump_areas");
        registerConCommand("nav_generate_incremental", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_generate_incremental");
        registerConCommand("nav_generate_incremental_range", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_generate_incremental_range");
        registerConCommand("nav_generate_incremental_tolerance", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_generate_incremental_tolerance");
        registerConCommand("nav_jump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_jump");
        registerConCommand("nav_ladder_flip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_ladder_flip");
        registerConCommand("nav_load", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_load");
        registerConCommand("nav_lower_drag_volume_max", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_lower_drag_volume_max");
        registerConCommand("nav_lower_drag_volume_min", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_lower_drag_volume_min");
        registerConCommand("nav_make_sniper_spots", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_make_sniper_spots");
        registerConCommand("nav_mark", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_mark");
        registerConCommand("nav_mark_attribute", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_mark_attribute");
        registerConCommand("nav_mark_unnamed", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_mark_unnamed");
        registerConCommand("nav_mark_walkable", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_mark_walkable");
        registerConCommand("nav_merge", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_merge");
        registerConCommand("nav_merge_mesh", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_merge_mesh");
        registerConCommand("nav_no_hostages", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_no_hostages");
        registerConCommand("nav_no_jump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_no_jump");
        registerConCommand("nav_place_floodfill", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_place_floodfill");
        registerConCommand("nav_place_list", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_place_list");
        registerConCommand("nav_place_pick", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_place_pick");
        registerConCommand("nav_place_replace", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_place_replace");
        registerConCommand("nav_place_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_place_set");
        registerConCommand("nav_precise", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_precise");
        registerConCommand("nav_quicksave", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_quicksave");
        registerConCommand("nav_raise_drag_volume_max", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_raise_drag_volume_max");
        registerConCommand("nav_raise_drag_volume_min", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_raise_drag_volume_min");
        registerConCommand("nav_recall_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_recall_selected_set");
        registerConCommand("nav_remove_from_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_remove_from_selected_set");
        registerConCommand("nav_remove_unused_jumping_areas", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_remove_unused_jumping_areas");
        registerConCommand("nav_run", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_run");
        registerConCommand("nav_save", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_save");
        registerConCommand("nav_save_selected", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_save_selected");
        registerConCommand("nav_select_blocked_areas", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_blocked_areas");
        registerConCommand("nav_select_damaging_areas", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_damaging_areas");
        registerConCommand("nav_select_half_space", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_half_space");
        registerConCommand("nav_select_invalid_areas", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_invalid_areas");
        registerConCommand("nav_select_obstructed_areas", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_obstructed_areas");
        registerConCommand("nav_select_orphans", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_orphans");
        registerConCommand("nav_select_overlapping", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_overlapping");
        registerConCommand("nav_select_stairs", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_select_stairs");
        registerConCommand("nav_set_place_mode", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_set_place_mode");
        registerConCommand("nav_shift", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_shift");
        registerConCommand("nav_show_approach_points", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_approach_points");
        registerConCommand("nav_show_area_info", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_area_info");
        registerConCommand("nav_show_compass", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_compass");
        registerConCommand("nav_show_contingent_bounds", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_contingent_bounds");
        registerConCommand("nav_show_danger", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_danger");
        registerConCommand("nav_show_dumped_areas", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_dumped_areas");
        registerConCommand("nav_show_func_nav_avoid", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_func_nav_avoid");
        registerConCommand("nav_show_func_nav_prefer", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_func_nav_prefer");
        registerConCommand("nav_show_light_intensity", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_light_intensity");
        registerConCommand("nav_show_node_grid", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_node_grid");
        registerConCommand("nav_show_node_id", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_node_id");
        registerConCommand("nav_show_nodes", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_nodes");
        registerConCommand("nav_show_player_counts", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_player_counts");
        registerConCommand("nav_show_pot_pos", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_pot_pos");
        registerConCommand("nav_show_substance_color", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_show_substance_color");
        registerConCommand("nav_simplify_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_simplify_selected_set");
        registerConCommand("nav_slope_limit", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_slope_limit");
        registerConCommand("nav_slope_tolerance", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_slope_tolerance");
        registerConCommand("nav_smooth_half_space", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_smooth_half_space");
        registerConCommand("nav_snap_to_grid", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_snap_to_grid");
        registerConCommand("nav_solid_props", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_solid_props");
        registerConCommand("nav_splice", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_splice");
        registerConCommand("nav_split", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_split");
        registerConCommand("nav_split_editing", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_split_editing");
        registerConCommand("nav_stand", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_stand");
        registerConCommand("nav_stop", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_stop");
        registerConCommand("nav_store_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_store_selected_set");
        registerConCommand("nav_strip", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_strip");
        registerConCommand("nav_subdivide", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_subdivide");
        registerConCommand("nav_test_node", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_test_node");
        registerConCommand("nav_test_node_crouch", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_test_node_crouch");
        registerConCommand("nav_test_node_crouch_dir", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_test_node_crouch_dir");
        registerConCommand("nav_test_stairs", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_test_stairs");
        registerConCommand("nav_toggle_attribute", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_toggle_attribute");
        registerConCommand("nav_toggle_deselected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_toggle_deselected_set");
        registerConCommand("nav_toggle_in_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_toggle_in_selected_set");
        registerConCommand("nav_toggle_place_mode", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_toggle_place_mode");
        registerConCommand("nav_toggle_place_painting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_toggle_place_painting");
        registerConCommand("nav_toggle_selected_set", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_toggle_selected_set");
        registerConCommand("nav_transient", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_transient");
        registerConCommand("nav_unmark", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_unmark");
        registerConCommand("nav_update_blocked_mesh", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_update_blocked_mesh");
        registerConCommand("nav_update_lighting", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_update_lighting");
        registerConCommand("nav_use_place", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_use_place");
        registerConCommand("nav_warp_to_mark", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_warp_to_mark");
        registerConCommand("nav_world_center", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Navigation mesh generator command nav_world_center");
        registerConCommand("npc_ammo_deplete", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_ammo_deplete");
        registerConCommand("npc_bipeds", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_bipeds");
        registerConCommand("npc_citizen_auto_player_squad", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_auto_player_squad");
        registerConCommand("npc_citizen_auto_player_squad_allow_use", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_auto_player_squad_allow_use");
        registerConCommand("npc_citizen_dont_precache_all", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_dont_precache_all");
        registerConCommand("npc_citizen_explosive_resist", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_explosive_resist");
        registerConCommand("npc_citizen_heal_ally_min_delay", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_heal_ally_min_delay");
        registerConCommand("npc_citizen_insignia_draw_distance", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_insignia_draw_distance");
        registerConCommand("npc_citizen_medic_emit_sound", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_medic_emit_sound");
        registerConCommand("npc_citizen_squad_marker", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_citizen_squad_marker");
        registerConCommand("npc_combat", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_combat");
        registerConCommand("npc_conditions", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_conditions");
        registerConCommand("npc_create", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_create");
        registerConCommand("npc_create_aimed", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_create_aimed");
        registerConCommand("npc_create_equipment", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_create_equipment");
        registerConCommand("npc_destroy", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_destroy");
        registerConCommand("npc_destroy_unselected", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_destroy_unselected");
        registerConCommand("npc_enemies", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_enemies");
        registerConCommand("npc_enemy", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_enemy");
        registerConCommand("npc_focus", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_focus");
        registerConCommand("npc_freeze", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_freeze");
        registerConCommand("npc_freeze_unselected", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_freeze_unselected");
        registerConCommand("npc_generator", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_generator");
        registerConCommand("npc_go", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_go");
        registerConCommand("npc_go_do_run", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_go_do_run");
        registerConCommand("npc_go_random", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_go_random");
        registerConCommand("npc_heal", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_heal");
        registerConCommand("npc_height_adjust", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_height_adjust");
        registerConCommand("npc_kill", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_kill");
        registerConCommand("npc_nearest", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_nearest");
        registerConCommand("npc_relationships", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_relationships");
        registerConCommand("npc_reset", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_reset");
        registerConCommand("npc_route", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_route");
        registerConCommand("npc_select", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_select");
        registerConCommand("npc_sentences", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_sentences");
        registerConCommand("npc_speak_all", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_speak_all");
        registerConCommand("npc_squads", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_squads");
        registerConCommand("npc_steering", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_steering");
        registerConCommand("npc_steering_all", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_steering_all");
        registerConCommand("npc_task", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_task");
        registerConCommand("npc_tasks", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_tasks");
        registerConCommand("npc_teleport", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_teleport");
        registerConCommand("npc_think_now", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_think_now");
        registerConCommand("npc_view", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_view");
        registerConCommand("npc_viewfield", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command npc_viewfield");
        registerConCommand("ent_absbox", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_absbox");
        registerConCommand("ent_attachments", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_attachments");
        registerConCommand("ent_auto_revive", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_auto_revive");
        registerConCommand("ent_bbox", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_bbox");
        registerConCommand("ent_cancelpendingentfires", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_cancelpendingentfires");
        registerConCommand("ent_create", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_create");
        registerConCommand("ent_dump", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_dump");
        registerConCommand("ent_fire", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_fire");
        registerConCommand("ent_info", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_info");
        registerConCommand("ent_keyvalue", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_keyvalue");
        registerConCommand("ent_messages", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_messages");
        registerConCommand("ent_name", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_name");
        registerConCommand("ent_orient", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_orient");
        registerConCommand("ent_pause", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_pause");
        registerConCommand("ent_pivot", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_pivot");
        registerConCommand("ent_remove", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_remove");
        registerConCommand("ent_remove_all", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_remove_all");
        registerConCommand("ent_rotate", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_rotate");
        registerConCommand("ent_setname", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_setname");
        registerConCommand("ent_show_response_criteria", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_show_response_criteria");
        registerConCommand("ent_step", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_step");
        registerConCommand("ent_teleport", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_teleport");
        registerConCommand("ent_text", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_text");
        registerConCommand("ent_viewmodel", [](const std::vector<std::string>& args) {
            (void)args;
        }, FCVAR_CHEAT, "Entity and NPC manipulation command ent_viewmodel");

        // 2. ConVars
        registerConVar("sv_cheats", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Allow cheats on server (0=disabled, 1=enabled)", true, 0.0f, true, 1.0f);
        registerConVar("sp_cheats", "0", FCVAR_CHEAT, "Singleplayer cheat mode toggle", true, 0.0f, true, 1.0f);
        registerConVar("sp_god", "0", FCVAR_CHEAT, "Singleplayer invincibility god mode", true, 0.0f, true, 1.0f);
        registerConVar("sp_noclip", "0", FCVAR_CHEAT, "Singleplayer noclip flying mode", true, 0.0f, true, 1.0f);
        registerConVar("sp_bhop", "1", FCVAR_ARCHIVE, "Enable bunny hop momentum preservation", true, 0.0f, true, 1.0f);
        registerConVar("sp_speed", "1.0", FCVAR_CHEAT, "Player movement speed multiplier", true, 0.1f, true, 10.0f);
        registerConVar("sp_jump", "5.2", FCVAR_CHEAT, "Player jump impulse velocity", true, 1.0f, true, 20.0f);
        registerConVar("sp_heal", "100", FCVAR_CHEAT, "Heal player health by specified amount", false, 0.0f, false, 0.0f);
        registerConVar("sp_armor", "100", FCVAR_CHEAT, "Give armor points to player", false, 0.0f, false, 0.0f);
        registerConVar("sp_ammo", "100", FCVAR_CHEAT, "Give ammunition to player", false, 0.0f, false, 0.0f);
        registerConVar("sp_get", "1", FCVAR_CHEAT, "Equip weapon by ID", false, 0.0f, false, 0.0f);
        registerConVar("sp_give_all", "1", FCVAR_CHEAT, "Give all weapons and full equipment", false, 0.0f, false, 0.0f);
        registerConVar("sp_respawn", "1", FCVAR_CHEAT, "Force player respawn", false, 0.0f, false, 0.0f);
        registerConVar("sp_suicide", "1", FCVAR_CHEAT, "Kill local player", false, 0.0f, false, 0.0f);
        registerConVar("log_momentum", "0", FCVAR_NONE, "Display horizontal speed and momentum telemetry on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_fps", "0", FCVAR_NONE, "Display FPS and frametime telemetry on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_ping", "0", FCVAR_NONE, "Display ping and network roundtrip telemetry on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_pos", "0", FCVAR_NONE, "Display player XYZ coordinates and view angles on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_colliders", "0", FCVAR_CHEAT, "Display nearby solid collision boxes count on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_triggers", "0", FCVAR_CHEAT, "Display active trigger entities count on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_ai", "0", FCVAR_CHEAT, "Display AI bot states and targeting telemetry on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_drawcalls", "0", FCVAR_NONE, "Display render draw calls and vertex count on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_memory", "0", FCVAR_NONE, "Display process RAM working set on HUD", false, 0.0f, false, 0.0f);
        registerConVar("log_all", "0", FCVAR_NONE, "Toggle all developer telemetry overlays on HUD", false, 0.0f, false, 0.0f);
        registerConVar("wrd_map", "facility_alpha.labmap", FCVAR_NONE, "Current sector map path", false, 0.0f, false, 0.0f);
        registerConVar("wrd_restart", "1", FCVAR_NONE, "Restart current world map", false, 0.0f, false, 0.0f);
        registerConVar("wrd_skybox", "#0d121a", FCVAR_NONE, "Active skybox name or hex color", false, 0.0f, false, 0.0f);
        registerConVar("wrd_model", "assets/models/guard.glb", FCVAR_CHEAT, "Spawn 3D GLB model in front of player", false, 0.0f, false, 0.0f);
        registerConVar("wrd_timescale", "1.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Physics and world simulation timescale", true, 0.05f, true, 10.0f);
        registerConVar("wrd_wireframe", "0", FCVAR_CHEAT, "Render world solid geometry as wireframe", false, 0.0f, false, 0.0f);
        registerConVar("wrd_fog", "0", FCVAR_NONE, "Enable atmospheric distance fog", false, 0.0f, false, 0.0f);
        registerConVar("wrd_fog_density", "0.02", FCVAR_NONE, "Atmospheric fog density coefficient", true, 0.0f, true, 1.0f);
        registerConVar("wrd_light_ambient", "0.2 0.2 0.25", FCVAR_NONE, "World ambient light color RGB", false, 0.0f, false, 0.0f);
        registerConVar("wrd_postprocess", "1", FCVAR_ARCHIVE, "Enable HDR tonemapping, vignette and color grading", false, 0.0f, false, 0.0f);
        registerConVar("wrd_gravity", "-12.0", FCVAR_CHEAT | FCVAR_REPLICATED, "World downward gravity acceleration (m/s^2)", false, 0.0f, false, 0.0f);
        registerConVar("cam_fov", "75.0", FCVAR_ARCHIVE, "Camera horizontal field of view in degrees", true, 30.0f, true, 140.0f);
        registerConVar("cam_viewmodel_fov", "54.0", FCVAR_ARCHIVE, "First-person viewmodel field of view in degrees", true, 30.0f, true, 100.0f);
        registerConVar("cam_viewmodel_offset", "0 0 0", FCVAR_ARCHIVE, "Viewmodel 3D position offset (X Y Z)", false, 0.0f, false, 0.0f);
        registerConVar("cam_thirdperson", "0", FCVAR_CHEAT, "Toggle third-person chase camera", false, 0.0f, false, 0.0f);
        registerConVar("cam_bob", "1", FCVAR_ARCHIVE, "Enable view bobbing while walking", false, 0.0f, false, 0.0f);
        registerConVar("cam_shake", "1.0", FCVAR_CHEAT, "Trigger camera explosion trauma shake", false, 0.0f, false, 0.0f);
        registerConVar("cam_freecam", "0", FCVAR_CHEAT, "Toggle spectator free-flight camera", false, 0.0f, false, 0.0f);
        registerConVar("r_bloom", "1", FCVAR_ARCHIVE, "Enable Half-Res dual-pass HDR bloom effect", false, 0.0f, false, 0.0f);
        registerConVar("r_fxaa", "1", FCVAR_ARCHIVE, "Enable Fast Approximate Anti-Aliasing (FXAA)", false, 0.0f, false, 0.0f);
        registerConVar("r_vsync", "1", FCVAR_ARCHIVE, "Enable vertical sync buffer swapping", false, 0.0f, false, 0.0f);
        registerConVar("r_shadows", "1", FCVAR_ARCHIVE, "Enable projective dynamic shadows", false, 0.0f, false, 0.0f);
        registerConVar("r_shadow_res", "1024", FCVAR_ARCHIVE, "Shadow map depth texture resolution", true, 256.0f, true, 4096.0f);
        registerConVar("r_lightmaps", "1", FCVAR_ARCHIVE, "Render baked radiosity lightmap atlas textures", false, 0.0f, false, 0.0f);
        registerConVar("r_normalmaps", "1", FCVAR_ARCHIVE, "Enable normal map bump lighting", false, 0.0f, false, 0.0f);
        registerConVar("r_cubemaps", "1", FCVAR_ARCHIVE, "Enable specular environment cubemap reflections", false, 0.0f, false, 0.0f);
        registerConVar("r_postprocess", "1", FCVAR_ARCHIVE, "Enable ACES tonemapping post-processing pipeline", false, 0.0f, false, 0.0f);
        registerConVar("r_reload_shaders", "1", FCVAR_CHEAT, "Hot-reload all runtime GLSL shaders", false, 0.0f, false, 0.0f);
        registerConVar("phys_gravity", "12.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Physics gravity magnitude (m/s^2)", true, 0.0f, true, 100.0f);
        registerConVar("phys_friction", "5.5", FCVAR_CHEAT | FCVAR_REPLICATED, "Ground friction coefficient", true, 0.0f, true, 20.0f);
        registerConVar("phys_air_accel", "12.0", FCVAR_ARCHIVE | FCVAR_REPLICATED, "Air strafe acceleration factor", true, 1.0f, true, 100.0f);
        registerConVar("phys_bhop_window", "0.08", FCVAR_ARCHIVE, "Landing frame buffer window for bhop (seconds)", true, 0.01f, true, 0.25f);
        registerConVar("phys_max_bhop_speed", "11.5", FCVAR_ARCHIVE | FCVAR_REPLICATED, "Maximum horizontal speed ceiling (m/s)", true, 5.0f, true, 50.0f);
        registerConVar("phys_step_height", "0.5", FCVAR_CHEAT | FCVAR_REPLICATED, "Maximum step/stair climbing height (meters)", true, 0.1f, true, 1.0f);
        registerConVar("phys_ragdoll_force", "1.0", FCVAR_ARCHIVE, "Ragdoll impact impulse force multiplier", true, 0.0f, true, 10.0f);
        registerConVar("phys_debug", "0", FCVAR_CHEAT, "Draw physics collision hulls and AABB wireframes", false, 0.0f, false, 0.0f);
        registerConVar("ai_disable", "0", FCVAR_CHEAT, "Globally disable all AI bot updates and decision logic", false, 0.0f, false, 0.0f);
        registerConVar("ai_freeze", "0", FCVAR_CHEAT, "Freeze bot positions and velocities in place", false, 0.0f, false, 0.0f);
        registerConVar("ai_sight", "0", FCVAR_CHEAT, "Draw bot field-of-view sight lines and raycasts", false, 0.0f, false, 0.0f);
        registerConVar("ai_paths", "0", FCVAR_CHEAT, "Draw bot navigation waypoints and patrol corridors", false, 0.0f, false, 0.0f);
        registerConVar("ai_aggro", "0", FCVAR_CHEAT, "Force all bots into permanent aggressive combat state", false, 0.0f, false, 0.0f);
        registerConVar("ai_ignore_player", "0", FCVAR_CHEAT, "Bots will never target or shoot human player", false, 0.0f, false, 0.0f);
        registerConVar("ai_spawn", "guard", FCVAR_CHEAT, "Spawn an AI bot of specified archetype (guard/worker/scientist)", false, 0.0f, false, 0.0f);
        registerConVar("snd_volume", "1.0", FCVAR_ARCHIVE, "Master audio output volume", true, 0.0f, true, 1.0f);
        registerConVar("snd_music", "0.7", FCVAR_ARCHIVE, "Music audio volume", true, 0.0f, true, 1.0f);
        registerConVar("snd_sfx", "1.0", FCVAR_ARCHIVE, "Sound effects volume", true, 0.0f, true, 1.0f);
        registerConVar("snd_voice", "1.0", FCVAR_ARCHIVE, "Voice chat volume", true, 0.0f, true, 1.0f);
        registerConVar("snd_play", "assets/audio/step_concrete1.wav", FCVAR_NONE, "Play a sound file directly", false, 0.0f, false, 0.0f);
        registerConVar("snd_stop_all", "1", FCVAR_NONE, "Stop all playing audio channels", false, 0.0f, false, 0.0f);
        registerConVar("snd_lipsync_test", "assets/audio/test_speech.wav", FCVAR_CHEAT, "Run audio viseme facial lip-sync test", false, 0.0f, false, 0.0f);
        registerConVar("snd_restart", "1", FCVAR_NONE, "Restart OpenAL / Core audio driver", false, 0.0f, false, 0.0f);
        registerConVar("ui_hide", "0", FCVAR_NONE, "Hide complete HUD interface", false, 0.0f, false, 0.0f);
        registerConVar("ui_crosshair", "1", FCVAR_ARCHIVE, "Show HUD reticle crosshair", false, 0.0f, false, 0.0f);
        registerConVar("ui_crosshair_color", "#00ff00", FCVAR_ARCHIVE, "HUD crosshair hex color", false, 0.0f, false, 0.0f);
        registerConVar("ui_scale", "1.0", FCVAR_ARCHIVE, "HUD user interface scale factor", true, 0.5f, true, 2.5f);
        registerConVar("ui_menu", "main", FCVAR_NONE, "Open UI screen by identifier", false, 0.0f, false, 0.0f);
        registerConVar("ui_reload", "1", FCVAR_CHEAT, "Reload HUD and VGUI2 themes and layouts", false, 0.0f, false, 0.0f);
        registerConVar("dcl_clear", "1", FCVAR_NONE, "Remove all active decals from map", false, 0.0f, false, 0.0f);
        registerConVar("dcl_limit", "256", FCVAR_ARCHIVE, "Maximum simultaneous decals in world", true, 16.0f, true, 2048.0f);
        registerConVar("dcl_spawn", "bullet_concrete", FCVAR_CHEAT, "Spawn decal at crosshair target", false, 0.0f, false, 0.0f);
        registerConVar("dcl_distance", "50.0", FCVAR_ARCHIVE, "Maximum decal visibility distance (meters)", true, 5.0f, true, 200.0f);
        registerConVar("mp_a_bots", "2", FCVAR_NONE, "Count of bots assigned to Team Alpha (Blue)", false, 0.0f, false, 0.0f);
        registerConVar("mp_b_bots", "2", FCVAR_NONE, "Count of bots assigned to Team Beta (Red)", false, 0.0f, false, 0.0f);
        registerConVar("mp_bot_kick_all", "1", FCVAR_NONE, "Kick all bots from match", false, 0.0f, false, 0.0f);
        registerConVar("mp_bot_freeze", "0", FCVAR_CHEAT, "Freeze all match bots", false, 0.0f, false, 0.0f);
        registerConVar("mp_restart", "1", FCVAR_SERVER_CAN_EXECUTE, "Restart match round", false, 0.0f, false, 0.0f);
        registerConVar("mp_friendly_fire", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Allow teammate friendly fire damage", false, 0.0f, false, 0.0f);
        registerConVar("net_status", "1", FCVAR_NONE, "Print multiplayer client-server network stats", false, 0.0f, false, 0.0f);
        registerConVar("net_fake_lag", "0", FCVAR_CHEAT, "Simulate artificial network latency in milliseconds", true, 0.0f, true, 1000.0f);
        registerConVar("net_fake_loss", "0.0", FCVAR_CHEAT, "Simulate artificial packet drop percentage", true, 0.0f, true, 100.0f);
        registerConVar("net_connect", "127.0.0.1:27015", FCVAR_NONE, "Initiate UDP connection to server address", false, 0.0f, false, 0.0f);
        registerConVar("net_disconnect", "1", FCVAR_NONE, "Disconnect current UDP network session", false, 0.0f, false, 0.0f);
        registerConVar("sys_copylog", "1", FCVAR_NONE, "Copy console text to clipboard", false, 0.0f, false, 0.0f);
        registerConVar("sys_clear", "1", FCVAR_NONE, "Clear developer console buffer", false, 0.0f, false, 0.0f);
        registerConVar("sys_help", "", FCVAR_NONE, "Search developer console commands", false, 0.0f, false, 0.0f);
        registerConVar("sys_bind", "", FCVAR_ARCHIVE, "Bind key to command string", false, 0.0f, false, 0.0f);
        registerConVar("sys_quit", "1", FCVAR_NONE, "Close engine and return to desktop", false, 0.0f, false, 0.0f);
        registerConVar("save_async", "1", FCVAR_ARCHIVE, "Enable non-blocking asynchronous map saving", false, 0.0f, false, 0.0f);
        registerConVar("save_asyncdelay", "0", FCVAR_NONE, "Simulated delay for asynchronous saves", false, 0.0f, false, 0.0f);
        registerConVar("save_console", "0", FCVAR_NONE, "Echo save progress to developer console", false, 0.0f, false, 0.0f);
        registerConVar("save_disable", "0", FCVAR_CHEAT, "Disable saving game states", false, 0.0f, false, 0.0f);
        registerConVar("save_history_count", "1", FCVAR_ARCHIVE, "Count of autosave rollbacks kept", false, 0.0f, false, 0.0f);
        registerConVar("save_huddelayframes", "1", FCVAR_NONE, "HUD frames delay for save notifications", false, 0.0f, false, 0.0f);
        registerConVar("save_in_memory", "0", FCVAR_NONE, "Keep save files in RAM buffer only", false, 0.0f, false, 0.0f);
        registerConVar("save_noxsave", "0", FCVAR_NONE, "Disable extended XML save metadata", false, 0.0f, false, 0.0f);
        registerConVar("fps_max", "300", FCVAR_ARCHIVE, "Frame rate limiter (0 = unlimited)", true, 0.0f, true, 1000.0f);
        registerConVar("fps_max_menu", "120", FCVAR_ARCHIVE, "Frame rate limiter while in main menu", true, 30.0f, true, 300.0f);
        registerConVar("sensitivity", "3.0", FCVAR_ARCHIVE, "Mouse look sensitivity", true, 0.1f, true, 30.0f);
        registerConVar("m_rawinput", "1", FCVAR_ARCHIVE, "Use raw mouse input from OS", false, 0.0f, false, 0.0f);
        registerConVar("m_pitch", "0.022", FCVAR_ARCHIVE, "Mouse pitch sensitivity factor", false, 0.0f, false, 0.0f);
        registerConVar("m_yaw", "0.022", FCVAR_ARCHIVE, "Mouse yaw sensitivity factor", false, 0.0f, false, 0.0f);
        registerConVar("m_customaccel", "0", FCVAR_ARCHIVE, "Custom mouse acceleration mode", false, 0.0f, false, 0.0f);
        registerConVar("zoom_sensitivity_ratio", "1.0", FCVAR_ARCHIVE, "Mouse sensitivity multiplier when scoped", true, 0.1f, true, 5.0f);
        registerConVar("con_enable", "1", FCVAR_ARCHIVE, "Enable developer console [~] key", false, 0.0f, false, 0.0f);
        registerConVar("con_filter_enable", "0", FCVAR_NONE, "Filter console spew by substring", false, 0.0f, false, 0.0f);
        registerConVar("con_filter_text", "", FCVAR_NONE, "Substring filter for console messages", false, 0.0f, false, 0.0f);
        registerConVar("con_filter_text_out", "", FCVAR_NONE, "Substring exclusion filter for console messages", false, 0.0f, false, 0.0f);
        registerConVar("con_logfile", "console.log", FCVAR_ARCHIVE, "File path for mirrored console logging", false, 0.0f, false, 0.0f);
        registerConVar("developer", "0", FCVAR_NONE, "Developer output verbosity (0=off, 1=on, 2=verbose)", true, 0.0f, true, 2.0f);
        registerConVar("rate", "128000", FCVAR_USERINFO | FCVAR_ARCHIVE, "Max bytes/sec client can receive from host", true, 10000.0f, true, 1000000.0f);
        registerConVar("cl_cmdrate", "64", FCVAR_USERINFO | FCVAR_ARCHIVE, "Client to server UserCmd send rate per second", true, 10.0f, true, 128.0f);
        registerConVar("cl_updaterate", "64", FCVAR_USERINFO | FCVAR_ARCHIVE, "Requested snapshot updates per second", true, 10.0f, true, 128.0f);
        registerConVar("cl_interp", "0.015625", FCVAR_USERINFO | FCVAR_ARCHIVE, "Interpolation delay in seconds", true, 0.0f, true, 0.1f);
        registerConVar("cl_interp_ratio", "1.0", FCVAR_USERINFO | FCVAR_ARCHIVE, "Client interpolation ratio multiplier", true, 1.0f, true, 5.0f);
        registerConVar("cl_predict", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "Perform client-side movement prediction", false, 0.0f, false, 0.0f);
        registerConVar("cl_predictweapons", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "Perform client-side weapon prediction", false, 0.0f, false, 0.0f);
        registerConVar("cl_lagcompensation", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "Perform server-side lag compensation", false, 0.0f, false, 0.0f);
        registerConVar("net_graph", "0", FCVAR_ARCHIVE, "Display network graph and telemetry (0=off, 1=fps/ping/var, 2=choke/loss, 3=rates)", true, 0.0f, true, 4.0f);
        registerConVar("net_graphpos", "1", FCVAR_ARCHIVE, "Net graph screen alignment (1=right, 2=center, 3=left)", false, 0.0f, false, 0.0f);
        registerConVar("net_graphsolid", "1", FCVAR_ARCHIVE, "Draw net graph with solid background", false, 0.0f, false, 0.0f);
        registerConVar("net_graphheight", "64", FCVAR_ARCHIVE, "Net graph vertical height in pixels", false, 0.0f, false, 0.0f);
        registerConVar("sv_airaccelerate", "10", FCVAR_NOTIFY | FCVAR_REPLICATED, "Air acceleration rate", false, 0.0f, false, 0.0f);
        registerConVar("sv_accelerate", "5.5", FCVAR_NOTIFY | FCVAR_REPLICATED, "Ground acceleration rate", false, 0.0f, false, 0.0f);
        registerConVar("sv_maxspeed", "320", FCVAR_NOTIFY | FCVAR_REPLICATED, "Maximum player velocity speed cap", false, 0.0f, false, 0.0f);
        registerConVar("sv_stopspeed", "75", FCVAR_NOTIFY | FCVAR_REPLICATED, "Minimum stopping speed threshold", false, 0.0f, false, 0.0f);
        registerConVar("sv_maxvelocity", "3500", FCVAR_REPLICATED, "Maximum velocity any entity can travel", false, 0.0f, false, 0.0f);
        registerConVar("sv_bounce", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Bounce multiplier for entity collisions", false, 0.0f, false, 0.0f);
        registerConVar("sv_rollspeed", "200", FCVAR_REPLICATED, "Max camera view roll speed", false, 0.0f, false, 0.0f);
        registerConVar("sv_rollangle", "0", FCVAR_REPLICATED, "Max camera view roll angle on strafe", false, 0.0f, false, 0.0f);
        registerConVar("sv_wateraccelerate", "10", FCVAR_REPLICATED, "Water acceleration rate", false, 0.0f, false, 0.0f);
        registerConVar("sv_waterfriction", "1", FCVAR_REPLICATED, "Water friction rate", false, 0.0f, false, 0.0f);
        registerConVar("sv_alltalk", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Players can hear all voice chat across teams", false, 0.0f, false, 0.0f);
        registerConVar("sv_voiceenable", "1", FCVAR_ARCHIVE | FCVAR_NOTIFY, "Enable in-game voice chat system", false, 0.0f, false, 0.0f);
        registerConVar("sv_maxrate", "0", FCVAR_REPLICATED, "Max bandwidth rate allowed on server (0=unlimited)", false, 0.0f, false, 0.0f);
        registerConVar("sv_minrate", "16000", FCVAR_REPLICATED, "Min bandwidth rate allowed on server", false, 0.0f, false, 0.0f);
        registerConVar("sv_maxupdaterate", "64", FCVAR_REPLICATED, "Max update rate server allows", false, 0.0f, false, 0.0f);
        registerConVar("sv_minupdaterate", "20", FCVAR_REPLICATED, "Min update rate server allows", false, 0.0f, false, 0.0f);
        registerConVar("sv_maxcmdrate", "64", FCVAR_REPLICATED, "Max usercmd rate server allows", false, 0.0f, false, 0.0f);
        registerConVar("sv_mincmdrate", "20", FCVAR_REPLICATED, "Min usercmd rate server allows", false, 0.0f, false, 0.0f);
        registerConVar("sv_client_cmdrate_difference", "20", FCVAR_REPLICATED, "Permitted cmdrate difference", false, 0.0f, false, 0.0f);
        registerConVar("sv_client_predict", "1", FCVAR_REPLICATED, "Require client prediction", false, 0.0f, false, 0.0f);
        registerConVar("sv_client_interpolate", "1", FCVAR_REPLICATED, "Require client interpolation", false, 0.0f, false, 0.0f);
        registerConVar("sv_client_interp_ratio", "1", FCVAR_REPLICATED, "Enforce client interp ratio", false, 0.0f, false, 0.0f);
        registerConVar("sv_client_min_interp_ratio", "1", FCVAR_REPLICATED, "Minimum client interp ratio", false, 0.0f, false, 0.0f);
        registerConVar("sv_client_max_interp_ratio", "2", FCVAR_REPLICATED, "Maximum client interp ratio", false, 0.0f, false, 0.0f);
        registerConVar("sv_unlag", "1", FCVAR_REPLICATED, "Enable server lag compensation", false, 0.0f, false, 0.0f);
        registerConVar("sv_maxunlag", "1.0", FCVAR_REPLICATED, "Maximum lag compensation time window", false, 0.0f, false, 0.0f);
        registerConVar("sv_timeout", "30", FCVAR_NONE, "Seconds without packet before client is dropped", false, 0.0f, false, 0.0f);
        registerConVar("sv_specspeed", "3.0", FCVAR_ARCHIVE, "Spectator camera movement speed", false, 0.0f, false, 0.0f);
        registerConVar("sv_specaccelerate", "5.0", FCVAR_ARCHIVE, "Spectator camera acceleration", false, 0.0f, false, 0.0f);
        registerConVar("sv_specnoclip", "1", FCVAR_ARCHIVE, "Spectators can clip through world walls", false, 0.0f, false, 0.0f);
        registerConVar("sv_pure", "0", FCVAR_REPLICATED, "Pure server file validation (0=off, 1=rules, 2=strict)", false, 0.0f, false, 0.0f);
        registerConVar("sv_pure_kick_clients", "1", FCVAR_REPLICATED, "Kick clients with modified game assets", false, 0.0f, false, 0.0f);
        registerConVar("sv_pure_trace", "0", FCVAR_NONE, "Print asset validation trace", false, 0.0f, false, 0.0f);
        registerConVar("sv_password", "", FCVAR_PROTECTED, "Server join password", false, 0.0f, false, 0.0f);
        registerConVar("sv_visiblemaxplayers", "16", FCVAR_NONE, "Visible maximum player slots", false, 0.0f, false, 0.0f);
        registerConVar("sv_region", "255", FCVAR_NONE, "Server geographic region code", false, 0.0f, false, 0.0f);
        registerConVar("sv_pausable", "0", FCVAR_NOTIFY, "Server pause permitted by clients", false, 0.0f, false, 0.0f);
        registerConVar("sv_allowupload", "1", FCVAR_NONE, "Allow clients to upload custom sprays and assets", false, 0.0f, false, 0.0f);
        registerConVar("sv_allowdownload", "1", FCVAR_NONE, "Allow clients to download custom maps and models", false, 0.0f, false, 0.0f);
        registerConVar("sv_consistency", "1", FCVAR_REPLICATED, "Check critical game files consistency", false, 0.0f, false, 0.0f);
        registerConVar("sv_contact", "", FCVAR_NOTIFY, "Server administrative contact email/URL", false, 0.0f, false, 0.0f);
        registerConVar("sv_downloadurl", "", FCVAR_REPLICATED, "FastDL HTTP asset download base URL", false, 0.0f, false, 0.0f);
        registerConVar("sv_footsteps", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Play audio footstep sounds on surfaces", false, 0.0f, false, 0.0f);
        registerConVar("sv_forcepreload", "1", FCVAR_ARCHIVE, "Force precaching all map resources at level load", false, 0.0f, false, 0.0f);
        registerConVar("sv_log_onefile", "0", FCVAR_ARCHIVE, "Log all multiplayer matches into single file", false, 0.0f, false, 0.0f);
        registerConVar("sv_logbans", "1", FCVAR_ARCHIVE, "Log player bans to logfile", false, 0.0f, false, 0.0f);
        registerConVar("sv_logecho", "1", FCVAR_ARCHIVE, "Echo server log events to console", false, 0.0f, false, 0.0f);
        registerConVar("sv_logfile", "1", FCVAR_ARCHIVE, "Enable logging server matches to disk", false, 0.0f, false, 0.0f);
        registerConVar("sv_logflush", "0", FCVAR_ARCHIVE, "Flush log output buffer to disk immediately", false, 0.0f, false, 0.0f);
        registerConVar("sv_logsdir", "logs", FCVAR_ARCHIVE, "Directory for server logs", false, 0.0f, false, 0.0f);
        registerConVar("sv_noclipaccelerate", "5", FCVAR_ARCHIVE | FCVAR_CHEAT, "Noclip camera acceleration", false, 0.0f, false, 0.0f);
        registerConVar("sv_noclipspeed", "5", FCVAR_ARCHIVE | FCVAR_CHEAT, "Noclip camera flight speed", false, 0.0f, false, 0.0f);
        registerConVar("sv_pushaway_force", "30000", FCVAR_REPLICATED, "Physics pushaway force on players", false, 0.0f, false, 0.0f);
        registerConVar("sv_pushaway_max_force", "1000", FCVAR_REPLICATED, "Maximum pushaway impulse", false, 0.0f, false, 0.0f);
        registerConVar("sv_pushaway_min_player_speed", "75", FCVAR_REPLICATED, "Minimum speed required to push entities", false, 0.0f, false, 0.0f);
        registerConVar("sv_showenvinfo", "0", FCVAR_CHEAT, "Display map environment entity parameters", false, 0.0f, false, 0.0f);
        registerConVar("sv_showhitboxes", "0", FCVAR_CHEAT, "Display skeletal player and bot hitboxes", false, 0.0f, false, 0.0f);
        registerConVar("sv_showimpacts", "0", FCVAR_CHEAT, "Display bullet client/server impact points", false, 0.0f, false, 0.0f);
        registerConVar("sv_showlagcompensation", "0", FCVAR_CHEAT, "Display lag compensation hitbox rewind", false, 0.0f, false, 0.0f);
        registerConVar("sv_stats", "1", FCVAR_NONE, "Collect CPU and network server telemetry", false, 0.0f, false, 0.0f);
        registerConVar("sv_turbophysics", "0", FCVAR_REPLICATED, "Fast simplified physics simulation", false, 0.0f, false, 0.0f);
        registerConVar("sv_unlag_debug", "0", FCVAR_CHEAT, "Print debug traces for lag compensation", false, 0.0f, false, 0.0f);
        registerConVar("sv_unlag_fixstuck", "1", FCVAR_REPLICATED, "Automatically unstuck player positions", false, 0.0f, false, 0.0f);
        registerConVar("sv_voicecodec", "vaudio_celt", FCVAR_NONE, "Audio compression codec for voice chat", false, 0.0f, false, 0.0f);
        registerConVar("sv_vote_failure_timer", "300", FCVAR_NONE, "Vote call retry lockout seconds", false, 0.0f, false, 0.0f);
        registerConVar("sv_vote_quorum_ratio", "0.6", FCVAR_NONE, "Fraction of players needed for vote pass", false, 0.0f, false, 0.0f);
        registerConVar("cl_forwardspeed", "320", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Forward movement speed scale", false, 0.0f, false, 0.0f);
        registerConVar("cl_backspeed", "320", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Backward movement speed scale", false, 0.0f, false, 0.0f);
        registerConVar("cl_sidespeed", "320", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Strafe movement speed scale", false, 0.0f, false, 0.0f);
        registerConVar("cl_upspeed", "320", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Vertical flight movement speed scale", false, 0.0f, false, 0.0f);
        registerConVar("cl_yawspeed", "210", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Keyboard yaw turning speed", false, 0.0f, false, 0.0f);
        registerConVar("cl_pitchspeed", "225", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Keyboard pitch turning speed", false, 0.0f, false, 0.0f);
        registerConVar("cl_showfps", "0", FCVAR_CLIENTDLL, "Display real-time FPS counter on HUD (1=fps, 2=smooth)", false, 0.0f, false, 0.0f);
        registerConVar("cl_showpos", "0", FCVAR_CLIENTDLL, "Display position coordinates, angle and speed on HUD", false, 0.0f, false, 0.0f);
        registerConVar("cl_showang", "0", FCVAR_CLIENTDLL, "Display view angle pitch and yaw on HUD", false, 0.0f, false, 0.0f);
        registerConVar("cl_showspeed", "0", FCVAR_CLIENTDLL, "Display current velocity speed on HUD", false, 0.0f, false, 0.0f);
        registerConVar("cl_showbattery", "0", FCVAR_CLIENTDLL, "Display device battery status if mobile", false, 0.0f, false, 0.0f);
        registerConVar("cl_mousespeed", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "OS mouse acceleration multiplier", false, 0.0f, false, 0.0f);
        registerConVar("cl_righthand", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Render viewmodel in right hand (1) or left hand (0)", false, 0.0f, false, 0.0f);
        registerConVar("cl_bob", "0.002", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Viewmodel bob factor", false, 0.0f, false, 0.0f);
        registerConVar("cl_bobup", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Viewmodel bob upward cycle fraction", false, 0.0f, false, 0.0f);
        registerConVar("cl_bobcycle", "0.8", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Viewmodel bob stride frequency", false, 0.0f, false, 0.0f);
        registerConVar("cl_viewbob", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable camera walk bobbing", false, 0.0f, false, 0.0f);
        registerConVar("cl_autowepswitch", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Automatically equip weapons on pickup", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Display weapon aim crosshair", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshaircolor", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crosshair color preset (0=green, 1=red, 2=blue, 3=yellow, 4=cyan)", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshairsize", "5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crosshair reticle tick length", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshairthickness", "1.0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crosshair reticle tick thickness", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshairdot", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Draw center dot in crosshair", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshairalpha", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crosshair opacity alpha (0-255)", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshairgap", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Gap between crosshair ticks", false, 0.0f, false, 0.0f);
        registerConVar("cl_crosshairscale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Legacy crosshair scale factor", false, 0.0f, false, 0.0f);
        registerConVar("cl_dynamiccrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crosshair expands when moving and firing", false, 0.0f, false, 0.0f);
        registerConVar("cl_drawhud", "1", FCVAR_CLIENTDLL | FCVAR_CHEAT, "Draw heads-up display HUD elements", false, 0.0f, false, 0.0f);
        registerConVar("cl_ejectbrass", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Eject weapon cartridge shells on gunshot", false, 0.0f, false, 0.0f);
        registerConVar("cl_fastdetailsprites", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Fast rendering of ground detail grass sprites", false, 0.0f, false, 0.0f);
        registerConVar("cl_hudhint_sound", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Play audio chime on tutorial hint", false, 0.0f, false, 0.0f);
        registerConVar("cl_minmodels", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Force single player model for all enemies", false, 0.0f, false, 0.0f);
        registerConVar("cl_mouselook", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Look around with mouse", false, 0.0f, false, 0.0f);
        registerConVar("cl_observercrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Draw crosshair in spectator mode", false, 0.0f, false, 0.0f);
        registerConVar("cl_phys_props_enable", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Render client-side debris physics props", false, 0.0f, false, 0.0f);
        registerConVar("cl_phys_props_max", "128", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Max count of client-side physics props", false, 0.0f, false, 0.0f);
        registerConVar("cl_playerspraydisable", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Disable viewing player spray logos", false, 0.0f, false, 0.0f);
        registerConVar("cl_radartype", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Radar display style (0=transparent, 1=solid)", false, 0.0f, false, 0.0f);
        registerConVar("cl_ragdoll_collide", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Allow ragdolls to collide with each other", false, 0.0f, false, 0.0f);
        registerConVar("cl_ragdoll_fade_time", "15", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Seconds before eliminated ragdolls fade out", false, 0.0f, false, 0.0f);
        registerConVar("cl_ragdoll_physics_enable", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Simulate skeletal ragdoll physics", false, 0.0f, false, 0.0f);
        registerConVar("cl_scalecrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Scale crosshair dynamically with resolution", false, 0.0f, false, 0.0f);
        registerConVar("cl_shadows", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Render character entity shadows", false, 0.0f, false, 0.0f);
        registerConVar("cl_threaded_bone_setup", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Multi-threaded skeletal bone transform blending", false, 0.0f, false, 0.0f);
        registerConVar("cl_smooth", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Smooth player prediction error corrections", false, 0.0f, false, 0.0f);
        registerConVar("cl_smoothtime", "0.1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Time in seconds to smooth prediction errors", false, 0.0f, false, 0.0f);
        registerConVar("cl_resend", "6", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Seconds before re-sending connection request", false, 0.0f, false, 0.0f);
        registerConVar("cl_timeout", "30", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Seconds without packet before disconnecting", false, 0.0f, false, 0.0f);
        registerConVar("cl_downloadfilter", "all", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Filter server asset downloads (all/nosounds/none)", false, 0.0f, false, 0.0f);
        registerConVar("cl_allowdownload", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Allow downloading custom maps and assets", false, 0.0f, false, 0.0f);
        registerConVar("cl_allowupload", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Allow uploading player spray logo to server", false, 0.0f, false, 0.0f);
        registerConVar("cl_forcepreload", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Preload level resources into memory at start", false, 0.0f, false, 0.0f);
        registerConVar("cl_packetdump", "0", FCVAR_CLIENTDLL | FCVAR_CHEAT, "Dump incoming UDP network packet hex values", false, 0.0f, false, 0.0f);
        registerConVar("cl_autohelp", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Display context help on first encounter", false, 0.0f, false, 0.0f);
        registerConVar("cl_clearhinthistory", "0", FCVAR_CLIENTDLL, "Clear recorded user hint triggers", false, 0.0f, false, 0.0f);
        registerConVar("cl_debugrumble", "0", FCVAR_CLIENTDLL | FCVAR_CHEAT, "Print controller haptic vibration telemetry", false, 0.0f, false, 0.0f);
        registerConVar("cl_detaildist", "1200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Distance at which foliage detail props render", false, 0.0f, false, 0.0f);
        registerConVar("cl_detailfade", "400", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Distance span over which detail props fade out", false, 0.0f, false, 0.0f);
        registerConVar("r_drawentities", "1", FCVAR_CHEAT, "Render all entity models in world", false, 0.0f, false, 0.0f);
        registerConVar("r_drawothermodels", "1", FCVAR_CHEAT, "Render other player/bot models (1=normal, 2=wireframe)", false, 0.0f, false, 0.0f);
        registerConVar("r_drawstaticprops", "1", FCVAR_CHEAT, "Render static world prop meshes", false, 0.0f, false, 0.0f);
        registerConVar("r_drawbrushmodels", "1", FCVAR_CHEAT, "Render brush geometry models", false, 0.0f, false, 0.0f);
        registerConVar("r_drawworld", "1", FCVAR_CHEAT, "Render world static architecture brushes", false, 0.0f, false, 0.0f);
        registerConVar("r_drawskybox", "1", FCVAR_CHEAT, "Render world skybox backdrop", false, 0.0f, false, 0.0f);
        registerConVar("r_drawparticles", "1", FCVAR_CHEAT, "Render particle systems and smoke emitters", false, 0.0f, false, 0.0f);
        registerConVar("r_drawviewmodel", "1", FCVAR_CHEAT, "Render first-person weapon viewmodel", false, 0.0f, false, 0.0f);
        registerConVar("r_drawclipbrushes", "0", FCVAR_CHEAT, "Render player collision clip brushes (1=player, 2=full)", false, 0.0f, false, 0.0f);
        registerConVar("r_drawlights", "0", FCVAR_CHEAT, "Render light source billboard debug icons", false, 0.0f, false, 0.0f);
        registerConVar("r_drawropes", "1", FCVAR_ARCHIVE, "Render catenary physics cables and ropes", false, 0.0f, false, 0.0f);
        registerConVar("r_drawtranslucentrenderables", "1", FCVAR_ARCHIVE, "Render transparent glass and water planes", false, 0.0f, false, 0.0f);
        registerConVar("r_drawdetailprops", "1", FCVAR_ARCHIVE, "Render detail grass and rock clutter", false, 0.0f, false, 0.0f);
        registerConVar("r_drawdecals", "1", FCVAR_ARCHIVE, "Render bullet impact and blood decals", false, 0.0f, false, 0.0f);
        registerConVar("r_drawwater", "1", FCVAR_ARCHIVE, "Render refractive and reflective water surfaces", false, 0.0f, false, 0.0f);
        registerConVar("r_drawpixelvisibility", "0", FCVAR_CHEAT, "Draw occlusion query lens flare visibility boxes", false, 0.0f, false, 0.0f);
        registerConVar("r_wireframe", "0", FCVAR_CHEAT, "Render world in wireframe polygon mesh", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowmaxrendered", "32", FCVAR_ARCHIVE, "Max count of simultaneous dynamic shadows", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowrendertotexture", "1", FCVAR_ARCHIVE, "Render high-quality depth map shadows", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightdepthres", "1024", FCVAR_ARCHIVE, "Flashlight shadow depth texture map resolution", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightmodels", "1", FCVAR_ARCHIVE, "Flashlight illuminates character models", false, 0.0f, false, 0.0f);
        registerConVar("r_bloomtint", "1.0 1.0 1.0", FCVAR_ARCHIVE, "HDR bloom RGB color tint factor", false, 0.0f, false, 0.0f);
        registerConVar("r_bloomscale", "0.8", FCVAR_ARCHIVE, "HDR bloom intensity scale", false, 0.0f, false, 0.0f);
        registerConVar("r_3dsky", "1", FCVAR_ARCHIVE, "Render miniature 3D skybox layer", false, 0.0f, false, 0.0f);
        registerConVar("r_skybox", "1", FCVAR_ARCHIVE, "Enable skybox background rendering", false, 0.0f, false, 0.0f);
        registerConVar("r_fog", "0", FCVAR_ARCHIVE, "Enable atmospheric distance fog", false, 0.0f, false, 0.0f);
        registerConVar("r_fog_density", "0.02", FCVAR_ARCHIVE, "Exponential fog density factor", false, 0.0f, false, 0.0f);
        registerConVar("r_fog_start", "20.0", FCVAR_ARCHIVE, "Linear fog start distance from camera (meters)", false, 0.0f, false, 0.0f);
        registerConVar("r_fog_end", "120.0", FCVAR_ARCHIVE, "Linear fog end distance from camera (meters)", false, 0.0f, false, 0.0f);
        registerConVar("r_fog_color", "0.15 0.18 0.22", FCVAR_ARCHIVE, "Distance fog RGB color vector", false, 0.0f, false, 0.0f);
        registerConVar("r_lod", "-1", FCVAR_ARCHIVE, "Model level of detail (-1=automatic, 0=high, 2=low)", false, 0.0f, false, 0.0f);
        registerConVar("r_rootlod", "0", FCVAR_ARCHIVE, "Base LOD tier for mesh rendering", false, 0.0f, false, 0.0f);
        registerConVar("r_aspectratio", "0", FCVAR_ARCHIVE, "Override display aspect ratio (0=auto, 1.7778=16:9)", false, 0.0f, false, 0.0f);
        registerConVar("r_renderoverlayfragment", "1", FCVAR_ARCHIVE, "Render projected Hammer overlay fragments", false, 0.0f, false, 0.0f);
        registerConVar("r_spray_lifetime", "2", FCVAR_ARCHIVE, "Rounds before player spray decals are purged", false, 0.0f, false, 0.0f);
        registerConVar("r_decals", "2048", FCVAR_ARCHIVE, "Maximum decals kept active in world", false, 0.0f, false, 0.0f);
        registerConVar("r_maxdecals", "256", FCVAR_ARCHIVE, "Maximum concurrent decals per sector", false, 0.0f, false, 0.0f);
        registerConVar("r_drawflecks", "1", FCVAR_ARCHIVE, "Spawn debris flecks on bullet impact", false, 0.0f, false, 0.0f);
        registerConVar("r_dynamic", "1", FCVAR_ARCHIVE, "Enable dynamic point light calculations", false, 0.0f, false, 0.0f);
        registerConVar("r_maxmodelbounds", "1024", FCVAR_NONE, "Maximum model bounding box dimension", false, 0.0f, false, 0.0f);
        registerConVar("r_eyeglintdensity", "3", FCVAR_ARCHIVE, "Character eye reflection glint density", false, 0.0f, false, 0.0f);
        registerConVar("r_eyemove", "1", FCVAR_ARCHIVE, "Animate character procedural eye saccades", false, 0.0f, false, 0.0f);
        registerConVar("r_eyesize", "0", FCVAR_ARCHIVE, "Scale character eye pupil diameter", false, 0.0f, false, 0.0f);
        registerConVar("r_eyegloss", "1", FCVAR_ARCHIVE, "Render cornea specular eye gloss", false, 0.0f, false, 0.0f);
        registerConVar("r_waterforceexpensive", "1", FCVAR_ARCHIVE, "Force full Fresnel refractive water rendering", false, 0.0f, false, 0.0f);
        registerConVar("r_waterforcereflectentities", "1", FCVAR_ARCHIVE, "Reflect models and dynamic entities in water", false, 0.0f, false, 0.0f);
        registerConVar("r_fastzreject", "1", FCVAR_ARCHIVE, "Enable GPU early depth testing and fast Z culling", false, 0.0f, false, 0.0f);
        registerConVar("r_ambientfactor", "1.0", FCVAR_ARCHIVE, "Ambient lighting scale multiplier", false, 0.0f, false, 0.0f);
        registerConVar("r_ambientfraction", "0.2", FCVAR_ARCHIVE, "Minimum ambient floor lighting", false, 0.0f, false, 0.0f);
        registerConVar("r_ambientlightingonly", "0", FCVAR_CHEAT, "Render only ambient bounce light without direct illumination", false, 0.0f, false, 0.0f);
        registerConVar("r_ambientmin", "0.3", FCVAR_ARCHIVE, "Minimum ambient brightness floor", false, 0.0f, false, 0.0f);
        registerConVar("r_lightaverage", "1", FCVAR_ARCHIVE, "Blend light samples smoothly across entity vertices", false, 0.0f, false, 0.0f);
        registerConVar("r_occlusion", "1", FCVAR_ARCHIVE, "Perform CPU/GPU hardware occlusion culling queries", false, 0.0f, false, 0.0f);
        registerConVar("r_worldlights", "4", FCVAR_ARCHIVE, "Maximum dynamic world lights affecting any single entity", false, 0.0f, false, 0.0f);
        registerConVar("r_PhysPropStaticLighting", "1", FCVAR_ARCHIVE, "Pre-sample static lighting for physics props", false, 0.0f, false, 0.0f);
        registerConVar("r_teeth", "1", FCVAR_ARCHIVE, "Render character teeth inside mouth", false, 0.0f, false, 0.0f);
        registerConVar("r_drawbeams", "1", FCVAR_ARCHIVE, "Render laser and electrical energy beams", false, 0.0f, false, 0.0f);
        registerConVar("r_decal_cables", "1", FCVAR_ARCHIVE, "Allow decals to be placed onto cables and ropes", false, 0.0f, false, 0.0f);
        registerConVar("r_drawdisp", "1", FCVAR_ARCHIVE, "Render terrain displacement surfaces", false, 0.0f, false, 0.0f);
        registerConVar("r_drawflat", "0", FCVAR_CHEAT, "Render all textures as flat unshaded diffuse colors", false, 0.0f, false, 0.0f);
        registerConVar("r_drawportals", "0", FCVAR_CHEAT, "Render area portal occlusion geometry", false, 0.0f, false, 0.0f);
        registerConVar("r_drawrain", "1", FCVAR_ARCHIVE, "Render environmental weather precipitation", false, 0.0f, false, 0.0f);
        registerConVar("r_drawsprites", "1", FCVAR_ARCHIVE, "Render billboard glow and particle sprites", false, 0.0f, false, 0.0f);
        registerConVar("r_drawtranslucentworld", "1", FCVAR_ARCHIVE, "Render translucent brush surfaces", false, 0.0f, false, 0.0f);
        registerConVar("r_frustumcullworld", "1", FCVAR_ARCHIVE, "Frustum cull world brushes outside view", false, 0.0f, false, 0.0f);
        registerConVar("r_lightcache_zbuffercache", "1", FCVAR_ARCHIVE, "Cache lighting sample vectors in Z buffer", false, 0.0f, false, 0.0f);
        registerConVar("r_lightmap", "-1", FCVAR_CHEAT, "Lightmap debug visualizer (-1=normal, 0-N=lightmap page)", false, 0.0f, false, 0.0f);
        registerConVar("r_novis", "0", FCVAR_CHEAT, "Disable Potentially Visible Set (PVS) culling", false, 0.0f, false, 0.0f);
        registerConVar("r_portalopenall", "0", FCVAR_CHEAT, "Force all vis area portals open", false, 0.0f, false, 0.0f);
        registerConVar("r_queued_decals", "1", FCVAR_ARCHIVE, "Offload decal vertex generation to helper thread", false, 0.0f, false, 0.0f);
        registerConVar("r_queued_post_processing", "1", FCVAR_ARCHIVE, "Multi-threaded post-processing dispatch", false, 0.0f, false, 0.0f);
        registerConVar("r_queued_ropes", "1", FCVAR_ARCHIVE, "Multi-threaded rope physics dispatch", false, 0.0f, false, 0.0f);
        registerConVar("r_rainalpha", "0.4", FCVAR_ARCHIVE, "Precipitation raindrop alpha transparency", false, 0.0f, false, 0.0f);
        registerConVar("r_raindir", "0.1 0.0 -1.0", FCVAR_ARCHIVE, "Wind-driven rain trajectory vector", false, 0.0f, false, 0.0f);
        registerConVar("r_rainlength", "0.1", FCVAR_ARCHIVE, "Rain streak length scale", false, 0.0f, false, 0.0f);
        registerConVar("r_rainradius", "1500", FCVAR_ARCHIVE, "Radius around player where rain simulates", false, 0.0f, false, 0.0f);
        registerConVar("r_rainspeed", "600", FCVAR_ARCHIVE, "Precipitation downward terminal speed", false, 0.0f, false, 0.0f);
        registerConVar("r_rainwidth", "0.5", FCVAR_ARCHIVE, "Precipitation droplet width", false, 0.0f, false, 0.0f);
        registerConVar("r_ropetranslucent", "1", FCVAR_ARCHIVE, "Render anti-aliased cable borders", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowcolor", "0 0 0", FCVAR_ARCHIVE, "Dynamic shadow tint color", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowdist", "40", FCVAR_ARCHIVE, "Max distance in meters character shadows render", false, 0.0f, false, 0.0f);
        registerConVar("r_skin", "0", FCVAR_ARCHIVE, "Model skin texture index override", false, 0.0f, false, 0.0f);
        registerConVar("r_staticprop_lod", "-1", FCVAR_ARCHIVE, "Static prop LOD override (-1=auto)", false, 0.0f, false, 0.0f);
        registerConVar("r_threaded_particles", "1", FCVAR_ARCHIVE, "Multi-threaded particle simulation", false, 0.0f, false, 0.0f);
        registerConVar("r_threaded_renderables", "1", FCVAR_ARCHIVE, "Multi-threaded entity list culling", false, 0.0f, false, 0.0f);
        registerConVar("r_updaterefracttexture", "1", FCVAR_ARCHIVE, "Update refractive screen texture copy", false, 0.0f, false, 0.0f);
        registerConVar("r_visualizelighttraces", "0", FCVAR_CHEAT, "Draw raycast paths of light sources", false, 0.0f, false, 0.0f);
        registerConVar("r_worldlightmin", "0.0002", FCVAR_ARCHIVE, "Minimum luminance threshold to discard light", false, 0.0f, false, 0.0f);
        registerConVar("mat_wireframe", "0", FCVAR_CHEAT, "Render materials in polygon wireframe mode", false, 0.0f, false, 0.0f);
        registerConVar("mat_fullbright", "0", FCVAR_CHEAT, "Render world in unlit 100% fullbright mode", false, 0.0f, false, 0.0f);
        registerConVar("mat_picmip", "0", FCVAR_ARCHIVE, "Texture resolution mipmap bias (0=ultra, 1=high, 2=medium)", false, 0.0f, false, 0.0f);
        registerConVar("mat_specular", "1", FCVAR_ARCHIVE, "Enable Blinn-Phong specular highlight reflections", false, 0.0f, false, 0.0f);
        registerConVar("mat_bumpmap", "1", FCVAR_ARCHIVE, "Enable normal and bump mapping", false, 0.0f, false, 0.0f);
        registerConVar("mat_diffuse", "1", FCVAR_ARCHIVE, "Enable diffuse albedo texture sampling", false, 0.0f, false, 0.0f);
        registerConVar("mat_phong", "1", FCVAR_ARCHIVE, "Enable Phong shading reflection exponent", false, 0.0f, false, 0.0f);
        registerConVar("mat_normalmaps", "1", FCVAR_ARCHIVE, "Calculate tangent-space normal perturbations", false, 0.0f, false, 0.0f);
        registerConVar("mat_fastspecular", "1", FCVAR_ARCHIVE, "Fast specular approximation without exp call", false, 0.0f, false, 0.0f);
        registerConVar("mat_filtertextures", "1", FCVAR_ARCHIVE, "Bilinear/trilinear texture sampling filter", false, 0.0f, false, 0.0f);
        registerConVar("mat_mipmaptextures", "1", FCVAR_ARCHIVE, "Generate and sample texture mipmaps", false, 0.0f, false, 0.0f);
        registerConVar("mat_trilinear", "1", FCVAR_ARCHIVE, "Use trilinear mipmap filtering", false, 0.0f, false, 0.0f);
        registerConVar("mat_forceaniso", "16", FCVAR_ARCHIVE, "Anisotropic filtering level (1, 2, 4, 8, 16)", false, 0.0f, false, 0.0f);
        registerConVar("mat_compressedtextures", "1", FCVAR_ARCHIVE, "Use compressed DXT1/DXT5 texture formats", false, 0.0f, false, 0.0f);
        registerConVar("mat_hdr_level", "2", FCVAR_ARCHIVE, "HDR level (0=LDR, 1=bloom, 2=full 16-bit FP HDR)", false, 0.0f, false, 0.0f);
        registerConVar("mat_hdr_tonemapscale", "1.0", FCVAR_ARCHIVE, "ACES filmic tonemapping exposure scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_queue_mode", "-1", FCVAR_ARCHIVE, "Threaded material queue mode (-1=auto, 0=single, 2=multithreaded)", false, 0.0f, false, 0.0f);
        registerConVar("mat_show_texture_memory_usage", "0", FCVAR_CHEAT, "Display VRAM texture memory consumption on HUD", false, 0.0f, false, 0.0f);
        registerConVar("mat_leafvis", "0", FCVAR_CHEAT, "Visualize BSP leaf geometry containing camera", false, 0.0f, false, 0.0f);
        registerConVar("mat_depthbias_decal", "-0.05", FCVAR_ARCHIVE, "Decal depth bias offset to prevent Z-fighting", false, 0.0f, false, 0.0f);
        registerConVar("mat_depthbias_normal", "0.0", FCVAR_ARCHIVE, "Geometry depth bias for normal mapping", false, 0.0f, false, 0.0f);
        registerConVar("mat_slope_scatter", "1.0", FCVAR_ARCHIVE, "Slope-dependent surface scattering scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_drawwater", "1", FCVAR_ARCHIVE, "Render water surface planes", false, 0.0f, false, 0.0f);
        registerConVar("mat_viewportscale", "1.0", FCVAR_ARCHIVE, "Dynamic 3D render resolution scale factor", false, 0.0f, false, 0.0f);
        registerConVar("mat_viewportscalex", "1.0", FCVAR_ARCHIVE, "Horizontal viewport render resolution scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_viewportscaley", "1.0", FCVAR_ARCHIVE, "Vertical viewport render resolution scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_monitorgamma", "2.2", FCVAR_ARCHIVE, "Display monitor gamma exponent (1.6 - 2.6)", false, 0.0f, false, 0.0f);
        registerConVar("mat_monitorgamma_tv_enabled", "0", FCVAR_ARCHIVE, "Use limited TV 16-235 color levels", false, 0.0f, false, 0.0f);
        registerConVar("mat_colorcorrection", "1", FCVAR_ARCHIVE, "Enable runtime 3D LUT color grading", false, 0.0f, false, 0.0f);
        registerConVar("mat_colcorrection_forceones", "0", FCVAR_CHEAT, "Force neutral color grading LUT", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_enabled", "0", FCVAR_ARCHIVE, "Enable camera rotation motion blur", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_strength", "1.0", FCVAR_ARCHIVE, "Motion blur intensity multiplier", false, 0.0f, false, 0.0f);
        registerConVar("mat_grain_enable", "0", FCVAR_ARCHIVE, "Enable cinematic photographic film grain", false, 0.0f, false, 0.0f);
        registerConVar("mat_grain_scale_override", "1.0", FCVAR_ARCHIVE, "Film grain noise frequency scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_vignette_enable", "1", FCVAR_ARCHIVE, "Enable lens edge vignette shadowing", false, 0.0f, false, 0.0f);
        registerConVar("mat_tonemapping_occlusion_use_stencil", "0", FCVAR_ARCHIVE, "Use stencil buffer for tonemapping metering", false, 0.0f, false, 0.0f);
        registerConVar("mat_software_aa_strength", "0.0", FCVAR_ARCHIVE, "Software post-process anti-aliasing strength", false, 0.0f, false, 0.0f);
        registerConVar("mat_software_aa_quality", "0", FCVAR_ARCHIVE, "Software anti-aliasing sample count", false, 0.0f, false, 0.0f);
        registerConVar("mat_software_aa_edge_threshold", "0.5", FCVAR_ARCHIVE, "Luminance edge threshold for FXAA", false, 0.0f, false, 0.0f);
        registerConVar("mat_software_aa_blur_one_pixel_lines", "0.5", FCVAR_ARCHIVE, "Blur sub-pixel thin lines in AA", false, 0.0f, false, 0.0f);
        registerConVar("mat_showlowresimage", "0", FCVAR_CHEAT, "Show low-res placeholder image for missing textures", false, 0.0f, false, 0.0f);
        registerConVar("mat_showmaterials", "0", FCVAR_CHEAT, "Print loaded material shader names", false, 0.0f, false, 0.0f);
        registerConVar("mat_showtextures", "0", FCVAR_CHEAT, "Print loaded GPU texture allocations", false, 0.0f, false, 0.0f);
        registerConVar("mat_reloadallmaterials", "1", FCVAR_CHEAT, "Reload all material definitions from disk", false, 0.0f, false, 0.0f);
        registerConVar("mat_reloadtextures", "1", FCVAR_CHEAT, "Reload all texture bitmaps from disk", false, 0.0f, false, 0.0f);
        registerConVar("mat_reloadshaders", "1", FCVAR_CHEAT, "Compile and link all GLSL shader programs", false, 0.0f, false, 0.0f);
        registerConVar("mat_antialias", "4", FCVAR_ARCHIVE, "Hardware MSAA sample count (0, 2, 4, 8)", false, 0.0f, false, 0.0f);
        registerConVar("mat_alphacoverage", "1", FCVAR_ARCHIVE, "Enable Alpha-to-Coverage MSAA on foliage textures", false, 0.0f, false, 0.0f);
        registerConVar("mat_autoexposure_max", "2.0", FCVAR_ARCHIVE, "Maximum HDR exposure adaptation ceiling", false, 0.0f, false, 0.0f);
        registerConVar("mat_autoexposure_min", "0.5", FCVAR_ARCHIVE, "Minimum HDR exposure adaptation floor", false, 0.0f, false, 0.0f);
        registerConVar("mat_bloomamount_rate", "0.05", FCVAR_ARCHIVE, "Rate at which bloom adapts to light changes", false, 0.0f, false, 0.0f);
        registerConVar("mat_bloomscale", "1.0", FCVAR_ARCHIVE, "Global bloom intensity scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_bufferprimitives", "1", FCVAR_ARCHIVE, "Buffer vertex arrays in GPU memory", false, 0.0f, false, 0.0f);
        registerConVar("mat_clipz", "1", FCVAR_ARCHIVE, "Enable hardware frustum depth clipping", false, 0.0f, false, 0.0f);
        registerConVar("mat_debug_bloom", "0", FCVAR_CHEAT, "Display raw bright-pass bloom buffer", false, 0.0f, false, 0.0f);
        registerConVar("mat_debugalbedo", "0", FCVAR_CHEAT, "Render raw diffuse texture albedos only", false, 0.0f, false, 0.0f);
        registerConVar("mat_debugdepth", "0", FCVAR_CHEAT, "Render camera depth buffer as greyscale", false, 0.0f, false, 0.0f);
        registerConVar("mat_disable_bloom", "0", FCVAR_ARCHIVE, "Completely disable bloom rendering pass", false, 0.0f, false, 0.0f);
        registerConVar("mat_disable_fancy_blending", "0", FCVAR_ARCHIVE, "Disable complex multi-layer terrain blending", false, 0.0f, false, 0.0f);
        registerConVar("mat_disable_lightwarp", "0", FCVAR_ARCHIVE, "Disable 1D cel-shaded lightwarp textures", false, 0.0f, false, 0.0f);
        registerConVar("mat_drawflat", "0", FCVAR_CHEAT, "Render flat shaded geometry", false, 0.0f, false, 0.0f);
        registerConVar("mat_dxlevel", "95", FCVAR_ARCHIVE, "Material pipeline compatibility level", false, 0.0f, false, 0.0f);
        registerConVar("mat_dynamic_tonemapping", "1", FCVAR_ARCHIVE, "Dynamically adapt eye pupil exposure to scene luminance", false, 0.0f, false, 0.0f);
        registerConVar("mat_envmapsize", "128", FCVAR_ARCHIVE, "Cubemap environment texture resolution", false, 0.0f, false, 0.0f);
        registerConVar("mat_exposurescale", "1.0", FCVAR_ARCHIVE, "Manual exposure compensation scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_fastclip", "0", FCVAR_ARCHIVE, "Fast plane clipping optimization", false, 0.0f, false, 0.0f);
        registerConVar("mat_fastnobump", "0", FCVAR_ARCHIVE, "Disable bump mapping on secondary passes", false, 0.0f, false, 0.0f);
        registerConVar("mat_fillrate", "0", FCVAR_CHEAT, "Display overdraw heatmap in red overlay", false, 0.0f, false, 0.0f);
        registerConVar("mat_filterlightmaps", "1", FCVAR_ARCHIVE, "Bilinear filtering on lightmap textures", false, 0.0f, false, 0.0f);
        registerConVar("mat_forcedynamic", "0", FCVAR_ARCHIVE, "Force dynamic lighting paths", false, 0.0f, false, 0.0f);
        registerConVar("mat_forcehardwaresync", "0", FCVAR_ARCHIVE, "Force glFinish GPU sync every frame", false, 0.0f, false, 0.0f);
        registerConVar("mat_frame_sync_enable", "1", FCVAR_ARCHIVE, "Synchronize frame presentation", false, 0.0f, false, 0.0f);
        registerConVar("mat_levelflush", "1", FCVAR_ARCHIVE, "Flush texture memory when loading new map", false, 0.0f, false, 0.0f);
        registerConVar("mat_lightmap_ambient", "0.1", FCVAR_ARCHIVE, "Ambient floor added to lightmap samples", false, 0.0f, false, 0.0f);
        registerConVar("mat_lightmap_density", "1.0", FCVAR_ARCHIVE, "Lightmap texel density multiplier", false, 0.0f, false, 0.0f);
        registerConVar("mat_loadtextures", "1", FCVAR_ARCHIVE, "Load texture maps from disk into VRAM", false, 0.0f, false, 0.0f);
        registerConVar("mat_luxels", "0", FCVAR_CHEAT, "Draw lightmap luxel grid overlay on surfaces", false, 0.0f, false, 0.0f);
        registerConVar("mat_managedtextures", "1", FCVAR_ARCHIVE, "Allow driver to manage texture paging", false, 0.0f, false, 0.0f);
        registerConVar("mat_max_world_mesh_vertices", "65536", FCVAR_ARCHIVE, "Max vertices per static brush batch", false, 0.0f, false, 0.0f);
        registerConVar("mat_maxframelatency", "1", FCVAR_ARCHIVE, "Max frames queued ahead on GPU", false, 0.0f, false, 0.0f);
        registerConVar("mat_measurefillrate", "0", FCVAR_CHEAT, "Benchmark fillrate overhead", false, 0.0f, false, 0.0f);
        registerConVar("mat_norendering", "0", FCVAR_CHEAT, "Disable 3D rendering loop entirely", false, 0.0f, false, 0.0f);
        registerConVar("mat_normalmap", "1", FCVAR_ARCHIVE, "Enable normal mapping on surfaces", false, 0.0f, false, 0.0f);
        registerConVar("mat_parallaxmap", "1", FCVAR_ARCHIVE, "Enable steep parallax occlusion height mapping", false, 0.0f, false, 0.0f);
        registerConVar("mat_powersavingsmode", "0", FCVAR_ARCHIVE, "Throttle framerate when idle", false, 0.0f, false, 0.0f);
        registerConVar("mat_reducefillrate", "0", FCVAR_ARCHIVE, "Reduce shader complexity on low-end GPUs", false, 0.0f, false, 0.0f);
        registerConVar("mat_reduceparticles", "0", FCVAR_ARCHIVE, "Reduce particle density on low-end GPUs", false, 0.0f, false, 0.0f);
        registerConVar("mat_reversedepth", "0", FCVAR_ARCHIVE, "Use inverted reversed-Z floating point depth buffer", false, 0.0f, false, 0.0f);
        registerConVar("mat_shadowstate", "1", FCVAR_ARCHIVE, "Enable shadow projection buffers", false, 0.0f, false, 0.0f);
        registerConVar("mat_show_histogram", "0", FCVAR_CHEAT, "Display scene luminance histogram on HUD", false, 0.0f, false, 0.0f);
        registerConVar("mat_showenvmapmask", "0", FCVAR_CHEAT, "Display specular mask texture in black/white", false, 0.0f, false, 0.0f);
        registerConVar("mat_showmiplevels", "0", FCVAR_CHEAT, "Colorize textures by active mipmap level", false, 0.0f, false, 0.0f);
        registerConVar("mat_softwarefog", "0", FCVAR_ARCHIVE, "Calculate fog in CPU vertex shader", false, 0.0f, false, 0.0f);
        registerConVar("mat_surfaceid", "0", FCVAR_CHEAT, "Draw surface ID integer above brushes", false, 0.0f, false, 0.0f);
        registerConVar("mat_tessellationlevel", "1.0", FCVAR_ARCHIVE, "Terrain displacement tessellation multiplier", false, 0.0f, false, 0.0f);
        registerConVar("mat_tonemap_algorithm", "1", FCVAR_ARCHIVE, "Tonemapping algorithm (0=Reinhard, 1=ACES filmic, 2=Uncharted)", false, 0.0f, false, 0.0f);
        registerConVar("mat_visualize_dof", "0", FCVAR_CHEAT, "Visualize depth of field blur radii", false, 0.0f, false, 0.0f);
        registerConVar("mat_wateroverlaysize", "512", FCVAR_ARCHIVE, "Water refraction texture resolution", false, 0.0f, false, 0.0f);
        registerConVar("mp_restartround", "0", FCVAR_GAMEDLL, "Trigger match round restart in N seconds", false, 0.0f, false, 0.0f);
        registerConVar("mp_roundtime", "5", FCVAR_NOTIFY | FCVAR_REPLICATED, "Round duration limit in minutes", false, 0.0f, false, 0.0f);
        registerConVar("mp_timelimit", "20", FCVAR_NOTIFY | FCVAR_REPLICATED, "Match duration limit in minutes", false, 0.0f, false, 0.0f);
        registerConVar("mp_fraglimit", "30", FCVAR_NOTIFY | FCVAR_REPLICATED, "Kills required to win match in FFA/DM", false, 0.0f, false, 0.0f);
        registerConVar("mp_maxrounds", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Max rounds before map change (0=disabled)", false, 0.0f, false, 0.0f);
        registerConVar("mp_friendlyfire", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Allow teammate friendly fire damage", false, 0.0f, false, 0.0f);
        registerConVar("mp_autoteambalance", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Automatically balance team player counts", false, 0.0f, false, 0.0f);
        registerConVar("mp_limitteams", "2", FCVAR_NOTIFY | FCVAR_REPLICATED, "Max player count differential between teams", false, 0.0f, false, 0.0f);
        registerConVar("mp_forcecamera", "0", FCVAR_REPLICATED, "Spectator camera restriction (0=all, 1=team only, 2=first person)", false, 0.0f, false, 0.0f);
        registerConVar("mp_allowspectators", "1", FCVAR_REPLICATED, "Allow players to spectate match", false, 0.0f, false, 0.0f);
        registerConVar("mp_footsteps", "1", FCVAR_REPLICATED, "Play audio footstep sounds on surfaces", false, 0.0f, false, 0.0f);
        registerConVar("mp_flashlight", "1", FCVAR_REPLICATED, "Allow flashlight usage in match", false, 0.0f, false, 0.0f);
        registerConVar("mp_falldamage", "1", FCVAR_REPLICATED, "Inflict fall impact damage on landing from height", false, 0.0f, false, 0.0f);
        registerConVar("mp_freezetime", "4", FCVAR_NOTIFY | FCVAR_REPLICATED, "Seconds players are frozen at start of round", false, 0.0f, false, 0.0f);
        registerConVar("mp_buytime", "45", FCVAR_NOTIFY | FCVAR_REPLICATED, "Seconds players can purchase equipment", false, 0.0f, false, 0.0f);
        registerConVar("mp_startmoney", "800", FCVAR_NOTIFY | FCVAR_REPLICATED, "Starting credits per player", false, 0.0f, false, 0.0f);
        registerConVar("mp_c4timer", "45", FCVAR_NOTIFY | FCVAR_REPLICATED, "Seconds before planted explosive detonates", false, 0.0f, false, 0.0f);
        registerConVar("mp_hostagepenalty", "1", FCVAR_REPLICATED, "Penalty for killing civilian hostages", false, 0.0f, false, 0.0f);
        registerConVar("mp_tkpunish", "1", FCVAR_ARCHIVE, "Punish team-killers with next round suicide", false, 0.0f, false, 0.0f);
        registerConVar("mp_spawnprotectiontime", "3", FCVAR_REPLICATED, "Seconds players are invulnerable after spawning", false, 0.0f, false, 0.0f);
        registerConVar("mp_chattime", "10", FCVAR_ARCHIVE, "Seconds scoreboard displays after match end", false, 0.0f, false, 0.0f);
        registerConVar("mp_decals", "200", FCVAR_ARCHIVE, "Max decals saved in match", false, 0.0f, false, 0.0f);
        registerConVar("mp_defaultteam", "0", FCVAR_NONE, "Default team for new joiners", false, 0.0f, false, 0.0f);
        registerConVar("mp_disable_autokick", "0", FCVAR_NONE, "Disable automatic kicking of idle or TK players", false, 0.0f, false, 0.0f);
        registerConVar("mp_enableroundwaittime", "1", FCVAR_REPLICATED, "Wait for all players to connect before starting", false, 0.0f, false, 0.0f);
        registerConVar("mp_fadetoblack", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Fade screen to black upon player elimination", false, 0.0f, false, 0.0f);
        registerConVar("mp_forcerespawn", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Force automatic respawn without waiting for button", false, 0.0f, false, 0.0f);
        registerConVar("mp_halftime", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Switch teams at match midpoint", false, 0.0f, false, 0.0f);
        registerConVar("mp_halftime_duration", "15", FCVAR_REPLICATED, "Duration of halftime pause in seconds", false, 0.0f, false, 0.0f);
        registerConVar("mp_match_can_clinch", "1", FCVAR_REPLICATED, "End match early once team clinches victory", false, 0.0f, false, 0.0f);
        registerConVar("mp_match_end_changelevel", "1", FCVAR_REPLICATED, "Cycle to next map upon match completion", false, 0.0f, false, 0.0f);
        registerConVar("mp_match_restart_delay", "15", FCVAR_REPLICATED, "Seconds to wait before match restart", false, 0.0f, false, 0.0f);
        registerConVar("mp_overtime_enable", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Play overtime rounds upon tie", false, 0.0f, false, 0.0f);
        registerConVar("mp_overtime_maxrounds", "6", FCVAR_REPLICATED, "Max rounds played per overtime phase", false, 0.0f, false, 0.0f);
        registerConVar("mp_playerid", "0", FCVAR_REPLICATED, "Player name tag display over character heads", false, 0.0f, false, 0.0f);
        registerConVar("mp_respawn_immunitytime", "3.0", FCVAR_REPLICATED, "Seconds of spawn protection shield", false, 0.0f, false, 0.0f);
        registerConVar("mp_round_restart_delay", "5", FCVAR_REPLICATED, "Delay in seconds after round victory before reset", false, 0.0f, false, 0.0f);
        registerConVar("mp_scrambleteams", "0", FCVAR_GAMEDLL, "Scramble all players randomly into teams", false, 0.0f, false, 0.0f);
        registerConVar("mp_solid_teammates", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Teammates have physical solid collision hulls", false, 0.0f, false, 0.0f);
        registerConVar("mp_spectators_max", "8", FCVAR_REPLICATED, "Maximum spectator slots on server", false, 0.0f, false, 0.0f);
        registerConVar("mp_switchteams", "0", FCVAR_GAMEDLL, "Instantly swap team rosters", false, 0.0f, false, 0.0f);
        registerConVar("mp_teamcashawards", "1", FCVAR_REPLICATED, "Award team cash for match objectives", false, 0.0f, false, 0.0f);
        registerConVar("mp_teamplay", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Enable team-based scoring modes", false, 0.0f, false, 0.0f);
        registerConVar("mp_warmup_end", "0", FCVAR_GAMEDLL, "End pre-match warmup period immediately", false, 0.0f, false, 0.0f);
        registerConVar("mp_warmup_start", "0", FCVAR_GAMEDLL, "Start pre-match warmup period", false, 0.0f, false, 0.0f);
        registerConVar("mp_warmuptime", "60", FCVAR_REPLICATED, "Duration of warmup period in seconds", false, 0.0f, false, 0.0f);
        registerConVar("mp_weaponstay", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Weapons stay on pickup pedestals in deathmatch", false, 0.0f, false, 0.0f);
        registerConVar("mp_winlimit", "16", FCVAR_NOTIFY | FCVAR_REPLICATED, "Rounds won required to win match", false, 0.0f, false, 0.0f);
        registerConVar("bot_add", "0", FCVAR_GAMEDLL, "Add a bot into match", false, 0.0f, false, 0.0f);
        registerConVar("bot_add_t", "0", FCVAR_GAMEDLL, "Add a bot to terrorist/Alpha team", false, 0.0f, false, 0.0f);
        registerConVar("bot_add_ct", "0", FCVAR_GAMEDLL, "Add a bot to counter-terrorist/Beta team", false, 0.0f, false, 0.0f);
        registerConVar("bot_add_alpha", "0", FCVAR_GAMEDLL, "Add a bot to Team Alpha", false, 0.0f, false, 0.0f);
        registerConVar("bot_add_bravo", "0", FCVAR_GAMEDLL, "Add a bot to Team Bravo", false, 0.0f, false, 0.0f);
        registerConVar("bot_kick", "", FCVAR_GAMEDLL, "Kick specified bot by name", false, 0.0f, false, 0.0f);
        registerConVar("bot_kick_all", "0", FCVAR_GAMEDLL, "Kick all active bots from match", false, 0.0f, false, 0.0f);
        registerConVar("bot_kill", "", FCVAR_GAMEDLL, "Kill specified bot", false, 0.0f, false, 0.0f);
        registerConVar("bot_stop", "0", FCVAR_CHEAT, "Stop all bot movement and pathfinding", false, 0.0f, false, 0.0f);
        registerConVar("bot_freeze", "0", FCVAR_CHEAT, "Freeze bots in current positions", false, 0.0f, false, 0.0f);
        registerConVar("bot_dont_shoot", "0", FCVAR_CHEAT, "Prevent bots from discharging weapons", false, 0.0f, false, 0.0f);
        registerConVar("bot_difficulty", "1", FCVAR_ARCHIVE | FCVAR_REPLICATED, "Bot skill difficulty (0=easy, 1=normal, 2=hard, 3=expert)", false, 0.0f, false, 0.0f);
        registerConVar("bot_quota", "4", FCVAR_NOTIFY | FCVAR_REPLICATED, "Target count of bots kept in match", false, 0.0f, false, 0.0f);
        registerConVar("bot_quota_mode", "fill", FCVAR_REPLICATED, "Bot quota mode (normal, fill, match)", false, 0.0f, false, 0.0f);
        registerConVar("bot_auto_vacate", "1", FCVAR_ARCHIVE, "Bots automatically leave when human players connect", false, 0.0f, false, 0.0f);
        registerConVar("bot_join_after_player", "1", FCVAR_ARCHIVE, "Bots only join after first human connects", false, 0.0f, false, 0.0f);
        registerConVar("bot_prefix", "[BOT]", FCVAR_ARCHIVE, "Text prefix prepended to bot names", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_rogues", "0", FCVAR_ARCHIVE, "Bots can disregard squad orders", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_pistols", "1", FCVAR_ARCHIVE, "Bots can purchase and use pistols", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_shotguns", "1", FCVAR_ARCHIVE, "Bots can purchase and use shotguns", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_sub_machine_guns", "1", FCVAR_ARCHIVE, "Bots can purchase and use SMGs", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_rifles", "1", FCVAR_ARCHIVE, "Bots can purchase and use assault rifles", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_snipers", "1", FCVAR_ARCHIVE, "Bots can purchase and use sniper rifles", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_grenades", "1", FCVAR_ARCHIVE, "Bots can purchase and throw grenades", false, 0.0f, false, 0.0f);
        registerConVar("bot_allow_machine_guns", "1", FCVAR_ARCHIVE, "Bots can purchase and use heavy machine guns", false, 0.0f, false, 0.0f);
        registerConVar("bot_join_team", "any", FCVAR_ARCHIVE, "Team bots join (any, alpha, bravo)", false, 0.0f, false, 0.0f);
        registerConVar("bot_chatter", "normal", FCVAR_ARCHIVE, "Bot radio chatter frequency (off, radio, minimal, normal)", false, 0.0f, false, 0.0f);
        registerConVar("bot_profile_db", "assets/configs/bot_profiles.json", FCVAR_ARCHIVE, "Path to bot profile database", false, 0.0f, false, 0.0f);
        registerConVar("bot_mimic", "0", FCVAR_CHEAT, "Bots mimic player actions and rotation", false, 0.0f, false, 0.0f);
        registerConVar("bot_mimic_yaw_offset", "0", FCVAR_CHEAT, "Yaw offset for bot mimicry", false, 0.0f, false, 0.0f);
        registerConVar("bot_crouch", "0", FCVAR_CHEAT, "Force bots into crouching posture", false, 0.0f, false, 0.0f);
        registerConVar("bot_walk", "0", FCVAR_CHEAT, "Force bots into walking speed", false, 0.0f, false, 0.0f);
        registerConVar("bot_loadout", "", FCVAR_CHEAT, "Force specific weapon ID loadout on bots", false, 0.0f, false, 0.0f);
        registerConVar("bot_debug", "0", FCVAR_CHEAT, "Display bot internal behavior tree states", false, 0.0f, false, 0.0f);
        registerConVar("bot_traceview", "0", FCVAR_CHEAT, "Draw bot sight raycasts and target vectors", false, 0.0f, false, 0.0f);
        registerConVar("bot_randombuy", "0", FCVAR_CHEAT, "Bots buy random weapons", false, 0.0f, false, 0.0f);
        registerConVar("bot_snipers_only", "0", FCVAR_CHEAT, "Force bots to use sniper rifles only", false, 0.0f, false, 0.0f);
        registerConVar("bot_pistols_only", "0", FCVAR_CHEAT, "Force bots to use pistols only", false, 0.0f, false, 0.0f);
        registerConVar("bot_knives_only", "0", FCVAR_CHEAT, "Force bots to use melee crowbar/pipe only", false, 0.0f, false, 0.0f);
        registerConVar("bot_all_weapons", "0", FCVAR_CHEAT, "Allow bots to equip all available weapon IDs", false, 0.0f, false, 0.0f);
        registerConVar("bot_auto_follow", "0", FCVAR_ARCHIVE, "Bots automatically follow nearby human leader", false, 0.0f, false, 0.0f);
        registerConVar("bot_flipout", "0", FCVAR_CHEAT, "Bots jump and look randomly", false, 0.0f, false, 0.0f);
        registerConVar("bot_defer_to_human", "0", FCVAR_ARCHIVE, "Bots wait for human to pick up scenario items", false, 0.0f, false, 0.0f);
        registerConVar("bot_zombie", "0", FCVAR_CHEAT, "Bots walk directly toward nearest target", false, 0.0f, false, 0.0f);
        registerConVar("ai_step", "0", FCVAR_CHEAT, "Step AI simulation forward by single tick", false, 0.0f, false, 0.0f);
        registerConVar("ai_resume", "0", FCVAR_CHEAT, "Resume stepped AI simulation", false, 0.0f, false, 0.0f);
        registerConVar("ai_show_connect", "0", FCVAR_CHEAT, "Draw graph connection lines between waypoints", false, 0.0f, false, 0.0f);
        registerConVar("ai_show_visibility", "0", FCVAR_CHEAT, "Draw visibility lines between nodes", false, 0.0f, false, 0.0f);
        registerConVar("ai_show_grid", "0", FCVAR_CHEAT, "Draw navigation grid cells", false, 0.0f, false, 0.0f);
        registerConVar("ai_nodes", "0", FCVAR_CHEAT, "Draw navigation node spheres", false, 0.0f, false, 0.0f);
        registerConVar("ai_drop_hint", "0", FCVAR_CHEAT, "Drop tactical hint at aim position", false, 0.0f, false, 0.0f);
        registerConVar("ai_debug_doors", "0", FCVAR_CHEAT, "Debug door path traversal for NPCs", false, 0.0f, false, 0.0f);
        registerConVar("ai_debug_los", "0", FCVAR_CHEAT, "Debug NPC line-of-sight queries", false, 0.0f, false, 0.0f);
        registerConVar("ai_debug_nav", "0", FCVAR_CHEAT, "Debug NPC pathfinding route calculation", false, 0.0f, false, 0.0f);
        registerConVar("ai_debug_path", "0", FCVAR_CHEAT, "Debug NPC active navigation path", false, 0.0f, false, 0.0f);
        registerConVar("ai_debug_speech", "0", FCVAR_CHEAT, "Print NPC dialog line triggers", false, 0.0f, false, 0.0f);
        registerConVar("ai_report_task_timings", "0", FCVAR_CHEAT, "Print time spent in NPC behavioral tasks", false, 0.0f, false, 0.0f);
        registerConVar("ai_sequence_debug", "0", FCVAR_CHEAT, "Print active skeletal animation sequence", false, 0.0f, false, 0.0f);
        registerConVar("ai_test_los", "0", FCVAR_CHEAT, "Test line of sight between player and targeted NPC", false, 0.0f, false, 0.0f);
        registerConVar("ai_block_damage", "0", FCVAR_CHEAT, "Prevent all damage to NPCs", false, 0.0f, false, 0.0f);
        registerConVar("ai_force_serverside_ragdoll", "0", FCVAR_CHEAT, "Simulate authoritative ragdolls on server", false, 0.0f, false, 0.0f);
        registerConVar("ai_lead_time", "0.2", FCVAR_ARCHIVE, "Target velocity leading lookahead seconds", false, 0.0f, false, 0.0f);
        registerConVar("ai_reaction_delay_alert", "0.25", FCVAR_ARCHIVE, "Reaction time delay when alerted (seconds)", false, 0.0f, false, 0.0f);
        registerConVar("ai_reaction_delay_idle", "0.5", FCVAR_ARCHIVE, "Reaction time delay when idle (seconds)", false, 0.0f, false, 0.0f);
        registerConVar("ai_rebuildgraph", "0", FCVAR_CHEAT, "Rebuild pathfinding navigation graph", false, 0.0f, false, 0.0f);
        registerConVar("ai_wake", "0", FCVAR_CHEAT, "Wake all dormant NPCs in world", false, 0.0f, false, 0.0f);
        registerConVar("phys_timescale", "1.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Physics simulation timescale", false, 0.0f, false, 0.0f);
        registerConVar("phys_pushscale", "1.0", FCVAR_ARCHIVE | FCVAR_REPLICATED, "Physics push force multiplier", false, 0.0f, false, 0.0f);
        registerConVar("phys_impactforcescale", "1.0", FCVAR_ARCHIVE, "Impulse force imparted on physical props by impacts", false, 0.0f, false, 0.0f);
        registerConVar("phys_upimpactforcescale", "0.5", FCVAR_ARCHIVE, "Vertical lift force imparted on props by explosions", false, 0.0f, false, 0.0f);
        registerConVar("phys_speeds", "0", FCVAR_CHEAT, "Display physics engine step times", false, 0.0f, false, 0.0f);
        registerConVar("phys_enable_sleeping", "1", FCVAR_ARCHIVE, "Allow motionless rigidbodies to enter sleeping state", false, 0.0f, false, 0.0f);
        registerConVar("phys_sleep_delay", "2.0", FCVAR_ARCHIVE, "Seconds motionless before rigidbody sleeps", false, 0.0f, false, 0.0f);
        registerConVar("phys_penetration_dist", "0.05", FCVAR_CHEAT, "Maximum penetration before positional correction", false, 0.0f, false, 0.0f);
        registerConVar("phys_penetration_force", "100.0", FCVAR_CHEAT, "Penetration resolution impulse multiplier", false, 0.0f, false, 0.0f);
        registerConVar("phys_contact_damping", "0.1", FCVAR_ARCHIVE, "Contact constraint damping factor", false, 0.0f, false, 0.0f);
        registerConVar("vcollide_wireframe", "0", FCVAR_CHEAT, "Draw VCollide physics collision convex hulls in wireframe", false, 0.0f, false, 0.0f);
        registerConVar("vcollide_wireframe_axes", "0", FCVAR_CHEAT, "Draw local origin axes of physics colliders", false, 0.0f, false, 0.0f);
        registerConVar("showtriggers", "0", FCVAR_CHEAT, "Draw trigger entity brush boundaries", false, 0.0f, false, 0.0f);
        registerConVar("collision_shake", "1.0", FCVAR_ARCHIVE, "Screen shake force from heavy physical collisions", false, 0.0f, false, 0.0f);
        registerConVar("collision_damage", "1.0", FCVAR_ARCHIVE, "Health damage scale from high-speed prop collisions", false, 0.0f, false, 0.0f);
        registerConVar("collision_restitution", "0.2", FCVAR_ARCHIVE, "Coefficient of restitution (bounciness) for physics props", false, 0.0f, false, 0.0f);
        registerConVar("csg_merge_coplanar", "1", FCVAR_ARCHIVE, "Merge coplanar polygons during CSG brush boolean operations", false, 0.0f, false, 0.0f);
        registerConVar("csg_snap_grid", "1", FCVAR_ARCHIVE, "Snap CSG brush vertices to integer grid units", false, 0.0f, false, 0.0f);
        registerConVar("csg_epsilon", "0.0001", FCVAR_ARCHIVE, "Numerical epsilon for CSG plane classification", false, 0.0f, false, 0.0f);
        registerConVar("csg_fast_clip", "1", FCVAR_ARCHIVE, "Use fast BSP clipping for CSG carving", false, 0.0f, false, 0.0f);
        registerConVar("csg_debug_faces", "0", FCVAR_CHEAT, "Draw CSG carved face normals and split planes", false, 0.0f, false, 0.0f);
        registerConVar("phys_show_active", "0", FCVAR_CHEAT, "Colorize rigidbodies green when active, red when sleeping", false, 0.0f, false, 0.0f);
        registerConVar("phys_torque_damping", "0.1", FCVAR_ARCHIVE, "Angular rotational damping factor on rigidbodies", false, 0.0f, false, 0.0f);
        registerConVar("phys_linear_damping", "0.05", FCVAR_ARCHIVE, "Linear translational air drag damping factor", false, 0.0f, false, 0.0f);
        registerConVar("phys_mass_scale", "1.0", FCVAR_CHEAT, "Global mass multiplier for physical props", false, 0.0f, false, 0.0f);
        registerConVar("phys_max_angular_velocity", "3600.0", FCVAR_ARCHIVE, "Maximum rotational angular speed in degrees/sec", false, 0.0f, false, 0.0f);
        registerConVar("phys_max_linear_velocity", "3000.0", FCVAR_ARCHIVE, "Maximum linear velocity in meters/sec", false, 0.0f, false, 0.0f);
        registerConVar("phys_solver_iterations", "10", FCVAR_ARCHIVE, "Constraint solver iteration passes per step", false, 0.0f, false, 0.0f);
        registerConVar("phys_substeps", "2", FCVAR_ARCHIVE, "Physics simulation sub-stepping passes per frame", false, 0.0f, false, 0.0f);
        registerConVar("phys_ccd_enabled", "1", FCVAR_ARCHIVE, "Continuous Collision Detection (CCD) for fast-moving projectiles", false, 0.0f, false, 0.0f);
        registerConVar("phys_buoyancy_scale", "1.0", FCVAR_ARCHIVE, "Water buoyancy flotation force multiplier", false, 0.0f, false, 0.0f);
        registerConVar("phys_water_drag", "2.0", FCVAR_ARCHIVE, "Water translational drag deceleration coefficient", false, 0.0f, false, 0.0f);
        registerConVar("phys_fluid_density", "1000.0", FCVAR_ARCHIVE, "Density of water in kg/m^3", false, 0.0f, false, 0.0f);
        registerConVar("phys_debris_lifetime", "30.0", FCVAR_ARCHIVE, "Lifetime in seconds before broken physical debris despawns", false, 0.0f, false, 0.0f);
        registerConVar("volume", "1.0", FCVAR_ARCHIVE, "Master game audio volume (0.0 to 1.0)", false, 0.0f, false, 0.0f);
        registerConVar("snd_musicvolume", "0.7", FCVAR_ARCHIVE, "Music playback volume", false, 0.0f, false, 0.0f);
        registerConVar("snd_sfxvolume", "1.0", FCVAR_ARCHIVE, "Sound effects volume", false, 0.0f, false, 0.0f);
        registerConVar("snd_voicevolume", "1.0", FCVAR_ARCHIVE, "In-game player voice communication volume", false, 0.0f, false, 0.0f);
        registerConVar("snd_mute_losefocus", "1", FCVAR_ARCHIVE, "Mute game audio when application window loses focus", false, 0.0f, false, 0.0f);
        registerConVar("snd_pitchquality", "1", FCVAR_ARCHIVE, "High-quality Hermite audio pitch resampling", false, 0.0f, false, 0.0f);
        registerConVar("snd_mixahead", "0.05", FCVAR_ARCHIVE, "Audio buffer mixing latency in seconds (50ms)", false, 0.0f, false, 0.0f);
        registerConVar("snd_surround_speakers", "2", FCVAR_ARCHIVE, "Speaker configuration (2=Stereo, 4=Quad, 5=5.1, 7=7.1)", false, 0.0f, false, 0.0f);
        registerConVar("snd_show", "0", FCVAR_CHEAT, "Display currently playing sound names and coordinates on HUD", false, 0.0f, false, 0.0f);
        registerConVar("snd_visualize", "0", FCVAR_CHEAT, "Draw 3D spatial spheres around sound emission sources", false, 0.0f, false, 0.0f);
        registerConVar("snd_ducking_off", "0", FCVAR_ARCHIVE, "Disable audio ducking volume drops during voice lines", false, 0.0f, false, 0.0f);
        registerConVar("snd_foliage_db_loss", "4", FCVAR_ARCHIVE, "Decibel volume drop per foliage brush occlusion", false, 0.0f, false, 0.0f);
        registerConVar("snd_gain", "1.0", FCVAR_ARCHIVE, "Overall audio gain", false, 0.0f, false, 0.0f);
        registerConVar("snd_gain_max", "1.0", FCVAR_ARCHIVE, "Maximum audio gain", false, 0.0f, false, 0.0f);
        registerConVar("snd_gain_min", "0.01", FCVAR_ARCHIVE, "Minimum audio gain", false, 0.0f, false, 0.0f);
        registerConVar("snd_cull_duplicates", "1", FCVAR_ARCHIVE, "Cull duplicate sound effects emitted in same frame", false, 0.0f, false, 0.0f);
        registerConVar("snd_defer_trace", "1", FCVAR_ARCHIVE, "Defer sound occlusion raycasts to background worker", false, 0.0f, false, 0.0f);
        registerConVar("snd_digital_surround", "0", FCVAR_ARCHIVE, "Enable digital encoded surround sound", false, 0.0f, false, 0.0f);
        registerConVar("dsp_room", "0", FCVAR_ARCHIVE, "Active DSP environmental reverb room preset", false, 0.0f, false, 0.0f);
        registerConVar("dsp_spatial", "1", FCVAR_ARCHIVE, "Calculate binaural 3D spatial pan and HRTF", false, 0.0f, false, 0.0f);
        registerConVar("dsp_slow_cpu", "0", FCVAR_ARCHIVE, "Use simplified low-CPU DSP reverb processing", false, 0.0f, false, 0.0f);
        registerConVar("dsp_vol_2ch", "1.0", FCVAR_ARCHIVE, "DSP volume scaling on stereo channels", false, 0.0f, false, 0.0f);
        registerConVar("dsp_vol_4ch", "1.0", FCVAR_ARCHIVE, "DSP volume scaling on quad channels", false, 0.0f, false, 0.0f);
        registerConVar("dsp_vol_5ch", "1.0", FCVAR_ARCHIVE, "DSP volume scaling on 5.1 surround channels", false, 0.0f, false, 0.0f);
        registerConVar("dsp_automatic", "1", FCVAR_ARCHIVE, "Automatically adjust DSP room presets from map sector brush volume", false, 0.0f, false, 0.0f);
        registerConVar("dsp_facingaway", "0.25", FCVAR_ARCHIVE, "High-frequency attenuation when sound source is behind player", false, 0.0f, false, 0.0f);
        registerConVar("dsp_water", "14", FCVAR_ARCHIVE, "Low-pass filter DSP preset when camera is submerged under water", false, 0.0f, false, 0.0f);
        registerConVar("dsp_enhance_stereo", "1", FCVAR_ARCHIVE, "Enhance stereo separation width", false, 0.0f, false, 0.0f);
        registerConVar("voice_enable", "1", FCVAR_ARCHIVE, "Enable in-game voice microphone input", false, 0.0f, false, 0.0f);
        registerConVar("voice_scale", "1.0", FCVAR_ARCHIVE, "Microphone transmission volume multiplier", false, 0.0f, false, 0.0f);
        registerConVar("voice_loopback", "0", FCVAR_NONE, "Hear your own voice microphone input for testing", false, 0.0f, false, 0.0f);
        registerConVar("voice_modenable", "1", FCVAR_ARCHIVE, "Enable voice chat within mods", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_crowbar", "25", FCVAR_CHEAT | FCVAR_REPLICATED, "Player crowbar/pipe melee damage", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_pistol", "12", FCVAR_CHEAT | FCVAR_REPLICATED, "Player 9mm pistol damage per bullet", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_smg1", "10", FCVAR_CHEAT | FCVAR_REPLICATED, "Player SMG damage per bullet", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_buckshot", "8", FCVAR_CHEAT | FCVAR_REPLICATED, "Player shotgun damage per pellet", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_ar2", "18", FCVAR_CHEAT | FCVAR_REPLICATED, "Player assault rifle damage per bullet", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_crossbow", "100", FCVAR_CHEAT | FCVAR_REPLICATED, "Player crossbow bolt direct hit damage", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_rpg_round", "150", FCVAR_CHEAT | FCVAR_REPLICATED, "Player rocket explosion damage", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_grenade", "125", FCVAR_CHEAT | FCVAR_REPLICATED, "Player hand grenade fragmentation damage", false, 0.0f, false, 0.0f);
        registerConVar("sk_plr_dmg_357", "75", FCVAR_CHEAT | FCVAR_REPLICATED, "Player .357 magnum revolver damage per shot", false, 0.0f, false, 0.0f);
        registerConVar("sk_npc_dmg_crowbar", "15", FCVAR_CHEAT | FCVAR_REPLICATED, "NPC melee strike damage", false, 0.0f, false, 0.0f);
        registerConVar("sk_npc_dmg_pistol", "8", FCVAR_CHEAT | FCVAR_REPLICATED, "NPC pistol damage per bullet", false, 0.0f, false, 0.0f);
        registerConVar("sk_npc_dmg_smg1", "5", FCVAR_CHEAT | FCVAR_REPLICATED, "NPC SMG damage per bullet", false, 0.0f, false, 0.0f);
        registerConVar("sk_npc_dmg_buckshot", "6", FCVAR_CHEAT | FCVAR_REPLICATED, "NPC shotgun damage per pellet", false, 0.0f, false, 0.0f);
        registerConVar("sk_npc_dmg_ar2", "12", FCVAR_CHEAT | FCVAR_REPLICATED, "NPC assault rifle damage per bullet", false, 0.0f, false, 0.0f);
        registerConVar("sk_npc_dmg_357", "50", FCVAR_CHEAT | FCVAR_REPLICATED, "NPC magnum damage per shot", false, 0.0f, false, 0.0f);
        registerConVar("sk_healthcharger", "50", FCVAR_CHEAT | FCVAR_REPLICATED, "Health points dispensed by wall terminal", false, 0.0f, false, 0.0f);
        registerConVar("sk_suitcharger", "75", FCVAR_CHEAT | FCVAR_REPLICATED, "Armor points dispensed by wall charger", false, 0.0f, false, 0.0f);
        registerConVar("sk_battery", "25", FCVAR_CHEAT | FCVAR_REPLICATED, "Armor points provided by floor battery pickup", false, 0.0f, false, 0.0f);
        registerConVar("sk_healthkit", "25", FCVAR_CHEAT | FCVAR_REPLICATED, "Health points provided by medkit pickup", false, 0.0f, false, 0.0f);
        registerConVar("sk_healthvial", "10", FCVAR_CHEAT | FCVAR_REPLICATED, "Health points provided by health vial pickup", false, 0.0f, false, 0.0f);
        registerConVar("sk_auto_reload_time", "2.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Base reload duration in seconds", false, 0.0f, false, 0.0f);
        registerConVar("sk_headshot_multiplier", "2.5", FCVAR_CHEAT | FCVAR_REPLICATED, "Damage multiplier on headshots", false, 0.0f, false, 0.0f);
        registerConVar("sk_limb_multiplier", "0.75", FCVAR_CHEAT | FCVAR_REPLICATED, "Damage multiplier on limb hits", false, 0.0f, false, 0.0f);
        registerConVar("sk_armor_absorption_ratio", "0.70", FCVAR_CHEAT | FCVAR_REPLICATED, "Fraction of damage absorbed by armor before health", false, 0.0f, false, 0.0f);
        registerConVar("host_framerate", "0", FCVAR_CHEAT, "Force fixed simulation time step per frame (0=realtime)", false, 0.0f, false, 0.0f);
        registerConVar("host_timescale", "1.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Engine host loop simulation speed", true, 0.0f, true, 10.0f);
        registerConVar("host_speeds", "0", FCVAR_CHEAT, "Display CPU host tick timing breakdown", false, 0.0f, false, 0.0f);
        registerConVar("host_sleep", "0", FCVAR_NONE, "Force milliseconds of CPU sleep per frame", false, 0.0f, false, 0.0f);
        registerConVar("host_thread_mode", "1", FCVAR_ARCHIVE, "Engine threading model (0=single-thread, 1=multi-threaded)", false, 0.0f, false, 0.0f);
        registerConVar("host_limitlocal", "0", FCVAR_NONE, "Limit loopback network rates on listen server", false, 0.0f, false, 0.0f);
        registerConVar("host_profile", "0", FCVAR_CHEAT, "Profile host tick performance", false, 0.0f, false, 0.0f);
        registerConVar("host_map", "facility_alpha.labmap", FCVAR_NONE, "Active host level", false, 0.0f, false, 0.0f);
        registerConVar("cpu_frequency_monitoring", "0", FCVAR_ARCHIVE, "Monitor CPU core frequency throttling", false, 0.0f, false, 0.0f);
        registerConVar("host_timer_report", "0", FCVAR_NONE, "Print high-resolution timer drift report", false, 0.0f, false, 0.0f);
        registerConVar("host_timer_spin_ms", "1", FCVAR_NONE, "Timer spin threshold in milliseconds", false, 0.0f, false, 0.0f);
        registerConVar("mem_max_heapsize", "1024", FCVAR_ARCHIVE, "Maximum heap size allocated for asset datacache in MB", false, 0.0f, false, 0.0f);
        registerConVar("mem_min_heapsize", "256", FCVAR_ARCHIVE, "Minimum heap size allocated in MB", false, 0.0f, false, 0.0f);
        registerConVar("mem_test", "0", FCVAR_CHEAT, "Test heap memory integrity for buffer overrun corruptions", false, 0.0f, false, 0.0f);
        registerConVar("mem_verify", "0", FCVAR_CHEAT, "Verify consistency of memory block headers", false, 0.0f, false, 0.0f);
        registerConVar("mem_vcollide", "0", FCVAR_NONE, "Print memory allocated to physical collision models", false, 0.0f, false, 0.0f);
        registerConVar("hud_fastswitch", "1", FCVAR_ARCHIVE, "Instantly switch weapons on number key without confirmation", false, 0.0f, false, 0.0f);
        registerConVar("hud_saytext_time", "12", FCVAR_ARCHIVE, "Duration chat messages linger on screen in seconds", false, 0.0f, false, 0.0f);
        registerConVar("hud_deathnotice_time", "6", FCVAR_ARCHIVE, "Duration kill feed death notices linger in seconds", false, 0.0f, false, 0.0f);
        registerConVar("hud_takesshots", "0", FCVAR_ARCHIVE, "Automatically capture screenshot on match victory", false, 0.0f, false, 0.0f);
        registerConVar("hud_showtargetid", "1", FCVAR_ARCHIVE, "Display player name tag when hovering crosshair over character", false, 0.0f, false, 0.0f);
        registerConVar("hud_centerid", "0", FCVAR_ARCHIVE, "Display player target name in screen center rather than lower edge", false, 0.0f, false, 0.0f);
        registerConVar("mod_enable", "1", FCVAR_MOD | FCVAR_ARCHIVE, "Enable custom modder addons system", false, 0.0f, false, 0.0f);
        registerConVar("mod_directory", "addons", FCVAR_MOD | FCVAR_ARCHIVE, "Directory for custom mods and addons", false, 0.0f, false, 0.0f);
        registerConVar("mod_allow_scripts", "1", FCVAR_MOD | FCVAR_ARCHIVE, "Allow mods to execute custom Lua scripts", false, 0.0f, false, 0.0f);
        registerConVar("mod_allow_custom_weapons", "1", FCVAR_MOD | FCVAR_ARCHIVE, "Allow mods to register custom weapon classes", false, 0.0f, false, 0.0f);
        registerConVar("mod_allow_custom_entities", "1", FCVAR_MOD | FCVAR_ARCHIVE, "Allow mods to register custom entity types", false, 0.0f, false, 0.0f);
        registerConVar("mod_author", "", FCVAR_MOD, "Active mod author metadata", false, 0.0f, false, 0.0f);
        registerConVar("mod_version", "1.0.0", FCVAR_MOD, "Active mod semantic version", false, 0.0f, false, 0.0f);
        registerConVar("mod_name", "", FCVAR_MOD, "Active mod friendly title", false, 0.0f, false, 0.0f);
        registerConVar("mod_description", "", FCVAR_MOD, "Active mod description", false, 0.0f, false, 0.0f);
        registerConVar("addon_install", "", FCVAR_MOD, "Mount a custom .lpak or directory addon", false, 0.0f, false, 0.0f);
        registerConVar("addon_uninstall", "", FCVAR_MOD, "Unmount a custom addon", false, 0.0f, false, 0.0f);
        registerConVar("lua_log_sv", "1", FCVAR_MOD | FCVAR_ARCHIVE, "Log Lua server print calls to console", false, 0.0f, false, 0.0f);
        registerConVar("lua_log_cl", "1", FCVAR_MOD | FCVAR_ARCHIVE, "Log Lua client print calls to console", false, 0.0f, false, 0.0f);
        registerConVar("cl_anglespeedkey", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_anglespeedkey", false, 0.0f, false, 0.0f);
        registerConVar("cl_camera_follow_bone", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_camera_follow_bone", false, 0.0f, false, 0.0f);
        registerConVar("cl_c4dynamiclight", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_c4dynamiclight", false, 0.0f, false, 0.0f);
        registerConVar("cl_chatfilters", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_chatfilters", false, 0.0f, false, 0.0f);
        registerConVar("cl_class", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_class", false, 0.0f, false, 0.0f);
        registerConVar("cl_clock_correction", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_clock_correction", false, 0.0f, false, 0.0f);
        registerConVar("cl_clock_correction_adjustment_max_amount", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_clock_correction_adjustment_max_amount", false, 0.0f, false, 0.0f);
        registerConVar("cl_clock_correction_adjustment_min_amount", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_clock_correction_adjustment_min_amount", false, 0.0f, false, 0.0f);
        registerConVar("cl_clock_correction_force_server_tick", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_clock_correction_force_server_tick", false, 0.0f, false, 0.0f);
        registerConVar("cl_clock_resetinterval", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_clock_resetinterval", false, 0.0f, false, 0.0f);
        registerConVar("cl_clockdrift_max_ms", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_clockdrift_max_ms", false, 0.0f, false, 0.0f);
        registerConVar("cl_clockdrift_max_ms_threadmode", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_clockdrift_max_ms_threadmode", false, 0.0f, false, 0.0f);
        registerConVar("cl_cloud_settings", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_cloud_settings", false, 0.0f, false, 0.0f);
        registerConVar("cl_connection_trouble_timeout", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_connection_trouble_timeout", false, 0.0f, false, 0.0f);
        registerConVar("cl_custom_sounds", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_custom_sounds", false, 0.0f, false, 0.0f);
        registerConVar("cl_debug_ugc_downloads", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_debug_ugc_downloads", false, 0.0f, false, 0.0f);
        registerConVar("cl_demoviewoverride", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_demoviewoverride", false, 0.0f, false, 0.0f);
        registerConVar("cl_disable_ragdolls", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_disable_ragdolls", false, 0.0f, false, 0.0f);
        registerConVar("cl_disablehtmlmotd", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_disablehtmlmotd", false, 0.0f, false, 0.0f);
        registerConVar("cl_drawmonitors", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_drawmonitors", false, 0.0f, false, 0.0f);
        registerConVar("cl_drawshadowtexture", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_drawshadowtexture", false, 0.0f, false, 0.0f);
        registerConVar("cl_entityreport", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_entityreport", false, 0.0f, false, 0.0f);
        registerConVar("cl_extrapolate", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_extrapolate", false, 0.0f, false, 0.0f);
        registerConVar("cl_extrapolate_amount", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_extrapolate_amount", false, 0.0f, false, 0.0f);
        registerConVar("cl_fasttempentcollision", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_fasttempentcollision", false, 0.0f, false, 0.0f);
        registerConVar("cl_flushentitypacket", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_flushentitypacket", false, 0.0f, false, 0.0f);
        registerConVar("cl_hud_color", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_hud_color", false, 0.0f, false, 0.0f);
        registerConVar("cl_hud_background_alpha", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_hud_background_alpha", false, 0.0f, false, 0.0f);
        registerConVar("cl_hud_bomb_under_radar", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_hud_bomb_under_radar", false, 0.0f, false, 0.0f);
        registerConVar("cl_hud_healthammo_style", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_hud_healthammo_style", false, 0.0f, false, 0.0f);
        registerConVar("cl_hud_playercount_pos", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_hud_playercount_pos", false, 0.0f, false, 0.0f);
        registerConVar("cl_hud_playercount_showcount", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_hud_playercount_showcount", false, 0.0f, false, 0.0f);
        registerConVar("cl_idealpitchscale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_idealpitchscale", false, 0.0f, false, 0.0f);
        registerConVar("cl_interp_threadmodeticks", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_interp_threadmodeticks", false, 0.0f, false, 0.0f);
        registerConVar("cl_jiggle_bone_debug", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_jiggle_bone_debug", false, 0.0f, false, 0.0f);
        registerConVar("cl_jiggle_bone_debug_pitch_constraints", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_jiggle_bone_debug_pitch_constraints", false, 0.0f, false, 0.0f);
        registerConVar("cl_jiggle_bone_debug_yaw_constraints", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_jiggle_bone_debug_yaw_constraints", false, 0.0f, false, 0.0f);
        registerConVar("cl_jiggle_bone_sanity", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_jiggle_bone_sanity", false, 0.0f, false, 0.0f);
        registerConVar("cl_leveloverview", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_leveloverview", false, 0.0f, false, 0.0f);
        registerConVar("cl_localnetworkbackdoor", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_localnetworkbackdoor", false, 0.0f, false, 0.0f);
        registerConVar("cl_logocolor", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_logocolor", false, 0.0f, false, 0.0f);
        registerConVar("cl_mainmenu_show_gamemodes", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_mainmenu_show_gamemodes", false, 0.0f, false, 0.0f);
        registerConVar("cl_min_ct", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_min_ct", false, 0.0f, false, 0.0f);
        registerConVar("cl_min_t", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_min_t", false, 0.0f, false, 0.0f);
        registerConVar("cl_new_impact_effects", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_new_impact_effects", false, 0.0f, false, 0.0f);
        registerConVar("cl_npc_speedragdoll_multiplier", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_npc_speedragdoll_multiplier", false, 0.0f, false, 0.0f);
        registerConVar("cl_overdraw_test", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_overdraw_test", false, 0.0f, false, 0.0f);
        registerConVar("cl_particle_batch_mode", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_particle_batch_mode", false, 0.0f, false, 0.0f);
        registerConVar("cl_particle_fallback_base", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_particle_fallback_base", false, 0.0f, false, 0.0f);
        registerConVar("cl_particle_fallback_multiplier", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_particle_fallback_multiplier", false, 0.0f, false, 0.0f);
        registerConVar("cl_particle_retire_cost", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_particle_retire_cost", false, 0.0f, false, 0.0f);
        registerConVar("cl_particle_sim_fallback_base_multiplier", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_particle_sim_fallback_base_multiplier", false, 0.0f, false, 0.0f);
        registerConVar("cl_particle_sim_fallback_threshold_ms", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_particle_sim_fallback_threshold_ms", false, 0.0f, false, 0.0f);
        registerConVar("cl_particles_dump_effects", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_particles_dump_effects", false, 0.0f, false, 0.0f);
        registerConVar("cl_pclass", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_pclass", false, 0.0f, false, 0.0f);
        registerConVar("cl_pdump", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_pdump", false, 0.0f, false, 0.0f);
        registerConVar("cl_phys_props_respawndist", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_phys_props_respawndist", false, 0.0f, false, 0.0f);
        registerConVar("cl_phys_props_respawntime", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_phys_props_respawntime", false, 0.0f, false, 0.0f);
        registerConVar("cl_playback_screenshots", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_playback_screenshots", false, 0.0f, false, 0.0f);
        registerConVar("cl_player_death_use_cameraman", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_player_death_use_cameraman", false, 0.0f, false, 0.0f);
        registerConVar("cl_predictioncopy_describe", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_predictioncopy_describe", false, 0.0f, false, 0.0f);
        registerConVar("cl_radar_always_centered", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_radar_always_centered", false, 0.0f, false, 0.0f);
        registerConVar("cl_radar_icon_scale_min", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_radar_icon_scale_min", false, 0.0f, false, 0.0f);
        registerConVar("cl_radar_rotate", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_radar_rotate", false, 0.0f, false, 0.0f);
        registerConVar("cl_radar_scale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_radar_scale", false, 0.0f, false, 0.0f);
        registerConVar("cl_radar_square_with_scoreboard", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_radar_square_with_scoreboard", false, 0.0f, false, 0.0f);
        registerConVar("cl_ragdoll_reload", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_ragdoll_reload", false, 0.0f, false, 0.0f);
        registerConVar("cl_report_soundpatch", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_report_soundpatch", false, 0.0f, false, 0.0f);
        registerConVar("cl_resend_timeout", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_resend_timeout", false, 0.0f, false, 0.0f);
        registerConVar("cl_rumblescale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_rumblescale", false, 0.0f, false, 0.0f);
        registerConVar("cl_sanitize_muted_players", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_sanitize_muted_players", false, 0.0f, false, 0.0f);
        registerConVar("cl_savescreenshot", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_savescreenshot", false, 0.0f, false, 0.0f);
        registerConVar("cl_scene_drawbounds", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_scene_drawbounds", false, 0.0f, false, 0.0f);
        registerConVar("cl_show_clan_in_death_notice", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_show_clan_in_death_notice", false, 0.0f, false, 0.0f);
        registerConVar("cl_show_market_data_on_items", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_show_market_data_on_items", false, 0.0f, false, 0.0f);
        registerConVar("cl_show_num_particles", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_show_num_particles", false, 0.0f, false, 0.0f);
        registerConVar("cl_show_splashes", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_show_splashes", false, 0.0f, false, 0.0f);
        registerConVar("cl_showdemooverlay", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_showdemooverlay", false, 0.0f, false, 0.0f);
        registerConVar("cl_showevents", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_showevents", false, 0.0f, false, 0.0f);
        registerConVar("cl_showhelp", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_showhelp", false, 0.0f, false, 0.0f);
        registerConVar("cl_showloadout", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_showloadout", false, 0.0f, false, 0.0f);
        registerConVar("cl_showpluginmessages", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_showpluginmessages", false, 0.0f, false, 0.0f);
        registerConVar("cl_showsteambillboard", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_showsteambillboard", false, 0.0f, false, 0.0f);
        registerConVar("cl_sim_grenade_trajectory", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_sim_grenade_trajectory", false, 0.0f, false, 0.0f);
        registerConVar("cl_skipslowpath", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_skipslowpath", false, 0.0f, false, 0.0f);
        registerConVar("cl_soundemitter_flush", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_soundemitter_flush", false, 0.0f, false, 0.0f);
        registerConVar("cl_soundfile", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_soundfile", false, 0.0f, false, 0.0f);
        registerConVar("cl_soundscape_flush", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_soundscape_flush", false, 0.0f, false, 0.0f);
        registerConVar("cl_soundscape_printdebuginfo", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_soundscape_printdebuginfo", false, 0.0f, false, 0.0f);
        registerConVar("cl_spec_follow_grenade_key", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_spec_follow_grenade_key", false, 0.0f, false, 0.0f);
        registerConVar("cl_spec_mode", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_spec_mode", false, 0.0f, false, 0.0f);
        registerConVar("cl_spec_show_bindings", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_spec_show_bindings", false, 0.0f, false, 0.0f);
        registerConVar("cl_spec_stats", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_spec_stats", false, 0.0f, false, 0.0f);
        registerConVar("cl_starfield_diameter", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_starfield_diameter", false, 0.0f, false, 0.0f);
        registerConVar("cl_starfield_distance", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_starfield_distance", false, 0.0f, false, 0.0f);
        registerConVar("cl_sun_disable", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_sun_disable", false, 0.0f, false, 0.0f);
        registerConVar("cl_team", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_team", false, 0.0f, false, 0.0f);
        registerConVar("cl_teammate_colors_show", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_teammate_colors_show", false, 0.0f, false, 0.0f);
        registerConVar("cl_threaded_init", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_threaded_init", false, 0.0f, false, 0.0f);
        registerConVar("cl_use_new_headbob", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_use_new_headbob", false, 0.0f, false, 0.0f);
        registerConVar("cl_view", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_view", false, 0.0f, false, 0.0f);
        registerConVar("cl_viewmodel_shift_left_amt", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_viewmodel_shift_left_amt", false, 0.0f, false, 0.0f);
        registerConVar("cl_viewmodel_shift_right_amt", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_viewmodel_shift_right_amt", false, 0.0f, false, 0.0f);
        registerConVar("cl_vote_modifier", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_vote_modifier", false, 0.0f, false, 0.0f);
        registerConVar("cl_weapon_reload_cancel", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_weapon_reload_cancel", false, 0.0f, false, 0.0f);
        registerConVar("cl_winddir", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_winddir", false, 0.0f, false, 0.0f);
        registerConVar("cl_windspeed", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Client subsystem control variable cl_windspeed", false, 0.0f, false, 0.0f);
        registerConVar("r_ambientboost", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_ambientboost", false, 0.0f, false, 0.0f);
        registerConVar("r_cheapwaterend", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_cheapwaterend", false, 0.0f, false, 0.0f);
        registerConVar("r_cheapwaterstart", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_cheapwaterstart", false, 0.0f, false, 0.0f);
        registerConVar("r_colorstaticprops", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_colorstaticprops", false, 0.0f, false, 0.0f);
        registerConVar("r_decal_cover", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_decal_cover", false, 0.0f, false, 0.0f);
        registerConVar("r_decal_overlap_area", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_decal_overlap_area", false, 0.0f, false, 0.0f);
        registerConVar("r_decal_overlap_count", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_decal_overlap_count", false, 0.0f, false, 0.0f);
        registerConVar("r_decalstaticprops", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_decalstaticprops", false, 0.0f, false, 0.0f);
        registerConVar("r_doptdisptris", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_doptdisptris", false, 0.0f, false, 0.0f);
        registerConVar("r_drawbatchdecals", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawbatchdecals", false, 0.0f, false, 0.0f);
        registerConVar("r_drawfunnel", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawfunnel", false, 0.0f, false, 0.0f);
        registerConVar("r_drawhair", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawhair", false, 0.0f, false, 0.0f);
        registerConVar("r_drawleaf", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawleaf", false, 0.0f, false, 0.0f);
        registerConVar("r_drawmodelstatsoverlay", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawmodelstatsoverlay", false, 0.0f, false, 0.0f);
        registerConVar("r_drawopaquerenderables", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawopaquerenderables", false, 0.0f, false, 0.0f);
        registerConVar("r_drawopaquestaticpropslast", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawopaquestaticpropslast", false, 0.0f, false, 0.0f);
        registerConVar("r_drawrenderers", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawrenderers", false, 0.0f, false, 0.0f);
        registerConVar("r_drawvgui", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_drawvgui", false, 0.0f, false, 0.0f);
        registerConVar("r_dlightamt", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_dlightamt", false, 0.0f, false, 0.0f);
        registerConVar("r_dynamiclighting", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_dynamiclighting", false, 0.0f, false, 0.0f);
        registerConVar("r_eyes", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_eyes", false, 0.0f, false, 0.0f);
        registerConVar("r_eyewaterepsilon", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_eyewaterepsilon", false, 0.0f, false, 0.0f);
        registerConVar("r_fastzrejectdisp", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_fastzrejectdisp", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightambient", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightambient", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightclip", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightclip", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightconstant", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightconstant", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightculldepth", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightculldepth", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightdepthtexture", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightdepthtexture", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightdrawfrustum", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightdrawfrustum", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightdrawsweptbbox", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightdrawsweptbbox", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightlinear", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightlinear", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightlockposition", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightlockposition", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightnear", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightnear", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightnodraw", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightnodraw", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightoffsetx", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightoffsetx", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightoffsety", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightoffsety", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightoffsetz", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightoffsetz", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightquadratic", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightquadratic", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightrender", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightrender", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightrendermodels", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightrendermodels", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightrendersworld", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightrendersworld", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightscissor", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightscissor", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightshadowatten", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightshadowatten", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlighttracedepth", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlighttracedepth", false, 0.0f, false, 0.0f);
        registerConVar("r_flashlightupdatedepth", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flashlightupdatedepth", false, 0.0f, false, 0.0f);
        registerConVar("r_flex", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flex", false, 0.0f, false, 0.0f);
        registerConVar("r_flushlod", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_flushlod", false, 0.0f, false, 0.0f);
        registerConVar("r_forcewaterleaf", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_forcewaterleaf", false, 0.0f, false, 0.0f);
        registerConVar("r_glint_always", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_glint_always", false, 0.0f, false, 0.0f);
        registerConVar("r_glint_procedural", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_glint_procedural", false, 0.0f, false, 0.0f);
        registerConVar("r_hunkalloclightmaps", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_hunkalloclightmaps", false, 0.0f, false, 0.0f);
        registerConVar("r_itemfx", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_itemfx", false, 0.0f, false, 0.0f);
        registerConVar("r_lightinterp", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_lightinterp", false, 0.0f, false, 0.0f);
        registerConVar("r_maxnewsamples", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_maxnewsamples", false, 0.0f, false, 0.0f);
        registerConVar("r_maxsampledist", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_maxsampledist", false, 0.0f, false, 0.0f);
        registerConVar("r_minnewsamples", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_minnewsamples", false, 0.0f, false, 0.0f);
        registerConVar("r_modelwireframedecal", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_modelwireframedecal", false, 0.0f, false, 0.0f);
        registerConVar("r_newflashlight", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_newflashlight", false, 0.0f, false, 0.0f);
        registerConVar("r_nohw", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_nohw", false, 0.0f, false, 0.0f);
        registerConVar("r_norefresh", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_norefresh", false, 0.0f, false, 0.0f);
        registerConVar("r_nosurf", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_nosurf", false, 0.0f, false, 0.0f);
        registerConVar("r_occlusionspew", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_occlusionspew", false, 0.0f, false, 0.0f);
        registerConVar("r_oldlightselection", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_oldlightselection", false, 0.0f, false, 0.0f);
        registerConVar("r_partition_level", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_partition_level", false, 0.0f, false, 0.0f);
        registerConVar("r_pixelfog", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_pixelfog", false, 0.0f, false, 0.0f);
        registerConVar("r_pixelvisibility_spew", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_pixelvisibility_spew", false, 0.0f, false, 0.0f);
        registerConVar("r_portalsopenall", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_portalsopenall", false, 0.0f, false, 0.0f);
        registerConVar("r_proplightingfromdisk", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_proplightingfromdisk", false, 0.0f, false, 0.0f);
        registerConVar("r_radiosity", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_radiosity", false, 0.0f, false, 0.0f);
        registerConVar("r_rainsidevel", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_rainsidevel", false, 0.0f, false, 0.0f);
        registerConVar("r_rainsimulate", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_rainsimulate", false, 0.0f, false, 0.0f);
        registerConVar("r_rainsplashpercentage", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_rainsplashpercentage", false, 0.0f, false, 0.0f);
        registerConVar("r_rimlight", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_rimlight", false, 0.0f, false, 0.0f);
        registerConVar("r_sequence_debug", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_sequence_debug", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowangles", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowangles", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowblobbycutoff", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowblobbycutoff", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowdir", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowdir", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowdrawdepthcamera", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowdrawdepthcamera", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowfromworldlights", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowfromworldlights", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowfromworldlights_debug", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowfromworldlights_debug", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowids", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowids", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowlist", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowlist", false, 0.0f, false, 0.0f);
        registerConVar("r_shadows_gamecontrol", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadows_gamecontrol", false, 0.0f, false, 0.0f);
        registerConVar("r_shadowwireframe", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_shadowwireframe", false, 0.0f, false, 0.0f);
        registerConVar("r_showenvcubemap", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_showenvcubemap", false, 0.0f, false, 0.0f);
        registerConVar("r_skybox_cubemap", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_skybox_cubemap", false, 0.0f, false, 0.0f);
        registerConVar("r_skybox_fog_factor", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_skybox_fog_factor", false, 0.0f, false, 0.0f);
        registerConVar("r_snappool", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_snappool", false, 0.0f, false, 0.0f);
        registerConVar("r_spewleaf", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_spewleaf", false, 0.0f, false, 0.0f);
        registerConVar("r_staticpropinfo", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_staticpropinfo", false, 0.0f, false, 0.0f);
        registerConVar("r_unloadlightmaps", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_unloadlightmaps", false, 0.0f, false, 0.0f);
        registerConVar("r_unref_lightmaps", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_unref_lightmaps", false, 0.0f, false, 0.0f);
        registerConVar("r_vehicleBrakeCycle", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_vehicleBrakeCycle", false, 0.0f, false, 0.0f);
        registerConVar("r_visocclusion", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_visocclusion", false, 0.0f, false, 0.0f);
        registerConVar("r_visualizelightcache", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_visualizelightcache", false, 0.0f, false, 0.0f);
        registerConVar("r_visualizeproplightcaching", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_visualizeproplightcaching", false, 0.0f, false, 0.0f);
        registerConVar("r_visualizetraces", "1", FCVAR_ARCHIVE, "Renderer subsystem visual parameter r_visualizetraces", false, 0.0f, false, 0.0f);
        registerConVar("mat_ambient_light_b", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_ambient_light_b", false, 0.0f, false, 0.0f);
        registerConVar("mat_ambient_light_g", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_ambient_light_g", false, 0.0f, false, 0.0f);
        registerConVar("mat_ambient_light_r", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_ambient_light_r", false, 0.0f, false, 0.0f);
        registerConVar("mat_bumpbasis", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_bumpbasis", false, 0.0f, false, 0.0f);
        registerConVar("mat_cameraman_postprocess", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_cameraman_postprocess", false, 0.0f, false, 0.0f);
        registerConVar("mat_colcorrection_editor", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_colcorrection_editor", false, 0.0f, false, 0.0f);
        registerConVar("mat_configcurrent", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_configcurrent", false, 0.0f, false, 0.0f);
        registerConVar("mat_crosshair", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_crosshair", false, 0.0f, false, 0.0f);
        registerConVar("mat_crosshair_edit", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_crosshair_edit", false, 0.0f, false, 0.0f);
        registerConVar("mat_crosshair_explorer", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_crosshair_explorer", false, 0.0f, false, 0.0f);
        registerConVar("mat_crosshair_reloadmaterial", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_crosshair_reloadmaterial", false, 0.0f, false, 0.0f);
        registerConVar("mat_debug_postprocessing_effects", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_debug_postprocessing_effects", false, 0.0f, false, 0.0f);
        registerConVar("mat_debug_process_halfscreen", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_debug_process_halfscreen", false, 0.0f, false, 0.0f);
        registerConVar("mat_debugdepthmode", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_debugdepthmode", false, 0.0f, false, 0.0f);
        registerConVar("mat_debugdepthval", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_debugdepthval", false, 0.0f, false, 0.0f);
        registerConVar("mat_debugdepthvalmax", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_debugdepthvalmax", false, 0.0f, false, 0.0f);
        registerConVar("mat_depthbias_edge_clamp", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_depthbias_edge_clamp", false, 0.0f, false, 0.0f);
        registerConVar("mat_depthbias_shadow_map", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_depthbias_shadow_map", false, 0.0f, false, 0.0f);
        registerConVar("mat_depthfeather_enable", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_depthfeather_enable", false, 0.0f, false, 0.0f);
        registerConVar("mat_disable_d3d9ex", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_disable_d3d9ex", false, 0.0f, false, 0.0f);
        registerConVar("mat_disable_hwmorph", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_disable_hwmorph", false, 0.0f, false, 0.0f);
        registerConVar("mat_disable_lightcache", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_disable_lightcache", false, 0.0f, false, 0.0f);
        registerConVar("mat_disable_ps_patch", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_disable_ps_patch", false, 0.0f, false, 0.0f);
        registerConVar("mat_displacementmap", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_displacementmap", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_blur_scale", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_blur_scale", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_enabled", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_enabled", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_far_blur_depth", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_far_blur_depth", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_far_blur_radius", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_far_blur_radius", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_far_focus_depth", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_far_focus_depth", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_max_blur_radius", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_max_blur_radius", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_near_blur_depth", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_near_blur_depth", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_near_blur_radius", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_near_blur_radius", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_near_focus_depth", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_near_focus_depth", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_override", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_override", false, 0.0f, false, 0.0f);
        registerConVar("mat_dof_quality", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_dof_quality", false, 0.0f, false, 0.0f);
        registerConVar("mat_drawmaterial", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_drawmaterial", false, 0.0f, false, 0.0f);
        registerConVar("mat_drawmaterials", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_drawmaterials", false, 0.0f, false, 0.0f);
        registerConVar("mat_edit", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_edit", false, 0.0f, false, 0.0f);
        registerConVar("mat_envmaptgasize", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_envmaptgasize", false, 0.0f, false, 0.0f);
        registerConVar("mat_force_bloom", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_force_bloom", false, 0.0f, false, 0.0f);
        registerConVar("mat_force_ps_patch", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_force_ps_patch", false, 0.0f, false, 0.0f);
        registerConVar("mat_force_tonemap_min_avglum", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_force_tonemap_min_avglum", false, 0.0f, false, 0.0f);
        registerConVar("mat_frame_sync_force_texture", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_frame_sync_force_texture", false, 0.0f, false, 0.0f);
        registerConVar("mat_framebuffercopyoverhead", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_framebuffercopyoverhead", false, 0.0f, false, 0.0f);
        registerConVar("mat_hdr_enabled", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_hdr_enabled", false, 0.0f, false, 0.0f);
        registerConVar("mat_hdr_manual_tonemap_rate", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_hdr_manual_tonemap_rate", false, 0.0f, false, 0.0f);
        registerConVar("mat_hdr_uncachedvolume", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_hdr_uncachedvolume", false, 0.0f, false, 0.0f);
        registerConVar("mat_hsv", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_hsv", false, 0.0f, false, 0.0f);
        registerConVar("mat_lpreview_mode", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_lpreview_mode", false, 0.0f, false, 0.0f);
        registerConVar("mat_maskstatus", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_maskstatus", false, 0.0f, false, 0.0f);
        registerConVar("mat_monitorgamma_tv_exp", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_monitorgamma_tv_exp", false, 0.0f, false, 0.0f);
        registerConVar("mat_monitorgamma_tv_range_max", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_monitorgamma_tv_range_max", false, 0.0f, false, 0.0f);
        registerConVar("mat_monitorgamma_tv_range_min", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_monitorgamma_tv_range_min", false, 0.0f, false, 0.0f);
        registerConVar("mat_morph_recompute_normals", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_morph_recompute_normals", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_falling_intensity", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_motion_blur_falling_intensity", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_falling_max", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_motion_blur_falling_max", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_falling_min", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_motion_blur_falling_min", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_forward_enabled", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_motion_blur_forward_enabled", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_percent_of_screen_max", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_motion_blur_percent_of_screen_max", false, 0.0f, false, 0.0f);
        registerConVar("mat_motion_blur_rotation_intensity", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_motion_blur_rotation_intensity", false, 0.0f, false, 0.0f);
        registerConVar("mat_non_hdr_bloombuzz_off", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_non_hdr_bloombuzz_off", false, 0.0f, false, 0.0f);
        registerConVar("mat_postprocess_x", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_postprocess_x", false, 0.0f, false, 0.0f);
        registerConVar("mat_postprocess_y", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_postprocess_y", false, 0.0f, false, 0.0f);
        registerConVar("mat_postprocessing_combine", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_postprocessing_combine", false, 0.0f, false, 0.0f);
        registerConVar("mat_proxy", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_proxy", false, 0.0f, false, 0.0f);
        registerConVar("mat_queue_priority", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_queue_priority", false, 0.0f, false, 0.0f);
        registerConVar("mat_queue_report", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_queue_report", false, 0.0f, false, 0.0f);
        registerConVar("mat_reloadmaterial", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_reloadmaterial", false, 0.0f, false, 0.0f);
        registerConVar("mat_remoteshadercompile", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_remoteshadercompile", false, 0.0f, false, 0.0f);
        registerConVar("mat_report_queue_status", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_report_queue_status", false, 0.0f, false, 0.0f);
        registerConVar("mat_show_ab_hdr", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_show_ab_hdr", false, 0.0f, false, 0.0f);
        registerConVar("mat_showmaterialsverbose", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_showmaterialsverbose", false, 0.0f, false, 0.0f);
        registerConVar("mat_showwatertextures", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_showwatertextures", false, 0.0f, false, 0.0f);
        registerConVar("mat_slopescaledepthbias_decal", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_slopescaledepthbias_decal", false, 0.0f, false, 0.0f);
        registerConVar("mat_slopescaledepthbias_normal", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_slopescaledepthbias_normal", false, 0.0f, false, 0.0f);
        registerConVar("mat_slopescaledepthbias_shadow_map", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_slopescaledepthbias_shadow_map", false, 0.0f, false, 0.0f);
        registerConVar("mat_software_aa_debug", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_software_aa_debug", false, 0.0f, false, 0.0f);
        registerConVar("mat_software_aa_strength_vgui", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_software_aa_strength_vgui", false, 0.0f, false, 0.0f);
        registerConVar("mat_spewalloc", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_spewalloc", false, 0.0f, false, 0.0f);
        registerConVar("mat_stub", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_stub", false, 0.0f, false, 0.0f);
        registerConVar("mat_surfacemat", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_surfacemat", false, 0.0f, false, 0.0f);
        registerConVar("mat_tessellation_acc_ins_threshold", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_tessellation_acc_ins_threshold", false, 0.0f, false, 0.0f);
        registerConVar("mat_tessellation_cornertesting", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_tessellation_cornertesting", false, 0.0f, false, 0.0f);
        registerConVar("mat_tessellation_linespace", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_tessellation_linespace", false, 0.0f, false, 0.0f);
        registerConVar("mat_tessellation_update_buffers", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_tessellation_update_buffers", false, 0.0f, false, 0.0f);
        registerConVar("mat_texture_list", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_texture_list", false, 0.0f, false, 0.0f);
        registerConVar("mat_texture_list_all", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_texture_list_all", false, 0.0f, false, 0.0f);
        registerConVar("mat_texture_list_content_path", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_texture_list_content_path", false, 0.0f, false, 0.0f);
        registerConVar("mat_texture_outline_fonts", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_texture_outline_fonts", false, 0.0f, false, 0.0f);
        registerConVar("mat_tonemap_min_avglum", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_tonemap_min_avglum", false, 0.0f, false, 0.0f);
        registerConVar("mat_use_compressed_hdr_textures", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_use_compressed_hdr_textures", false, 0.0f, false, 0.0f);
        registerConVar("mat_vsync", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_vsync", false, 0.0f, false, 0.0f);
        registerConVar("mat_yuv", "1", FCVAR_ARCHIVE, "Material system shader parameter mat_yuv", false, 0.0f, false, 0.0f);
        registerConVar("net_blockmsg", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_blockmsg", false, 0.0f, false, 0.0f);
        registerConVar("net_channel", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_channel", false, 0.0f, false, 0.0f);
        registerConVar("net_chokeloop", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_chokeloop", false, 0.0f, false, 0.0f);
        registerConVar("net_compresspackets", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_compresspackets", false, 0.0f, false, 0.0f);
        registerConVar("net_compresspackets_minsize", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_compresspackets_minsize", false, 0.0f, false, 0.0f);
        registerConVar("net_compressvoice", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_compressvoice", false, 0.0f, false, 0.0f);
        registerConVar("net_drawslider", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_drawslider", false, 0.0f, false, 0.0f);
        registerConVar("net_droppackets", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_droppackets", false, 0.0f, false, 0.0f);
        registerConVar("net_fakelag", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_fakelag", false, 0.0f, false, 0.0f);
        registerConVar("net_fakeloss", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_fakeloss", false, 0.0f, false, 0.0f);
        registerConVar("net_graphholdsvframerate", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphholdsvframerate", false, 0.0f, false, 0.0f);
        registerConVar("net_graphipc", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphipc", false, 0.0f, false, 0.0f);
        registerConVar("net_graphmsecs", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphmsecs", false, 0.0f, false, 0.0f);
        registerConVar("net_graphproportionalfont", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphproportionalfont", false, 0.0f, false, 0.0f);
        registerConVar("net_graphshowinterp", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphshowinterp", false, 0.0f, false, 0.0f);
        registerConVar("net_graphshowlatency", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphshowlatency", false, 0.0f, false, 0.0f);
        registerConVar("net_graphshowsvframerate", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphshowsvframerate", false, 0.0f, false, 0.0f);
        registerConVar("net_graphtext", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_graphtext", false, 0.0f, false, 0.0f);
        registerConVar("net_maxcleartime", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_maxcleartime", false, 0.0f, false, 0.0f);
        registerConVar("net_maxfilesize", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_maxfilesize", false, 0.0f, false, 0.0f);
        registerConVar("net_maxfragments", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_maxfragments", false, 0.0f, false, 0.0f);
        registerConVar("net_maxpacketdrop", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_maxpacketdrop", false, 0.0f, false, 0.0f);
        registerConVar("net_maxroutable", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_maxroutable", false, 0.0f, false, 0.0f);
        registerConVar("net_paranoid", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_paranoid", false, 0.0f, false, 0.0f);
        registerConVar("net_public_adr", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_public_adr", false, 0.0f, false, 0.0f);
        registerConVar("net_queue_trace", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_queue_trace", false, 0.0f, false, 0.0f);
        registerConVar("net_queued_packet_thread", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_queued_packet_thread", false, 0.0f, false, 0.0f);
        registerConVar("net_scale", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_scale", false, 0.0f, false, 0.0f);
        registerConVar("net_showdrop", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showdrop", false, 0.0f, false, 0.0f);
        registerConVar("net_showevents", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showevents", false, 0.0f, false, 0.0f);
        registerConVar("net_showfragments", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showfragments", false, 0.0f, false, 0.0f);
        registerConVar("net_showmsg", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showmsg", false, 0.0f, false, 0.0f);
        registerConVar("net_showpeaks", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showpeaks", false, 0.0f, false, 0.0f);
        registerConVar("net_showsplits", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showsplits", false, 0.0f, false, 0.0f);
        registerConVar("net_showtcp", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showtcp", false, 0.0f, false, 0.0f);
        registerConVar("net_showudp", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showudp", false, 0.0f, false, 0.0f);
        registerConVar("net_showudp_oob", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_showudp_oob", false, 0.0f, false, 0.0f);
        registerConVar("net_splitpacket_maxrate", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_splitpacket_maxrate", false, 0.0f, false, 0.0f);
        registerConVar("net_splitrate", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_splitrate", false, 0.0f, false, 0.0f);
        registerConVar("net_steamcnx_allowrelay", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_steamcnx_allowrelay", false, 0.0f, false, 0.0f);
        registerConVar("net_steamcnx_debug", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_steamcnx_debug", false, 0.0f, false, 0.0f);
        registerConVar("net_steamcnx_enabled", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_steamcnx_enabled", false, 0.0f, false, 0.0f);
        registerConVar("net_udp_rcvbuf", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_udp_rcvbuf", false, 0.0f, false, 0.0f);
        registerConVar("net_validateticks", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_validateticks", false, 0.0f, false, 0.0f);
        registerConVar("net_warning", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_warning", false, 0.0f, false, 0.0f);
        registerConVar("net_ws_latency", "0", FCVAR_ARCHIVE, "Network protocol and socket diagnostic net_ws_latency", false, 0.0f, false, 0.0f);
        registerConVar("dsp_db_min", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_db_min", false, 0.0f, false, 0.0f);
        registerConVar("dsp_db_mixdrop", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_db_mixdrop", false, 0.0f, false, 0.0f);
        registerConVar("dsp_dist_max", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_dist_max", false, 0.0f, false, 0.0f);
        registerConVar("dsp_dist_min", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_dist_min", false, 0.0f, false, 0.0f);
        registerConVar("dsp_dly_limit", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_dly_limit", false, 0.0f, false, 0.0f);
        registerConVar("dsp_mix_max", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_mix_max", false, 0.0f, false, 0.0f);
        registerConVar("dsp_mix_min", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_mix_min", false, 0.0f, false, 0.0f);
        registerConVar("dsp_off", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_off", false, 0.0f, false, 0.0f);
        registerConVar("dsp_player", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_player", false, 0.0f, false, 0.0f);
        registerConVar("dsp_reload", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_reload", false, 0.0f, false, 0.0f);
        registerConVar("dsp_speaker", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_speaker", false, 0.0f, false, 0.0f);
        registerConVar("dsp_volume", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter dsp_volume", false, 0.0f, false, 0.0f);
        registerConVar("snd_async_fullyasync", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_async_fullyasync", false, 0.0f, false, 0.0f);
        registerConVar("snd_async_minsize", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_async_minsize", false, 0.0f, false, 0.0f);
        registerConVar("snd_async_spew_flag", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_async_spew_flag", false, 0.0f, false, 0.0f);
        registerConVar("snd_delay_sound_shift", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_delay_sound_shift", false, 0.0f, false, 0.0f);
        registerConVar("snd_disable_mixer_duck", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_disable_mixer_duck", false, 0.0f, false, 0.0f);
        registerConVar("snd_duckerattacktime", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_duckerattacktime", false, 0.0f, false, 0.0f);
        registerConVar("snd_duckerdamping", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_duckerdamping", false, 0.0f, false, 0.0f);
        registerConVar("snd_duckerreleasetime", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_duckerreleasetime", false, 0.0f, false, 0.0f);
        registerConVar("snd_duckerthreshold", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_duckerthreshold", false, 0.0f, false, 0.0f);
        registerConVar("snd_ducktovolume", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_ducktovolume", false, 0.0f, false, 0.0f);
        registerConVar("snd_dump_filepaths", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_dump_filepaths", false, 0.0f, false, 0.0f);
        registerConVar("snd_legacy_surround", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_legacy_surround", false, 0.0f, false, 0.0f);
        registerConVar("snd_lockpartial", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_lockpartial", false, 0.0f, false, 0.0f);
        registerConVar("snd_memasync", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_memasync", false, 0.0f, false, 0.0f);
        registerConVar("snd_mix_async", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_mix_async", false, 0.0f, false, 0.0f);
        registerConVar("snd_noextraupdate", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_noextraupdate", false, 0.0f, false, 0.0f);
        registerConVar("snd_obscured_gain_dB", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_obscured_gain_dB", false, 0.0f, false, 0.0f);
        registerConVar("snd_rebuildaudiocache", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_rebuildaudiocache", false, 0.0f, false, 0.0f);
        registerConVar("snd_refdb", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_refdb", false, 0.0f, false, 0.0f);
        registerConVar("snd_refdist", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_refdist", false, 0.0f, false, 0.0f);
        registerConVar("snd_report_looping_sounds", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_report_looping_sounds", false, 0.0f, false, 0.0f);
        registerConVar("snd_report_verbose_error", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_report_verbose_error", false, 0.0f, false, 0.0f);
        registerConVar("snd_showclassname", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_showclassname", false, 0.0f, false, 0.0f);
        registerConVar("snd_showmixer", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_showmixer", false, 0.0f, false, 0.0f);
        registerConVar("snd_soundmixer", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_soundmixer", false, 0.0f, false, 0.0f);
        registerConVar("snd_spatialize_roundrobin", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_spatialize_roundrobin", false, 0.0f, false, 0.0f);
        registerConVar("snd_updateaudiocache", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_updateaudiocache", false, 0.0f, false, 0.0f);
        registerConVar("snd_vox_captiontrace", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_vox_captiontrace", false, 0.0f, false, 0.0f);
        registerConVar("snd_vox_kill", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_vox_kill", false, 0.0f, false, 0.0f);
        registerConVar("snd_vox_seqdelay", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_vox_seqdelay", false, 0.0f, false, 0.0f);
        registerConVar("snd_vox_volume", "1", FCVAR_ARCHIVE, "Audio processing and DSP parameter snd_vox_volume", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawfocus", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawfocus", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawhud", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawhud", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_bounds", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_bounds", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_clear", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_clear", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_draw_selected", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_draw_selected", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_freeze", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_freeze", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_hidden", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_hidden", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_panelalpha", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_panelalpha", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_panelptr", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_panelptr", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_popupsonly", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_popupsonly", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_render_order", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_render_order", false, 0.0f, false, 0.0f);
        registerConVar("vgui_drawtree_visible", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_drawtree_visible", false, 0.0f, false, 0.0f);
        registerConVar("vgui_message_dialog_modal", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_message_dialog_modal", false, 0.0f, false, 0.0f);
        registerConVar("vgui_spew_fonts", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_spew_fonts", false, 0.0f, false, 0.0f);
        registerConVar("vgui_toggle_scheme_editor", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter vgui_toggle_scheme_editor", false, 0.0f, false, 0.0f);
        registerConVar("hud_drawhistory_time", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter hud_drawhistory_time", false, 0.0f, false, 0.0f);
        registerConVar("hud_scaling", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter hud_scaling", false, 0.0f, false, 0.0f);
        registerConVar("hud_subtitles", "0", FCVAR_ARCHIVE, "VGUI interface and HUD display parameter hud_subtitles", false, 0.0f, false, 0.0f);
        registerConVar("fov", "90", FCVAR_ARCHIVE, "Field of view angle in degrees", true, 60.0f, true, 130.0f);
        registerConVar("default_fov", "90", FCVAR_ARCHIVE, "Default field of view angle", true, 60.0f, true, 130.0f);
        registerConVar("fov_desired", "90", FCVAR_ARCHIVE, "Desired field of view angle in degrees", true, 60.0f, true, 130.0f);
        registerConVar("viewmodel_fov", "54", FCVAR_ARCHIVE, "Weapon viewmodel field of view angle", true, 40.0f, true, 90.0f);
        registerConVar("sv_gravity", "800", FCVAR_NOTIFY | FCVAR_REPLICATED, "World gravity acceleration in units/s^2", true, 0.0f, true, 2000.0f);
        registerConVar("sv_lan", "1", FCVAR_REPLICATED, "Server is a local area network game (no internet heartbeat)", true, 0.0f, true, 1.0f);
        registerConVar("maxplayers", "16", FCVAR_REPLICATED, "Maximum concurrent players supported on the server", true, 1.0f, true, 64.0f);
    }

} // namespace Lab
