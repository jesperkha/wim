#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdbool.h>
#include <string.h>

#include "theme.h"

#define TITLE "wim v0.1.0"
#define UPDATED "21.12.23"

#define DEBUG_MODE

#define RETURN_SUCCESS 1
#define RETURN_ERROR 0

#define BUFFER_LINE_CAP 32
#define DEFAULT_LINE_LENGTH 256

#define COLORS_LENGTH 144
#define THEME_NAME_LEN 32

typedef struct Line
{
    int idx; // Row index in file, not buffer
    int row; // Relative row in buffer, not file

    int cap;     // Capacity of line
    int length;  // Length of line
    char *chars; // Characters in line
} Line;

enum FileTypes
{
    FT_UNKNOWN,
    FT_C,
    FT_PYTHON,
};

typedef struct Info
{
    char filename[64];
    char filepath[260];
    char error[64];
    int fileType;
    bool hasError;
    bool dirty;
    bool fileOpen;
} Info;

typedef struct Config
{
    bool syntaxEnabled;
    bool matchParen;
    bool useCRLF;
    int tabSize;
} Config;

typedef struct Editor
{
    Info info;
    Config config;

    HANDLE hstdin;  // Handle for standard input
    HANDLE hbuffer; // Handle to new screen buffer

    COORD initSize;    // Inital size to return to
    int width, height; // Size of terminal window
    int textW, textH;  // Size of text editing area
    int padV, padH;    // Vertical and horizontal padding

    int row, col;           // Current row and col of cursor in buffer
    int colMax;             // Keep track of the most right pos of the cursor when moving down
    int offx, offy;         // x, y offset from left/top
    int indent;             // Indent in spaces for current line
    int scrollDx, scrollDy; // Minimum distance from top/bottom or left/right before scrolling

    int numLines, lineCap; // Count and capacity of lines in array
    Line *lines;           // Array of lines in buffer

    char *renderBuffer; // Written to and printed on render
    char colors[COLORS_LENGTH]; // Theme colors
} Editor;

enum InputEvents
{
    INPUT_UNKNOWN,
    INPUT_KEYDOWN,
    INPUT_WINDOW_RESIZE,
};

// Input record written to by editorReadInput()
typedef struct InputInfo
{
    int eventType;  // Event type: INPUT_KEYDOWN, INPUT_WINDOW_RESIZE
    char asciiChar; // Ascii character
    int keyCode;    // Virtual key code
    bool ctrlDown;  // If control is pressed
} InputInfo;

enum KeyCodes
{
    K_BACKSPACE = 8,
    K_TAB = 9,
    K_ENTER = 13,
    K_CTRL = 17,
    K_ESCAPE = 27,
    K_SPACE = 32,
    K_PAGEUP = 33,
    K_PAGEDOWN = 34,
    K_DELETE = 46,
    K_COLON = 58,

    K_ARROW_LEFT = 37,
    K_ARROW_UP,
    K_ARROW_RIGHT,
    K_ARROW_DOWN,
};

// Part of the impl module. Called from core.
void onInput();
void onExit();

Editor *editorGetHandle();
void editorInit();
void editorReset();
void editorExit();
void editorWriteAt(int x, int y, const char *text);
void editorUpdateSize();
int editorReadInput(InputInfo *info);
int editorHandleInput();
void editorPromptFileNotSaved();
int editorOpenFile(char *filepath);
int editorSaveFile();
void editorCommand(char *command);
int editorLoadTheme(const char *theme);

void screenBufferWrite(const char *string, int length);
void screenBufferClearAll();
void screenBufferBg(int col);
void screenBufferFg(int col);
void screenBufferClearLine(int row);

void cursorHide();
void cursorShow();
void cursorSetPos(int x, int y, bool keepX);
void cursorMove(int x, int y);
void cursorTempPos(int x, int y);
void cursorRestore();

void bufferCreateLine(int idx);
void bufferWriteChar(char c);
void bufferDeleteChar();
void bufferExtendLine(int row, int new_size);
void bufferInsertLine(int row);
void bufferDeleteLine(int row);
void bufferSplitLineDown(int row);
void bufferSplitLineUp(int row);
void bufferScroll(int x, int y);
void bufferScrollDown();
void bufferScrollUp();

void typingInsertTab();
void typingDeleteForward();
void typingBreakParen();
void typingMatchParen(char c);

void renderBuffer();
void renderBufferBlank();

void statusBarUpdate(char *filename, char *error);
void statusBarClear();

// void terminalOpen();

typedef struct CharBuffer
{
    char *buffer;
    char *pos;
    int lineLength;
} CharBuffer;

void charbufClear(CharBuffer *buf);
void charbufAppend(CharBuffer *buf, char *src, int length);
void charbufNextLine(CharBuffer *buf);
void charbufColor(CharBuffer *buf, char *col);
void charbufRender(CharBuffer *buf, int x, int y);
void charbufBg(CharBuffer *buf, int col);
void charbufFg(CharBuffer *buf, int col);

enum
{
    UI_YES,
    UI_NO,
    UI_OK,
    UI_CANCEL,
};

int uiPromptYesNo(char *message, bool select);
int uiTextInput(int x, int y, char *buffer, int size);

enum
{
    HL_KEYWORD,
    HL_NUMBER,
    HL_STRING,
    HL_TYPE,
};

char *highlightLine(char *line, int lineLength, int *newLength);
