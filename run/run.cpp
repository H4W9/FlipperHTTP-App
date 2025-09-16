#include "run/run.hpp"
#include "app.hpp"

FlipperHTTPRun::FlipperHTTPRun(void *appContext) : appContext(appContext), shouldReturnToMenu(false)
{
}

FlipperHTTPRun::~FlipperHTTPRun()
{
    // nothing to do
}

void FlipperHTTPRun::debounceInput()
{
    static uint8_t debounceCounter = 0;
    if (shouldDebounce)
    {
        lastInput = InputKeyMAX;
        debounceCounter++;
        if (debounceCounter < 2)
        {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

void FlipperHTTPRun::drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount)
{
    canvas_clear(canvas);

    // Draw title
    canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
    const char *title = "FlipperHTTP";
    int title_width = canvas_string_width(canvas, title);
    int title_x = (128 - title_width) / 2;
    canvas_draw_str(canvas, title_x, 12, title);

    // Draw underline for title
    canvas_draw_line(canvas, title_x, 14, title_x + title_width, 14);

    // Draw decorative horizontal pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 18);
    }

    // Menu items with word wrapping
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    const char *currentItem = menuItems[selectedIndex];

    const int box_padding = 10;
    const int box_width = 108;
    const int usable_width = box_width - (box_padding * 2); // Text area inside box
    const int line_height = 8;                              // Typical line height for medium font
    const int max_lines = 2;                                // Maximum lines to prevent overflow

    int menu_y = 40;

    // Calculate word wrapping
    char lines[max_lines][64];
    int line_count = 0;

    // word wrap
    const char *text = currentItem;
    int text_len = strlen(text);
    int current_pos = 0;

    while (current_pos < text_len && line_count < max_lines)
    {
        int line_start = current_pos;
        int last_space = -1;
        int current_width = 0;
        int char_pos = 0;

        // Find how much text fits on this line
        while (current_pos < text_len && char_pos < 63) // Leave room for null terminator
        {
            if (text[current_pos] == ' ')
            {
                last_space = char_pos;
            }

            lines[line_count][char_pos] = text[current_pos];
            char_pos++;

            // Check if adding this character exceeds width
            lines[line_count][char_pos] = '\0'; // Temporary null terminator
            current_width = canvas_string_width(canvas, lines[line_count]);

            if (current_width > usable_width)
            {
                // Text is too wide, need to break
                if (last_space > 0)
                {
                    // Break at last space
                    lines[line_count][last_space] = '\0';
                    current_pos = line_start + last_space + 1; // Skip the space
                }
                else
                {
                    // No space found, break at previous character
                    char_pos--;
                    lines[line_count][char_pos] = '\0';
                    current_pos = line_start + char_pos;
                }
                break;
            }

            current_pos++;
        }

        // If we reached end of text
        if (current_pos >= text_len)
        {
            lines[line_count][char_pos] = '\0';
        }

        line_count++;
    }

    // If there's still more text and we're at max lines, add ellipsis
    if (current_pos < text_len && line_count == max_lines)
    {
        int last_line = line_count - 1;
        int line_len = strlen(lines[last_line]);
        if (line_len > 3)
        {
            strcpy(&lines[last_line][line_len - 3], "...");
        }
    }

    // Calculate box height based on number of lines, but keep minimum height
    int box_height = (line_count * line_height) + 8;
    if (box_height < 16)
        box_height = 16;

    // Dynamic box positioning based on content height
    int box_y_offset;
    if (line_count > 1)
    {
        box_y_offset = -22;
    }
    else
    {
        box_y_offset = -12;
    }

    // Draw main selection box
    canvas_draw_rbox(canvas, 10, menu_y + box_y_offset, box_width, box_height, 4);
    canvas_set_color(canvas, ColorWhite);

    // Draw each line of text centered
    for (int i = 0; i < line_count; i++)
    {
        int line_width = canvas_string_width(canvas, lines[i]);
        int line_x = (128 - line_width) / 2;
        int text_y_offset = (line_count > 1) ? -18 : -4;
        int line_y = menu_y + (i * line_height) + 4 + text_y_offset;
        canvas_draw_str(canvas, line_x, line_y, lines[i]);
    }

    canvas_set_color(canvas, ColorBlack);

    // Draw navigation arrows
    if (selectedIndex > 0)
    {
        canvas_draw_str(canvas, 2, menu_y + 4, "<");
    }
    if (selectedIndex < (menuCount - 1))
    {
        canvas_draw_str(canvas, 122, menu_y + 4, ">");
    }

    const int MAX_DOTS = 15;
    const int dots_spacing = 6;
    int indicator_y = 52;

    if (menuCount <= MAX_DOTS)
    {
        // Show all dots if they fit
        int dots_start_x = (128 - (menuCount * dots_spacing)) / 2;
        for (int i = 0; i < menuCount; i++)
        {
            int dot_x = dots_start_x + (i * dots_spacing);
            if (i == selectedIndex)
            {
                canvas_draw_box(canvas, dot_x, indicator_y, 4, 4);
            }
            else
            {
                canvas_draw_frame(canvas, dot_x, indicator_y, 4, 4);
            }
        }
    }
    else
    {
        // condensed indicator with current position
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        char position_text[16];
        snprintf(position_text, sizeof(position_text), "%d/%d", selectedIndex + 1, menuCount);
        int pos_width = canvas_string_width(canvas, position_text);
        int pos_x = (128 - pos_width) / 2;
        canvas_draw_str(canvas, pos_x, indicator_y + 3, position_text);

        // progress bar
        int bar_width = 60;
        int bar_x = (128 - bar_width) / 2;
        int bar_y = indicator_y - 6;
        canvas_draw_frame(canvas, bar_x, bar_y, bar_width, 3);
        int progress_width = (selectedIndex * (bar_width - 2)) / (menuCount - 1);
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, progress_width, 1);
    }

    // Draw decorative bottom pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 58);
    }
}

void FlipperHTTPRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "FlipperHTTP Run!");
}

void FlipperHTTPRun::updateInput(InputEvent *event)
{
    lastInput = event->key;
    debounceInput();
    if (lastInput == InputKeyBack)
    {
        shouldDebounce = true;
        // return to menu
        shouldReturnToMenu = true;
    }
}
