#pragma once
#include "easy_flipper/easy_flipper.h"

typedef enum
{
    AppViewMainMenu = -1,
    AppViewStatus = 0,
    AppViewConnect = 1,
    AppViewScan = 2,
    AppViewDeauth = 3,
    AppViewCaptivePortal = 4
} AppView;

typedef enum
{
    RequestStatusCredentialsMissing = -1, // Credentials missing
    RequestStatusSuccess = 0,             // Request successful
    RequestStatusRequestError = 1,        // Request error
    RequestStatusNotStarted = 2,          // Request not started
    RequestStatusWaiting = 3,             // Waiting for request response
    RequestStatusParseError = 4,          // Error parsing request info
} RequestStatus;

typedef enum
{
    RequestTypeStatusConnection, // Check if Wi-Fi is connected
    RequestTypeStatusSSID,       // Get current SSID
    RequestTypeStatusIP,         // Get current IP address
    RequestTypeConnect,          // Connect to Wi-Fi
    RequestTypeScan,             // Scan for Wi-Fi networks
} RequestType;

class FlipperHTTPApp;

class FlipperHTTPRun
{
    void *appContext;            // reference to the app context
    RequestStatus connectStatus; // status of the Connect view
    uint8_t currentMenuIndex;    // current menu index
    AppView currentView;         // current view of the social run
    bool inputHeld;              // flag to check if input is held
    InputKey lastInput;          // last input key pressed
    RequestStatus scanStatus;    // status of the Scan view
    bool shouldDebounce;         // flag to debounce input
    bool shouldReturnToMenu;     // Flag to signal return to menu
    RequestStatus statusStatus;  // status of the Status view
public:
    FlipperHTTPRun(void *appContext);
    ~FlipperHTTPRun();
    //
    bool isActive() const { return shouldReturnToMenu == false; }                                    // Check if the run is active
    void debounceInput();                                                                            // debounce input to prevent multiple triggers
    void drawMainMenuView(Canvas *canvas);                                                           // Draw the main menu view
    void drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount); // Generic menu drawer
    bool httpRequestIsFinished();                                                                    // check if the HTTP request is finished
    void updateDraw(Canvas *canvas);                                                                 // update and draw the run
    void updateInput(InputEvent *event);                                                             // update input for the run
    void userRequest(RequestType requestType);                                                       // Send a user request to the server based on the request type
};
