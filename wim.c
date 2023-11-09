#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "wim.h"

#define log(msg) editorWriteAt(0, 9, "Log: "); editorWriteAt(5, 9, msg);
#define error(msg) editorWriteAt(0, 10, "Error: "); editorWriteAt(7, 10, msg);

// ---------------------- GLOBAL STATE ----------------------

typedef struct linebuf
{
    int idx;     // Row index in file, not buffer
    int row;     // Relative row in buffer, not file
    
    int cap;     // Capacity of line
    int length;  // Length of line
    char *chars; // Characters in line
} linebuf;

struct editorGlobals
{
    HANDLE hstdin;  // Handle for standard input
    HANDLE hstdout; // Handle for standard output
    DWORD mode_in;  // Restore to previous input mode

    int width, height; // Size of terminal window

    int cx, cy;     // Relative cursor x, y (to buffer)
    int absx, absy; // Absolute cursor x, y

    int numLines, lineCap; // Count and capacity of lines in array
    linebuf* lines;        // Array of lines in buffer
} editor;

// ---------------------- EDITOR ----------------------

// Populates editor global struct and creates empty file buffer. Exits on error.
void editorInit()
{
    editor.hstdin = GetStdHandle(STD_INPUT_HANDLE);
    editor.hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(editor.hstdin, &editor.mode_in);
    SetConsoleMode(editor.hstdin, 0);

    if (editor.hstdin == NULL || editor.hstdout == NULL)
    {
        error("editorInit() - Failed to get std handles");
        ExitProcess(EXIT_FAILURE);
    }

    editorTerminalGetSize();
    if (editorClearScreen() == RETURN_ERROR)
    {
        error("editorInit() - Failed to clear screen");
        ExitProcess(EXIT_FAILURE);
    }

    editorTerminalGetSize(); // Get new height of buffer
    FlushConsoleInputBuffer(editor.hstdin);
    SetConsoleTitle(TITLE);
}

// Free, clean, and exit
void editorExit()
{
    bufferFree();
    editorClearScreen();
    SetConsoleMode(editor.hstdin, editor.mode_in);
    ExitProcess(EXIT_SUCCESS);
}

// Update editor size values. Returns -1 on error.
int editorTerminalGetSize()
{
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    if (!GetConsoleScreenBufferInfo(editor.hstdout, &cinfo))
        return_error("editorTerminalGetSize() - Failed to get buffer info");

    editor.width = (int)cinfo.srWindow.Right;
    editor.height = (int)(cinfo.srWindow.Bottom + 1);

    return RETURN_SUCCESS;
}

// Clears the terminal. Returns -1 on error.
int editorClearScreen()
{
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    if (!GetConsoleScreenBufferInfo(editor.hstdout, &cinfo))
        return_error("editorClearScreen() - Failed to get buffer info");

    cursorSetPos(0, 0);

    DWORD written;
    DWORD size = editor.width * editor.height;
    COORD origin = {0, 0};
    if (!FillConsoleOutputCharacter(editor.hstdout, (WCHAR)' ', size, origin, &written))
        return_error("editorClearScreen() - Failed to fill buffer");

    return RETURN_SUCCESS;
}

// Writes text at given x, y.
void editorWriteAt(int x, int y, const char* text)
{
    int ox = editor.cx;
    int oy = editor.cy;
    cursorSetPos(x, y);
    printf("%s", text);
    cursorSetPos(ox, oy);
}

// Takes action based on current user input. Returns -1 on error.
int editorHandleInput()
{
    INPUT_RECORD record;
    DWORD read;

    if (!ReadConsoleInput(editor.hstdin, &record, 1, &read) || read == 0)
        return_error("editorHandleInput() - Failed to read input");

    if (record.EventType == KEY_EVENT)
    {
        if (record.Event.KeyEvent.bKeyDown)
        {
            KEY_EVENT_RECORD event = record.Event.KeyEvent;
            WORD keyCode = event.wVirtualKeyCode;
            char inputChar = event.uChar.AsciiChar;

            switch (keyCode)
            {
            case ESCAPE:
                editorExit();

            case BACKSPACE:
                bufferDeleteChar();
                break;

            case ENTER:
                break;
            
            case ARROW_UP:
                cursorMove(0, -1);
                break;
            
            case ARROW_DOWN:
                cursorMove(0, 1);
                break;

            case ARROW_LEFT:
                cursorMove(-1, 0);
                break;

            case ARROW_RIGHT:
                cursorMove(1, 0);
                break;

            default:
                bufferWriteChar(inputChar);
            }
        }
    }

    return RETURN_SUCCESS;
}

// ---------------------- CURSOR ----------------------

void cursorShow()
{
    CONSOLE_CURSOR_INFO info = {100, true};
    SetConsoleCursorInfo(editor.hstdout, &info);
}

void cursorHide()
{
    CONSOLE_CURSOR_INFO info = {100, false};
    SetConsoleCursorInfo(editor.hstdout, &info);
}

// Adds x, y to cursor position
void cursorMove(int x, int y)
{
    editor.cx += x;
    editor.cy += y;
    editor.absx += x;
    editor.absy += y;
    cursorSetPos(editor.absx, editor.absy);
}

// S|ts the cursor position to x, y
void cursorSetPos(int x, int y)
{
    editor.cx = x;
    editor.cy = y;
    editor.absx = x;
    editor.absy = y;
    COORD pos = {x, y};
    SetConsoleCursorPosition(editor.hstdout, pos);
}

// ---------------------- BUFFER ----------------------

// Creates an empty file buffer.
void bufferCreate()
{

}

// Free lines in buffer
void bufferFree()
{
    for (int i = 0; i < editor.numLines; i++)
        free(editor.lines[i].chars);
    
    free(editor.lines);
}

// Renders the line found at given row index.
void bufferRenderLine(int row)
{

}

// Write single character to current line.
void bufferWriteChar(char c)
{

}

// Deletes the caharcter before the cursor position.
void bufferDeleteChar()
{

}

// Inserts new line at row. If row is -1 line is appended to end of file.
void bufferInsertLine(int row)
{

}

int main(void)
{
    editorInit();

    log("Press ESC to exit");

    while (1)
    {
        editorHandleInput();
    }

    editorExit();

    return EXIT_SUCCESS;
}