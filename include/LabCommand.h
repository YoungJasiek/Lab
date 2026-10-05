#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

namespace Lab {

    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void execute() = 0;
        virtual void undo() = 0;
        virtual std::string getName() const = 0;
    };

    class CommandHistory {
    public:
        CommandHistory(size_t maxHistory = 100);

        void executeCommand(std::unique_ptr<ICommand> command);
        bool undo();
        bool redo();

        bool canUndo() const { return !_undoStack.empty(); }
        bool canRedo() const { return !_redoStack.empty(); }

        size_t getUndoCount() const { return _undoStack.size(); }
        size_t getRedoCount() const { return _redoStack.size(); }

        std::string getUndoCommandName() const;
        std::string getRedoCommandName() const;

        void clear();

        void setMaxHistory(size_t maxHistory) { _maxHistory = maxHistory; }
        size_t getMaxHistory() const { return _maxHistory; }

    private:
        size_t _maxHistory;
        std::vector<std::unique_ptr<ICommand>> _undoStack;
        std::vector<std::unique_ptr<ICommand>> _redoStack;
    };

    // Generic Lambda / Action Command
    class ActionCommand : public ICommand {
    public:
        ActionCommand(const std::string& name, std::function<void()> onExecute, std::function<void()> onUndo);

        void execute() override;
        void undo() override;
        std::string getName() const override { return _name; }

    private:
        std::string _name;
        std::function<void()> _onExecute;
        std::function<void()> _onUndo;
    };

} // namespace Lab
