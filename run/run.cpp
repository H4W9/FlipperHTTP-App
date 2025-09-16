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
