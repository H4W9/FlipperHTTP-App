#pragma once
#include "easy_flipper/easy_flipper.h"

typedef enum
{
    AppViewMainMenu,
    AppViewStatus,
    AppViewConnect,
    AppViewScan,
    AppViewDeauth,
    AppViewCaptivePortal
} AppView;

class FlipperHTTPApp;

class FlipperHTTPRun
{
    void *appContext;         // reference to the app context
    uint8_t currentMenuIndex; // current menu index
    AppView currentView;      // current view of the social run
    bool inputHeld;           // flag to check if input is held
    InputKey lastInput;       // last input key pressed
    bool shouldDebounce;      // flag to debounce input
    bool shouldReturnToMenu;  // Flag to signal return to menu
public:
    FlipperHTTPRun(void *appContext);
    ~FlipperHTTPRun();
    //
    bool isActive() const { return shouldReturnToMenu == false; }                                    // Check if the run is active
    void debounceInput();                                                                            // debounce input to prevent multiple triggers
    void drawMainMenuView(Canvas *canvas);                                                           // Draw the main menu view
    void drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount); // Generic menu drawer
    void updateDraw(Canvas *canvas);                                                                 // update and draw the run
    void updateInput(InputEvent *event);                                                             // update input for the run
};
