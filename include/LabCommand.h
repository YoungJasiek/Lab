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
        CommandHistory(size_t maxHistory = 100) : _maxHistory(maxHistory) {}

        void executeCommand(std::unique_ptr<ICommand> command) {
            if (!command) return;

            command->execute();

            // Executing new command truncates redo stack
            _redoStack.clear();

            _undoStack.push_back(std::move(command));
            if (_undoStack.size() > _maxHistory) {
                _undoStack.erase(_undoStack.begin());
            }
        }

        bool undo() {
            if (_undoStack.empty()) return false;

            auto cmd = std::move(_undoStack.back());
            _undoStack.pop_back();

            cmd->undo();
            _redoStack.push_back(std::move(cmd));
            return true;
        }

        bool redo() {
            if (_redoStack.empty()) return false;

            auto cmd = std::move(_redoStack.back());
            _redoStack.pop_back();

            cmd->execute();
            _undoStack.push_back(std::move(cmd));
            return true;
        }

        bool canUndo() const { return !_undoStack.empty(); }
        bool canRedo() const { return !_redoStack.empty(); }

        size_t getUndoCount() const { return _undoStack.size(); }
        size_t getRedoCount() const { return _redoStack.size(); }

        std::string getUndoCommandName() const {
            return _undoStack.empty() ? "" : _undoStack.back()->getName();
        }

        std::string getRedoCommandName() const {
            return _redoStack.empty() ? "" : _redoStack.back()->getName();
        }

        void clear() {
            _undoStack.clear();
            _redoStack.clear();
        }

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
        ActionCommand(const std::string& name, std::function<void()> onExecute, std::function<void()> onUndo)
            : _name(name), _onExecute(onExecute), _onUndo(onUndo) {}

        void execute() override { if (_onExecute) _onExecute(); }
        void undo() override { if (_onUndo) _onUndo(); }
        std::string getName() const override { return _name; }

    private:
        std::string _name;
        std::function<void()> _onExecute;
        std::function<void()> _onUndo;
    };

} // namespace Lab
