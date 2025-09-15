#include "run/run.hpp"
#include "app.hpp"

FlipperHTTPRun::FlipperHTTPRun(void *appContext) : appContext(appContext), shouldReturnToMenu(false)
{
}

FlipperHTTPRun::~FlipperHTTPRun()
{
    // nothing to do
}

void FlipperHTTPRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "FlipperHTTP Run!");
}

void FlipperHTTPRun::updateInput(InputEvent *event)
{
    if (event->key == InputKeyBack)
    {
        // return to menu
        shouldReturnToMenu = true;
    }
}
