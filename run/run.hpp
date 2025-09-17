#pragma once
#include "easy_flipper/easy_flipper.h"
#include "loading/loading.hpp"
#include "run/keyboard.hpp"
#include "memory"
#include "vector"

typedef enum
{
    AppViewMainMenu = -1,
    AppViewStatus = 0,
    AppViewConnect = 1,
    AppViewScan = 2,
    AppViewSaveWiFi = 3,
} AppView;

typedef enum
{
    RequestStatusSuccess = 0,      // Request successful
    RequestStatusRequestError = 1, // Request error
    RequestStatusNotStarted = 2,   // Request not started
    RequestStatusWaiting = 3,      // Waiting for request response
    RequestStatusParseError = 4,   // Error parsing request info
    RequestStatusKeyboard = 5,     // Keyboard for input (only used in scan for now)
} RequestStatus;

typedef enum
{
    RequestTypeStatusConnection, // Check if Wi-Fi is connected
    RequestTypeStatusSSID,       // Get current SSID
    RequestTypeStatusIP,         // Get current IP address of the device
    RequestTypeConnect,          // Connect to Wi-Fi
    RequestTypeScan,             // Scan for Wi-Fi networks
    RequestTypeSaveWiFi,         // Save Wi-Fi credentials
} RequestType;

typedef enum
{
    ConnectionTypeConnection, // check connection status
    ConnectionTypeSSID,       // get current SSID
    ConnectionTypeIP,         // get current IP address of the device
} ConnectionType;

class FlipperHTTPApp;

class FlipperHTTPRun
{
    void *appContext;                   // reference to the app context
    bool connectInfoStatus;             // true/false if is connected
    std::string currentSSID;            // current connected SSID
    std::string currentIP;              // current IP address of the device
    ConnectionType connectionType;      // type of connection info to retrieve
    RequestStatus connectStatus;        // status of the Connect view
    uint8_t currentMenuIndex;           // current menu index
    uint8_t currentSSIDIndex;           // current SSID index for scan view
    AppView currentView;                // current view of the social run
    bool inputHeld;                     // flag to check if input is held
    std::unique_ptr<Keyboard> keyboard; // keyboard instance for input handling
    InputKey lastInput;                 // last input key pressed
    std::unique_ptr<Loading> loading;   // loading animation instance
    RequestStatus saveWiFiStatus;       // status of the Save WiFi view
    RequestStatus scanStatus;           // status of the Scan view
    std::vector<std::string> ssidList;  // list of scanned SSIDs
    bool shouldDebounce;                // flag to debounce input
    bool shouldReturnToMenu;            // Flag to signal return to menu
    RequestStatus statusStatus;         // status of the Status view
public:
    FlipperHTTPRun(void *appContext);
    ~FlipperHTTPRun();
    //
    bool isActive() const { return shouldReturnToMenu == false; }                                    // Check if the run is active
    void debounceInput();                                                                            // debounce input to prevent multiple triggers
    void drawConnectView(Canvas *canvas);                                                            // Draw the Connect view
    void drawMainMenuView(Canvas *canvas);                                                           // Draw the main menu view
    void drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount); // Generic menu drawer
    void drawSaveWiFiView(Canvas *canvas);                                                           // Draw the Save WiFi view
    void drawScanView(Canvas *canvas);                                                               // Draw the Scan view
    void drawStatusView(Canvas *canvas);                                                             // Draw the Status view
    bool httpRequestIsFinished();                                                                    // check if the HTTP request is finished
    void updateDraw(Canvas *canvas);                                                                 // update and draw the run
    void updateInput(InputEvent *event);                                                             // update input for the run
    void userRequest(RequestType requestType);                                                       // Send a user request to the server based on the request type
};
