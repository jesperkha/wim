#pragma once

#include <windows.h>

#define curBuffer (editor.buffers[editor.activeBuffer])
#define curLine (curBuffer->lines[curBuffer->cursor.row])
#define curRow (curBuffer->cursor.row)
#define curCol (curBuffer->cursor.col)
#define curChar (curLine.chars[curCol])

// Populates editor global struct and creates empty file buffer. Exits on error.
void EditorInit(CmdOptions options);
void EditorFree();

// Hangs when waiting for input. Returns error if read failed. Writes to info.
Status EditorReadInput(InputInfo *info);

// Waits for input and takes action for insert mode.
Status EditorHandleInput();

// Loads file into buffer. Filepath must either be an absolute path
// or name of a file in the same directory as working directory.
Status EditorOpenFile(char *filepath);

// Writes content of buffer to filepath. Always truncates file.
Status EditorSaveFile();

// Loads config file and writes to given config. Sets default config
// if file failed to open.
Status LoadConfig(Config *config);

void Undo();
void Redo();

// Saves action to undo stack. May group it with previous actions if suitable.
void UndoSaveAction(Action type, char *text, int textLen);
void UndoSaveActionEx(Action type, int row, int col, char *text, int textLen);

// Joins last n actions under same undo call.
void UndoJoin(int n);
