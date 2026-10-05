#include "LabCommand.h"

namespace Lab {

    CommandHistory::CommandHistory(size_t maxHistory) : _maxHistory(maxHistory) {}

    void CommandHistory::executeCommand(std::unique_ptr<ICommand> command) {
        if (!command) return;

        command->execute();

        // Executing new command truncates redo stack
        _redoStack.clear();

        _undoStack.push_back(std::move(command));
        if (_undoStack.size() > _maxHistory) {
            _undoStack.erase(_undoStack.begin());
        }
    }

    bool CommandHistory::undo() {
        if (_undoStack.empty()) return false;

        auto cmd = std::move(_undoStack.back());
        _undoStack.pop_back();

        cmd->undo();
        _redoStack.push_back(std::move(cmd));
        return true;
    }

    bool CommandHistory::redo() {
        if (_redoStack.empty()) return false;

        auto cmd = std::move(_redoStack.back());
        _redoStack.pop_back();

        cmd->execute();
        _undoStack.push_back(std::move(cmd));
        return true;
    }

    std::string CommandHistory::getUndoCommandName() const {
        return _undoStack.empty() ? "" : _undoStack.back()->getName();
    }

    std::string CommandHistory::getRedoCommandName() const {
        return _redoStack.empty() ? "" : _redoStack.back()->getName();
    }

    void CommandHistory::clear() {
        _undoStack.clear();
        _redoStack.clear();
    }

    ActionCommand::ActionCommand(const std::string& name, std::function<void()> onExecute, std::function<void()> onUndo)
        : _name(name), _onExecute(std::move(onExecute)), _onUndo(std::move(onUndo)) {}

    void ActionCommand::execute() {
        if (_onExecute) _onExecute();
    }

    void ActionCommand::undo() {
        if (_onUndo) _onUndo();
    }

} // namespace Lab
