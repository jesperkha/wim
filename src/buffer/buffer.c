// All things related to writing, deleting, and modifying things in the current buffer.

#include "rum.h"

extern Editor editor;
extern Colors colors;
extern Config config;

static char padding[256] = {[0 ... 255] = ' '}; // For indents

// Reallocs lines char array to new size.
static void bufferExtendLine(Buffer *b, int row, int new_size)
{
    if (row >= curBuffer->numLines)
        Error("row out of bounds");
    Line *line = &b->lines[row];
    line->cap = new_size;
    line->chars = MemRealloc(line->chars, line->cap);
    AssertNotNull(line->chars);
    memset(line->chars + line->length, 0, line->cap - line->length);
}

Buffer *BufferNew()
{
    Buffer *b = MemZeroAlloc(sizeof(Buffer));
    b->lineCap = BUFFER_DEFAULT_LINE_CAP;
    b->lines = MemZeroAlloc(b->lineCap * sizeof(Line));
    AssertNotNull(b->lines);

    b->padX = 6; // Line numbers
    b->padY = 0;

    b->cursor = (Cursor){
        .scrollDx = 5,
        .scrollDy = 5,
    };

    b->undos = list(EditorAction, UNDO_CAP);
    AssertNotNull(b->undos);

    BufferInsertLine(b, 0);
    b->dirty = false;
    b->syntaxReady = false;
    b->readOnly = false;
    b->searchLen = 0;
    return b;
}

void BufferFree(Buffer *b)
{
    for (int i = 0; i < b->numLines; i++)
        MemFree(b->lines[i].chars);

    if (b->syntaxReady)
        MemFree(b->syntaxTable);

    MemFree(b->lines);
    MemFree(b);
}

// Writes characters to buffer at row/col.
void BufferWriteEx(Buffer *b, int row, int col, char *source, int length)
{
    Line *line = &b->lines[row];

    if (line->length + length >= line->cap)
    {
        // Allocate enough memory for the total string
        int l = LINE_DEFAULT_LENGTH;
        int requiredSpace = (length / l + 1) * l;
        bufferExtendLine(b, row, line->cap + requiredSpace);
    }

    if (col < line->length)
    {
        // Move text when typing in the middle of a line
        char *pos = line->chars + col;
        memmove(pos + length, pos, line->length - col);
    }

    memcpy(line->chars + col, source, length);
    line->length += length;
    b->dirty = true;
}

// Writes characters to buffer at cursor position.
void BufferWrite(Buffer *b, char *source, int length)
{
    BufferWriteEx(b, b->cursor.row, b->cursor.col, source, length);
}

// Writes to buffer at row/col. Replaces any characters that are already there.
void BufferOverWriteEx(Buffer *b, int row, int col, char *source, int length)
{
    Line *line = &b->lines[row];

    if (line->length + length >= line->cap)
    {
        // Allocate enough memory for the total string
        int l = LINE_DEFAULT_LENGTH;
        int requiredSpace = (length / l + 1) * l;
        bufferExtendLine(b, row, line->cap + requiredSpace);
    }

    memcpy(line->chars + col, source, length);
    line->length = col + length;
    b->dirty = true;
}

// Writes to buffer at current row/col. Replaces any characters that are already there.
void BufferOverWrite(Buffer *b, char *source, int length)
{
    BufferOverWriteEx(b, b->cursor.row, b->cursor.col, source, length);
}

// Deletes backwards from col at row. Stops at empty line, does not remove newline.
void BufferDeleteEx(Buffer *b, int row, int col, int count)
{
    if (col == 0)
        return;

    Line *line = &b->lines[row];
    count = min(count, col); // Dont delete past 0

    if (col <= line->length)
    {
        // Move chars when deleting in middle of line
        char *pos = line->chars + col;
        memmove(pos - count, pos, line->length - col);
    }

    memset(line->chars + line->length, 0, line->cap - line->length);
    line->length -= count;
    b->dirty = true;
}

// Deletes backwards from cursor position. Stops at empty line, does not remove newline.
void BufferDelete(Buffer *b, int count)
{
    BufferDeleteEx(b, b->cursor.row, b->cursor.col, count);
}

// Returns number of spaces before the cursor
int BufferGetPrefixedSpaces(Buffer *b)
{
    Line *line = &b->lines[b->cursor.row];
    int prefixedSpaces = 0;

    for (int i = b->cursor.col - 1; i >= 0; i--)
    {
        if (line->chars[i] != ' ')
            break;
        prefixedSpaces++;
    }

    return prefixedSpaces;
}

// Inserts new line at row. If row is -1 line is appended to end of file. If text is not NULL, it is added to the line with correct indentation.
void BufferInsertLine(Buffer *b, int row)
{
    BufferInsertLineEx(b, row, NULL, 0);
}

void BufferInsertLineEx(Buffer *b, int row, char *text, int textLen)
{
    row = row != -1 ? row : b->numLines;

    if (b->numLines >= b->lineCap)
    {
        // Realloc editor line buffer array when full
        b->lineCap += BUFFER_DEFAULT_LINE_CAP;
        b->lines = MemRealloc(b->lines, b->lineCap * sizeof(Line));
        AssertNotNull(b->lines);
    }

    if (row < b->numLines)
    {
        // Move lines down when adding newline mid-file
        Line *pos = b->lines + row;
        int count = b->numLines - row;
        memmove(pos + 1, pos, count * sizeof(Line));
    }

    char *chars = NULL;
    int cap = LINE_DEFAULT_LENGTH;

    if (text != NULL)
    {
        // Copy over text and set correct line length/cap
        if (textLen + b->cursor.indent >= cap)
        {
            int l = LINE_DEFAULT_LENGTH;
            cap = (textLen / l) * l + l;
        }

        chars = MemZeroAlloc(cap * sizeof(char));
        strncpy(chars, padding, b->cursor.indent);
        strncat(chars, text, textLen);
    }
    else
    {
        // No text was passed
        chars = MemZeroAlloc(LINE_DEFAULT_LENGTH * sizeof(char));
        strncpy(chars, padding, b->cursor.indent);
    }

    AssertNotNull(chars);

    Line line = {
        .chars = chars,
        .cap = cap,
        .row = row,
        .length = strlen(chars),
    };

    memcpy(&b->lines[row], &line, sizeof(Line));
    b->numLines++;
    b->dirty = true;
}

// Deletes line at row and move all lines below upwards.
void BufferDeleteLine(Buffer *b, int row)
{
    row = row != -1 ? row : b->numLines - 1;

    if (row > b->numLines - 1)
        return;

    Line *line = &b->lines[row];

    if (row == 0 && b->numLines == 1)
    {
        memset(line->chars, 0, line->cap);
        line->length = 0;
        return;
    }

    MemFree(line->chars);
    Line *pos = b->lines + row + 1;

    if (row != b->lineCap - 1)
    {
        int count = b->numLines - row;
        memmove(pos - 1, pos, count * sizeof(Line));
        memset(b->lines + b->numLines, 0, sizeof(Line));
    }

    b->numLines--;
    b->dirty = true;
}

// Copies and removes all characters behind the cursor position,
// then pastes them at the end of the line below.
void BufferMoveTextDownEx(Buffer *b, int row, int col)
{
    Line *from = &b->lines[row];
    Line *to = &b->lines[row + 1];
    int length = from->length - col;

    if (to->cap <= length + to->length)
    {
        // Realloc line buffer so new text fits
        int l = LINE_DEFAULT_LENGTH;
        bufferExtendLine(b, row + 1, (length / l) * l + l);
    }

    // Copy characters and set right side of row to 0
    strcpy(to->chars + to->length, from->chars + col);
    memset(from->chars + col + to->length, 0, length);
    to->length += length;
    from->length -= length;
    b->dirty = true;
}

// Copies and removes all characters behind the cursor position,
// then pastes them at the end of the line below.
void BufferMoveTextDown(Buffer *b)
{
    BufferMoveTextDownEx(b, b->cursor.row, b->cursor.col);
}

// Moves line content from row to end of line above. Returns length of line above.
int BufferMoveTextUpEx(Buffer *b, int row, int col)
{
    Line *from = &b->lines[row];
    Line *to = &b->lines[row - 1];
    int toLength = to->length;

    if (from->length == 0)
        return toLength;

    int length = from->length - col + to->length;
    if (to->cap <= length)
    {
        // Realloc line buffer so new text fits
        int l = LINE_DEFAULT_LENGTH;
        bufferExtendLine(b, row - 1, (length / l) * l + l);
    }

    memcpy(to->chars + to->length, from->chars, from->length);
    to->length += from->length;
    b->dirty = true;
    return toLength;
}

// Moves line content from row to end of line above. Returns length of line above.
int BufferMoveTextUp(Buffer *b)
{
    return BufferMoveTextUpEx(b, b->cursor.row, b->cursor.col);
}

void BufferScroll(Buffer *b)
{
    // Cursor position on screen
    int real_y = (b->cursor.row - b->cursor.offy);

    if (real_y < b->cursor.scrollDy)
        b->cursor.offy = max(b->cursor.row - b->cursor.scrollDy, 0);
    else if (real_y > b->textH - b->cursor.scrollDy)
        b->cursor.offy = min(b->cursor.row - b->textH + b->cursor.scrollDy, b->numLines - b->textH);
}

// From buffer/color.c
//
// Returns pointer to highlight buffer. Must NOT be freed. Line is the
// pointer to the line contents and the length is excluding the NULL
// terminator. Writes byte length of highlighted text to newLength.
char *HighlightLine(Buffer *b, char *line, int lineLength, int *newLength);

void BufferRender(Buffer *b, int y, int h)
{
    int textW = editor.width - b->padX;
    int textH = h - b->padY;
    b->textH = textH;

    CharBuf cb = CbNew(editor.renderBuffer);

    for (int i = 0; i < textH; i++)
    {
        int row = i + b->cursor.offy;

        if (row >= b->numLines || y + i >= editor.height)
            break;

        Line line = b->lines[row];

        // Line background color
        if (b->cursor.row == row)
            CbColor(&cb, colors.bg1, colors.yellow);
        else
            CbColor(&cb, colors.bg0, colors.bg2);

        // Line numbers
        char numbuf[12];
        sprintf(numbuf, " %4d ", (short)(row + 1));
        CbAppend(&cb, numbuf, b->padX);

        // Line contents
        CbFg(&cb, colors.fg0);
        b->cursor.offx = max(b->cursor.col - textW + b->cursor.scrollDx, 0);
        int lineLength = line.length - b->cursor.offx;

        int renderLength = max(min(min(lineLength, textW), editor.width), 0);
        char *lineBegin = line.chars + b->cursor.offx;

        if (config.syntaxEnabled && b->syntaxReady)
        {
            // Generate syntax highlighting for line and get new byte length
            int newLength;
            char *line = HighlightLine(b, lineBegin, renderLength, &newLength);
            CbAppend(&cb, line, newLength);
        }
        else
            CbAppend(&cb, lineBegin, renderLength);

        // Padding after
        if (renderLength < textW)
            CbAppend(&cb, padding, textW - renderLength);
    }

    // Draw squiggles for non-filled lines
    CbColor(&cb, colors.bg0, colors.bg2);
    if (b->numLines < b->textH)
    {
        for (int i = 0; i < b->textH - b->numLines; i++)
        {
            CbAppend(&cb, "~", 1);
            CbAppend(&cb, padding, editor.width - 1);
        }
    }

    CbRender(&cb, 0, y);
}

// Draws buffer contents at x, y, with a maximum width and height.
void BufferRenderEx(Buffer *b, int x, int y, int width, int height)
{
    HANDLE H = editor.hbuffer;

    int textW = width - b->padX;
    int textH = height - b->padY;
    b->textH = textH;

    CursorHide();

    for (int i = 0; i < textH; i++)
    {
        int row = i + b->cursor.offy;

        if (row >= b->numLines || y + i >= editor.height)
            break;

        Line line = b->lines[row];
        SetConsoleCursorPosition(H, (COORD){x, y + i});

        // Line background color
        if (b->cursor.row == row)
            ScreenColor(colors.bg1, colors.yellow);
        else
            ScreenColor(colors.bg0, colors.bg2);

        // Line numbers
        char numbuf[12];
        sprintf(numbuf, " %4d ", (short)(row + 1));
        ScreenWrite(numbuf, b->padX);

        // Line contents
        ScreenFg(colors.fg0);
        b->cursor.offx = max(b->cursor.col - textW + b->cursor.scrollDx, 0);
        int lineLength = line.length - b->cursor.offx;

        int renderLength = max(min(min(lineLength, textW), editor.width), 0);
        char *lineBegin = line.chars + b->cursor.offx;

        if (config.syntaxEnabled && b->syntaxReady)
        {
            // Generate syntax highlighting for line and get new byte length
            int newLength;
            char *line = HighlightLine(b, lineBegin, renderLength, &newLength);
            ScreenWrite(line, newLength);
        }
        else
            ScreenWrite(lineBegin, renderLength);

        // Padding after
        if (renderLength < textW)
            ScreenWrite(padding, textW - renderLength);
    }

    // Draw squiggles for non-filled lines
    ScreenColor(colors.bg0, colors.bg2);
    if (b->numLines < b->textH)
    {
        for (int i = 0; i < b->textH - b->numLines; i++)
        {
            ScreenWrite("~", 1);
            ScreenWrite(padding, editor.width - 1);
        }
    }

    CursorShow();
}

// Loads file contents into a new Buffer and returns it.
Buffer *BufferLoadFile(char *filepath, char *buf, int size)
{
    Logf("File size: %d", size);
    Buffer *b = BufferNew();
    b->isFile = true;
    strcpy(b->filepath, filepath);

    char *newline;
    char *ptr = buf;
    int row = 0;

    while ((newline = strstr(ptr, "\n")) != NULL)
    {
        // Get distance from current pos in buffer and found newline
        // Then strncpy the line into the line char buffer
        int length = newline - ptr;
        if (length > 0 && *(newline - 1) == '\r')
            BufferInsertLineEx(b, row, ptr, length - 1);
        else
            BufferInsertLineEx(b, row, ptr, length);
        ptr += length + 1;
        row++;
    }

    // Write last line of file
    BufferInsertLineEx(b, row, ptr, size - (ptr - buf));
    BufferDeleteLine(b, -1); // Remove line added at buffer create
    b->dirty = false;
    return b;
}

// Saves buffer contents to file. Returns true on success.
bool BufferSaveFile(Buffer *b)
{
    if (b->readOnly)
        return false;

    // Give file name before saving if blank
    if (!b->isFile)
    {
        UiResult res = UiGetTextInput("Filename: ", 64);
        if (res.status != UI_OK || res.length == 0)
        {
            UiFreeResult(res);
            return false;
        }

        strncpy(b->filepath, res.buffer, res.length);
        b->isFile = true;
        UiFreeResult(res);
    }

    bool CRLF = config.useCRLF;

    // Accumulate size of buffer by line length
    int size = 0;
    int newlineSize = CRLF ? 2 : 1;

    for (int i = 0; i < b->numLines; i++)
        size += b->lines[i].length + newlineSize;

    // Write to buffer, add newline for each line
    char buf[size];
    char *ptr = buf;
    for (int i = 0; i < b->numLines; i++)
    {
        Line line = b->lines[i];
        memcpy(ptr, line.chars, line.length);
        ptr += line.length;
        if (CRLF)
            *(ptr++) = '\r'; // CR
        *(ptr++) = '\n';     // LF
    }

    // Open file - truncate existing and write
    HANDLE file = CreateFileA(b->filepath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        Error("failed to open file");
        return false;
    }

    DWORD written;
    if (!WriteFile(file, buf, size - newlineSize, &written, NULL))
    {
        Error("failed to write to file");
        CloseHandle(file);
        return false;
    }

    b->dirty = false;
    CloseHandle(file);
    return true;
}
