#include "run/run.hpp"
#include "app.hpp"



FlipperHTTPRun::FlipperHTTPRun(void *appContext)
    : appContext(appContext), connectInfoStatus(false), connectionType(ConnectionTypeConnection),
      connectStatus(RequestStatusNotStarted), currentMenuIndex(0), currentSSIDIndex(0),
      currentView(AppViewMainMenu), inputHeld(false), keyboard(nullptr),
      keyboardFlow(KeyboardFlowScanPassword), keyboardSkipEvents(0), lastInput(InputKeyMAX),
      loading(nullptr), loadingStarted(false),
      playlist(nullptr), savedAPIndex(0), savedAPsStatus(RequestStatusNotStarted),
      savedAPDetailStatus(RequestStatusNotStarted), commandIndex(0),
      commandStatus(RequestStatusNotStarted), commandResponseScrollOffset(0), commandResponseMaxScroll(0),
      saveWiFiStatus(RequestStatusNotStarted), scanStatus(RequestStatusNotStarted),
      shouldDebounce(false), shouldReturnToMenu(false), statusStatus(RequestStatusNotStarted)
{
    memset(pendingSSID, 0, sizeof(pendingSSID));
}

FlipperHTTPRun::~FlipperHTTPRun()
{
    // nothing to do (playlist is freed below if allocated)
    if (playlist)
    {
        free(playlist);
        playlist = nullptr;
    }
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

void FlipperHTTPRun::drawConnectView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    switch (connectStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Connecting...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                connectStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                /* This returns:
                 - [SUCCESS] Connected to Wifi.
                 - [ERROR] Failed to connect to Wifi.
                 - [INFO] Already connected to WiFi.
                */
                if (strstr(response, "[ERROR]") == NULL)
                {
                    connectStatus = RequestStatusSuccess;
                }
                else
                {
                    connectStatus = RequestStatusRequestError;
                }
            }
            else
            {
                connectStatus = RequestStatusRequestError;
            }
        }
        break;
    case RequestStatusSuccess:
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 10, "WiFi saved and connected!!");
        canvas_draw_str(canvas, 0, 60, "Press 'BACK' to leave.");
        canvas_set_font(canvas, FontPrimary);
        break;
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Connect request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Connecting...");
        break;
    }
}

void FlipperHTTPRun::drawMainMenuView(Canvas *canvas)
{
    const char *menuItems[] = {"Status", "Connect", "Scan", "Saved APs", "Commands"};
    drawMenu(canvas, (uint8_t)currentMenuIndex, menuItems, 5);
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

void FlipperHTTPRun::drawSaveWiFiView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    switch (saveWiFiStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Saving...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                saveWiFiStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                /* This returns:
                 - [SUCCESS] Wifi settings saved.
                 - [ERROR] Failed to save Wifi settings.
                 - [ERROR] Failed to parse JSON
                 - [ERROR] JSON does not contain ssid and password.
                 - [ERROR] Failed to save settings to file.
                 - [SUCCESS] Connected to the new Wifi network.
                 - [ERROR] Failed to parse JSON data.
                 - [ERROR] JSON must contain 'ssid' and 'password'.
                 - [ERROR] Failed to write settings to storage.
                 - [SUCCESS] Settings saved.
                */
                if (strstr(response, "[ERROR]") == NULL)
                {
                    saveWiFiStatus = RequestStatusSuccess;

                    // switch to connect
                    currentView = AppViewConnect;
                    connectStatus = RequestStatusWaiting;
                    loadingStarted = false;
                    userRequest(RequestTypeConnect);
                }
                else
                {
                    saveWiFiStatus = RequestStatusRequestError;
                }
            }
            else
            {
                saveWiFiStatus = RequestStatusRequestError;
            }
        }
        break;
    case RequestStatusSuccess:
        canvas_draw_str(canvas, 0, 10, "Saved successfully!");
        canvas_draw_str(canvas, 0, 20, "Press BACK to leave.");
        break;
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Save request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Saving...");
        break;
    }
}

void FlipperHTTPRun::drawScanView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    switch (scanStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Scanning...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                scanStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                if (strstr(response, "[ERROR]") == NULL)
                {
                    scanStatus = RequestStatusSuccess;

                    // Clear previous SSID list and reset index
                    ssidList.clear();

                    // Parse JSON response and populate SSID list
                    for (uint8_t i = 0; i < 50; i++)
                    {
                        char *ssid_item = get_json_array_value("networks", i, app->getHttpResponse());
                        if (ssid_item == NULL)
                        {
                            // end of the list
                            break;
                        }
                        ssidList.push_back(std::string(ssid_item));
                        free(ssid_item);
                    }
                }
                else
                {
                    scanStatus = RequestStatusRequestError;
                }
            }
            else
            {
                scanStatus = RequestStatusRequestError;
            }
        }
        break;
    case RequestStatusSuccess:
    {
        std::vector<const char *> ssid_cstr_list;
        for (const auto &ssid : ssidList)
        {
            ssid_cstr_list.push_back(ssid.c_str());
        }

        if (!ssidList.empty())
        {
            drawMenu(canvas, currentSSIDIndex, ssid_cstr_list.data(), ssidList.size());
        }
        else
        {
            canvas_draw_str(canvas, 0, 30, "No networks found!");
        }
        break;
    }
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Save request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case RequestStatusKeyboard:
        if (!keyboard)
        {
            keyboard = std::make_unique<Keyboard>();
        }
        if (keyboard)
        {
            keyboard->draw(canvas, "Enter password:");
        }
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Scanning...");
        break;
    }
}

void FlipperHTTPRun::drawStatusView(Canvas *canvas)
{
    /*
    This view sequentially collects three pieces of information:
    1. ConnectionTypeConnection: Check if Wi-Fi is connected (true/false)
    2. ConnectionTypeSSID: Get current connected SSID
    3. ConnectionTypeIP: Get current IP address
    Then displays all three together.
    */
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch (statusStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Updating...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                statusStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                // Process response based on current connection type
                switch (connectionType)
                {
                case ConnectionTypeConnection:
                    // Parse connection status response
                    if (strstr(response, "[ERROR]") == NULL)
                    {
                        connectInfoStatus = (strstr(response, "true") != NULL ||
                                             strstr(response, "Connected") != NULL ||
                                             strstr(response, "[SUCCESS]") != NULL ||
                                             strstr(response, "[INFO]") != NULL);
                    }
                    else
                    {
                        connectInfoStatus = false;
                    }
                    // Move to next request type
                    connectionType = ConnectionTypeSSID;
                    userRequest(RequestTypeStatusSSID);
                    break;

                case ConnectionTypeSSID:
                    // Parse SSID response
                    if (strstr(response, "[ERROR]") == NULL)
                    {
                        // Extract SSID from response (remove [SUCCESS] or [INFO] prefix if present)
                        const char *ssid_start = response;
                        if (strstr(response, "[SUCCESS]") != NULL)
                        {
                            ssid_start = strstr(response, "]");
                            if (ssid_start)
                                ssid_start++; // skip the ]
                            while (*ssid_start == ' ')
                                ssid_start++; // skip spaces
                        }
                        else if (strstr(response, "[INFO]") != NULL)
                        {
                            ssid_start = strstr(response, "]");
                            if (ssid_start)
                                ssid_start++; // skip the ]
                            while (*ssid_start == ' ')
                                ssid_start++; // skip spaces
                        }
                        currentSSID = std::string(ssid_start);
                    }
                    else
                    {
                        currentSSID = "Not connected";
                    }
                    // Move to next request type
                    connectionType = ConnectionTypeIP;
                    userRequest(RequestTypeStatusIP);
                    break;

                case ConnectionTypeIP:
                    // Parse IP response
                    if (strstr(response, "[ERROR]") == NULL)
                    {
                        // Extract IP from response (remove [SUCCESS] or [INFO] prefix if present)
                        const char *ip_start = response;
                        if (strstr(response, "[SUCCESS]") != NULL)
                        {
                            ip_start = strstr(response, "]");
                            if (ip_start)
                                ip_start++; // skip the ]
                            while (*ip_start == ' ')
                                ip_start++; // skip spaces
                        }
                        else if (strstr(response, "[INFO]") != NULL)
                        {
                            ip_start = strstr(response, "]");
                            if (ip_start)
                                ip_start++; // skip the ]
                            while (*ip_start == ' ')
                                ip_start++; // skip spaces
                        }
                        currentIP = std::string(ip_start);
                    }
                    else
                    {
                        currentIP = "No IP assigned";
                    }
                    // All requests complete, show results
                    statusStatus = RequestStatusSuccess;
                    break;
                }
            }
            else
            {
                statusStatus = RequestStatusRequestError;
            }
        }
        break;

    case RequestStatusSuccess:
    {
        // Display all collected information
        canvas_draw_str(canvas, 0, 10, "WiFi Status:");

        canvas_set_font(canvas, FontSecondary);

        // Connection status
        const char *connectionStatus = connectInfoStatus ? "Connected" : "Disconnected";
        canvas_draw_str(canvas, 70, 10, connectionStatus);

        // SSID
        char ssidBuffer[64];
        snprintf(ssidBuffer, sizeof(ssidBuffer), "SSID: %s", currentSSID.c_str());
        canvas_draw_str(canvas, 0, 22, ssidBuffer);

        // IP Address
        char ipBuffer[64];
        snprintf(ipBuffer, sizeof(ipBuffer), "IP: %s", currentIP.c_str());
        canvas_draw_str(canvas, 0, 34, ipBuffer);

        canvas_draw_str(canvas, 0, 60, "Press 'BACK' to leave.");

        canvas_set_font(canvas, FontPrimary);
        break;
    }

    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Status request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;

    case RequestStatusNotStarted:
    case RequestStatusParseError:
    default:
        canvas_draw_str(canvas, 0, 10, "Updating...");
        break;
    }
}

bool FlipperHTTPRun::httpRequestIsFinished()
{
    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E(TAG, "httpRequestIsFinished: App context is NULL");
        return true;
    }
    // for this app, we just need to check the response
    const char *response = app->getHttpResponse();
    if (!response || strlen(response) == 0)
    {
        return false;
    }
    // state should be IDLE in this app
    auto state = app->getHttpState();
    return state == IDLE || state == ISSUE || state == INACTIVE;
}

void FlipperHTTPRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);
    switch (currentView)
    {
    case AppViewMainMenu:
        drawMainMenuView(canvas);
        break;
    case AppViewStatus:
        drawStatusView(canvas);
        break;
    case AppViewConnect:
        drawConnectView(canvas);
        break;
    case AppViewScan:
        drawScanView(canvas);
        break;
    case AppViewSaveWiFi:
        drawSaveWiFiView(canvas);
        break;
    case AppViewSavedAPs:
        drawSavedAPsView(canvas);
        break;
    case AppViewSavedAPDetail:
        drawSavedAPDetailView(canvas);
        break;
    case AppViewCommands:
        drawCommandsView(canvas);
        break;
    default:
        break;
    };
}

void FlipperHTTPRun::updateInput(InputEvent *event)
{
    lastInput = event->key;
    debounceInput();
    switch (currentView)
    {
    case AppViewMainMenu:
        switch (lastInput)
        {
        case InputKeyRight:
            currentMenuIndex = (currentMenuIndex < 4) ? currentMenuIndex + 1 : 0; // 5 items: Status/Connect/Scan/SavedAPs/Commands
            shouldDebounce = true;
            break;
        case InputKeyLeft:
            currentMenuIndex = (currentMenuIndex > 0) ? currentMenuIndex - 1 : 4;
            shouldDebounce = true;
            break;
        case InputKeyBack:
            shouldDebounce = true;
            shouldReturnToMenu = true;
            if (loading)
            {
                loading.reset();
            }
            if (keyboard)
            {
                keyboard.reset();
            }
            currentView = AppViewMainMenu;
            currentMenuIndex = 0;
            break;
        case InputKeyOk:
            shouldDebounce = true;
            switch (currentMenuIndex)
            {
            case AppViewStatus:
                currentView = AppViewStatus;
                connectionType = ConnectionTypeConnection;
                statusStatus = RequestStatusWaiting;
                loadingStarted = false;
                userRequest(RequestTypeStatusConnection);
                break;
            case AppViewConnect:
                currentView = AppViewSaveWiFi;
                saveWiFiStatus = RequestStatusWaiting;
                loadingStarted = false;
                userRequest(RequestTypeSaveWiFi);
                break;
            case AppViewScan:
                currentView = AppViewScan;
                currentSSIDIndex = 0;
                scanStatus = RequestStatusWaiting;
                loadingStarted = false;
                userRequest(RequestTypeScan);
                break;
            case 3: // Saved APs (menu index 3)
                if (!playlist)
                {
                    playlist = static_cast<WiFiPlaylist *>(malloc(sizeof(WiFiPlaylist)));
                    if (playlist)
                    {
                        playlist->count = 0;
                        saved_aps_load(playlist);
                    }
                }
                savedAPIndex = 0;
                savedAPsStatus = RequestStatusNotStarted;
                currentView = AppViewSavedAPs;
                break;
            case 4: // Commands (menu index 4)
                commandIndex = 0;
                commandStatus = RequestStatusNotStarted;
                currentView = AppViewCommands;
                break;
            default:
                break;
            };
        default:
            break;
        }
        break;
    case AppViewScan:
        if (scanStatus == RequestStatusKeyboard)
        {
            if (keyboard)
            {
                // Long Back always exits; short Back exits when buffer is empty
                bool back_exits = (event->key == InputKeyBack) &&
                                  (event->type == InputTypeLong ||
                                   (event->type == InputTypeShort && keyboard->getTextLength() == 0));
                if (back_exits)
                {
                    // Return to the SSID list so the user doesn't lose their scan results
                    keyboard.reset();
                    scanStatus = RequestStatusSuccess;
                    shouldDebounce = true;
                }
                else if (keyboardSkipEvents > 0) { keyboardSkipEvents--; } else if (keyboard->handleInput(event))
                {
                    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
                    furi_check(app);
                    const char *ssid = ssidList[currentSSIDIndex].c_str();
                    const char *pass = keyboard->getText();
                    app->saveChar("wifi_ssid", ssid);
                    app->saveChar("wifi_pass", pass);
                    // Also persist to the Saved APs list if not already present
                    if (!playlist)
                    {
                        playlist = static_cast<WiFiPlaylist *>(malloc(sizeof(WiFiPlaylist)));
                        if (playlist)
                    {
                        playlist->count = 0;
                        saved_aps_load(playlist);
                    }
                    }
                    if (playlist && playlist->count < MAX_SAVED_APS)
                    {
                        bool already_saved = false;
                        for (size_t i = 0; i < playlist->count; i++)
                        {
                            if (strncmp(playlist->ssids[i], ssid, MAX_AP_SSID_LENGTH) == 0)
                            {
                                already_saved = true;
                                break;
                            }
                        }
                        if (!already_saved)
                        {
                            snprintf(playlist->ssids[playlist->count], MAX_AP_SSID_LENGTH, "%s", ssid);
                            snprintf(playlist->passwords[playlist->count], MAX_AP_SSID_LENGTH, "%s", pass);
                            playlist->count++;
                            saved_aps_save(playlist);
                        }
                    }
                    scanStatus = RequestStatusWaiting;
                    saveWiFiStatus = RequestStatusWaiting;
                    loadingStarted = false;
                    userRequest(RequestTypeSaveWiFi);
                    currentView = AppViewSaveWiFi;
                    currentMenuIndex = 1; // move to connect
                    currentSSIDIndex = 0;
                    keyboard.reset();
                }
                else if (lastInput != InputKeyMAX)
                {
                    shouldDebounce = true;
                }
            }
        }
        else
        {
            switch (lastInput)
            {
            case InputKeyRight:
                currentSSIDIndex = (currentSSIDIndex < ssidList.size() - 1) ? currentSSIDIndex + 1 : 0;
                shouldDebounce = true;
                break;
            case InputKeyLeft:
                currentSSIDIndex = (currentSSIDIndex > 0) ? currentSSIDIndex - 1 : ssidList.size() - 1;
                shouldDebounce = true;
                break;
            case InputKeyBack:
                shouldDebounce = true;
                currentView = AppViewMainMenu;
                break;
            case InputKeyOk:
                if (!keyboard)
                {
                    keyboard = std::make_unique<Keyboard>();
                }
                if (keyboard)
                {
                    keyboard->clearText();
                    keyboard->setText(""); // Start with empty text
                }
                scanStatus = RequestStatusKeyboard;
                keyboardSkipEvents = 2;
                shouldDebounce = true;
                break;
            default:
                break;
            }
        }
        break;
    // ── Saved APs list ────────────────────────────────────────────────────────
    case AppViewSavedAPs:
        if (savedAPsStatus == RequestStatusKeyboard)
        {
            if (keyboard)
            {
                // Long Back always exits; short Back exits when buffer is empty
                bool back_exits = (event->key == InputKeyBack) &&
                                  (event->type == InputTypeLong ||
                                   (event->type == InputTypeShort && keyboard->getTextLength() == 0));
                if (back_exits)
                {
                    keyboard.reset();
                    savedAPsStatus = RequestStatusNotStarted;
                    shouldDebounce = true;
                }
                else if (keyboardSkipEvents > 0) { keyboardSkipEvents--; } else if (keyboard->handleInput(event))
                {
                    if (keyboardFlow == KeyboardFlowSavedAPAddSSID)
                    {
                        // Store the SSID and move to the password step
                        strncpy(pendingSSID, keyboard->getText(), sizeof(pendingSSID) - 1);
                        pendingSSID[sizeof(pendingSSID) - 1] = '\0';
                        keyboard->clearText();
                        keyboard->setText("");
                        keyboardFlow = KeyboardFlowSavedAPAddPassword;
                    }
                    else // KeyboardFlowSavedAPAddPassword
                    {
                        // Add the new entry to the playlist
                        if (playlist && playlist->count < MAX_SAVED_APS)
                        {
                            snprintf(playlist->ssids[playlist->count], MAX_AP_SSID_LENGTH, "%s", pendingSSID);
                            snprintf(playlist->passwords[playlist->count], MAX_AP_SSID_LENGTH, "%s", keyboard->getText());
                            playlist->count++;
                            saved_aps_save(playlist);
                            // Land on the newly added entry (index 0 = [Add Network], so +1)
                            savedAPIndex = playlist->count;
                        }
                        keyboard.reset();
                        savedAPsStatus = RequestStatusNotStarted;
                    }
                }
                else if (lastInput != InputKeyMAX)
                {
                    shouldDebounce = true;
                }
            }
            break; // consume all input while keyboard is open
        }
        // Normal list navigation
        {
            const size_t total = playlist ? playlist->count + 1 : 1;
            switch (lastInput)
            {
            case InputKeyRight:
                savedAPIndex = (savedAPIndex < total - 1) ? savedAPIndex + 1 : 0;
                shouldDebounce = true;
                break;
            case InputKeyLeft:
                savedAPIndex = (savedAPIndex > 0) ? savedAPIndex - 1 : total - 1;
                shouldDebounce = true;
                break;
            case InputKeyBack:
                shouldDebounce = true;
                currentView = AppViewMainMenu;
                break;
            case InputKeyOk:
                shouldDebounce = true;
                if (savedAPIndex == 0)
                {
                    // [Add Network] — open keyboard for SSID
                    if (!keyboard)
                    {
                        keyboard = std::make_unique<Keyboard>();
                    }
                    if (keyboard)
                    {
                        keyboard->clearText();
                        keyboard->setText("");
                    }
                    keyboardFlow = KeyboardFlowSavedAPAddSSID;
                    savedAPsStatus = RequestStatusKeyboard;
                    keyboardSkipEvents = 2;
                }
                else
                {
                    // Open the detail view for this AP
                    savedAPDetailStatus = RequestStatusNotStarted;
                    currentView = AppViewSavedAPDetail;
                }
                break;
            default:
                break;
            }
        }
        break;

    // ── Saved AP detail ───────────────────────────────────────────────────────
    case AppViewSavedAPDetail:
        if (savedAPDetailStatus == RequestStatusKeyboard)
        {
            if (keyboard)
            {
                // Long Back always exits; short Back exits when buffer is empty
                bool back_exits = (event->key == InputKeyBack) &&
                                  (event->type == InputTypeLong ||
                                   (event->type == InputTypeShort && keyboard->getTextLength() == 0));
                if (back_exits)
                {
                    keyboard.reset();
                    savedAPDetailStatus = RequestStatusNotStarted;
                    shouldDebounce = true;
                }
                else if (keyboardSkipEvents > 0) { keyboardSkipEvents--; } else if (keyboard->handleInput(event))
                {
                    // Update password in playlist
                    const size_t idx = savedAPIndex - 1;
                    if (playlist && idx < playlist->count)
                    {
                        snprintf(playlist->passwords[idx], MAX_AP_SSID_LENGTH, "%s", keyboard->getText());
                        saved_aps_save(playlist);
                    }
                    keyboard.reset();
                    savedAPDetailStatus = RequestStatusNotStarted;
                }
                else if (lastInput != InputKeyMAX)
                {
                    shouldDebounce = true;
                }
            }
            break;
        }
        if (savedAPDetailStatus == RequestStatusWaiting ||
            savedAPDetailStatus == RequestStatusSuccess ||
            savedAPDetailStatus == RequestStatusRequestError)
        {
            if (lastInput == InputKeyBack)
            {
                shouldDebounce = true;
                savedAPDetailStatus = RequestStatusNotStarted;
                currentView = AppViewSavedAPs;
            }
            break;
        }
        // Normal detail navigation
        {
            const size_t idx = savedAPIndex - 1;
            switch (lastInput)
            {
            case InputKeyOk: // Set WiFi on the board
                shouldDebounce = true;
                if (playlist && idx < playlist->count)
                {
                    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
                    furi_check(app);
                    app->saveChar("wifi_ssid", playlist->ssids[idx]);
                    app->saveChar("wifi_pass", playlist->passwords[idx]);
                    app->clearHttpResponse();
                    bool sent = app->sendWiFiCredentials(playlist->ssids[idx], playlist->passwords[idx]);
                    if (sent)
                    {
                        loadingStarted = false;
                    }
                    savedAPDetailStatus = sent ? RequestStatusWaiting : RequestStatusRequestError;
                }
                break;
            case InputKeyRight: // Edit password
                shouldDebounce = true;
                if (playlist && idx < playlist->count)
                {
                    if (!keyboard)
                    {
                        keyboard = std::make_unique<Keyboard>();
                    }
                    if (keyboard)
                    {
                        keyboard->clearText();
                        keyboard->setText(playlist->passwords[idx]);
                    }
                    savedAPDetailStatus = RequestStatusKeyboard;
                    keyboardSkipEvents = 2;
                }
                break;
            case InputKeyLeft: // Delete AP
                shouldDebounce = true;
                if (playlist && idx < playlist->count)
                {
                    for (size_t i = idx; i < playlist->count - 1; i++)
                    {
                        strncpy(playlist->ssids[i], playlist->ssids[i + 1], MAX_AP_SSID_LENGTH);
                        strncpy(playlist->passwords[i], playlist->passwords[i + 1], MAX_AP_SSID_LENGTH);
                    }
                    playlist->count--;
                    saved_aps_save(playlist);
                    savedAPIndex = (savedAPIndex > 0) ? savedAPIndex - 1 : 0;
                    currentView = AppViewSavedAPs;
                }
                break;
            case InputKeyBack:
                shouldDebounce = true;
                currentView = AppViewSavedAPs;
                break;
            default:
                break;
            }
        }
        break;

    // ── Commands ──────────────────────────────────────────────────────────────
    case AppViewCommands:
        if (commandStatus == RequestStatusKeyboard)
        {
            if (keyboard)
            {
                // Long Back always exits; short Back exits when buffer is empty
                bool back_exits = (event->key == InputKeyBack) &&
                                  (event->type == InputTypeLong ||
                                   (event->type == InputTypeShort && keyboard->getTextLength() == 0));
                if (back_exits)
                {
                    keyboard.reset();
                    commandStatus = RequestStatusNotStarted;
                    shouldDebounce = true;
                }
                else if (keyboardSkipEvents > 0) { keyboardSkipEvents--; } else if (keyboard->handleInput(event))
                {
                    const char *cmd_text = keyboard->getText();
                    keyboard.reset();
                    if (cmd_text && strlen(cmd_text) > 0)
                    {
                        FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
                        furi_check(app);
                        app->clearHttpResponse();
                        if (app->sendRawData(cmd_text))
                        {
                            loadingStarted = false;
                            commandStatus = RequestStatusWaiting;
                        }
                        else
                        {
                            commandResponse = "Failed to send command.";
                            commandStatus = RequestStatusRequestError;
                        }
                    }
                    else
                    {
                        commandStatus = RequestStatusNotStarted;
                    }
                }
                else if (lastInput != InputKeyMAX)
                {
                    shouldDebounce = true;
                }
            }
            break;
        }
        if (commandStatus == RequestStatusWaiting ||
            commandStatus == RequestStatusSuccess ||
            commandStatus == RequestStatusRequestError)
        {
            if (lastInput == InputKeyBack)
            {
                shouldDebounce = true;
                commandStatus = RequestStatusNotStarted;
                commandResponse.clear();
                commandResponseScrollOffset = 0;
                commandResponseMaxScroll = 0;
            }
            else if (lastInput == InputKeyDown)
            {
                if (commandResponseScrollOffset < commandResponseMaxScroll)
                    commandResponseScrollOffset++;
                shouldDebounce = true;
            }
            else if (lastInput == InputKeyUp)
            {
                if (commandResponseScrollOffset > 0)
                    commandResponseScrollOffset--;
                shouldDebounce = true;
            }
            break;
        }
        // Normal command list navigation
        switch (lastInput)
        {
        case InputKeyRight:
            commandIndex = (commandIndex < 14) ? commandIndex + 1 : 0; // 15 commands (0–14)
            shouldDebounce = true;
            break;
        case InputKeyLeft:
            commandIndex = (commandIndex > 0) ? commandIndex - 1 : 14;
            shouldDebounce = true;
            break;
        case InputKeyOk:
            shouldDebounce = true;
            if (commandIndex == 0)
            {
                // [CUSTOM] — open keyboard for freeform input
                if (!keyboard)
                {
                    keyboard = std::make_unique<Keyboard>();
                }
                if (keyboard)
                {
                    keyboard->clearText();
                    keyboard->setText("");
                }
                commandStatus = RequestStatusKeyboard;
                keyboardSkipEvents = 2;
            }
            else
            {
                // Dispatch the selected built-in command
                FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
                furi_check(app);
                app->clearHttpResponse();
                bool sent = false;
                bool has_response = true;
                switch (commandIndex)
                {
                case 1: 
                    sent = app->sendHttpCommand(HTTP_CMD_PING); 
                    break;
                case 2: 
                    sent = app->sendRawData("[BOARD/NAME]"); 
                    break; // firmware v2.1.4, raw string
                case 3: 
                    sent = app->sendHttpCommand(HTTP_CMD_VERSION); 
                    break;
                case 4: 
                    sent = app->sendHttpCommand(HTTP_CMD_LIST_COMMANDS); 
                    break;
                case 5: 
                    sent = app->sendHttpCommand(HTTP_CMD_STATUS); 
                    break;
                case 6: 
                    sent = app->sendHttpCommand(HTTP_CMD_SSID); 
                    break;
                case 7: 
                    sent = app->sendHttpCommand(HTTP_CMD_WIFI_LIST); 
                    break;
                case 8: 
                    sent = app->sendHttpCommand(HTTP_CMD_IP_ADDRESS); 
                    break;
                case 9: 
                    sent = app->sendHttpCommand(HTTP_CMD_IP_WIFI); 
                    break;
                case 10: 
                    sent = app->sendHttpCommand(HTTP_CMD_WIFI_CONNECT); 
                    break;
                case 11: 
                    sent = app->sendHttpCommand(HTTP_CMD_WIFI_DISCONNECT); 
                    break;
                case 12: 
                    sent = app->sendHttpCommand(HTTP_CMD_LED_ON); has_response = false; 
                    break;  // no response expected
                case 13: 
                    sent = app->sendHttpCommand(HTTP_CMD_LED_OFF); has_response = false; 
                    break; // no response expected
                case 14: 
                    sent = app->sendHttpCommand(HTTP_CMD_REBOOT); has_response = false; 
                    break;  // no response expected
                default: break;
                }
                if (!sent)
                {
                    commandResponse = "Failed to send command.";
                    commandStatus = RequestStatusRequestError;
                    commandResponseScrollOffset = 0;
                    commandResponseMaxScroll = 0;
                }
                else if (!has_response)
                {
                    // Commands like REBOOT, LED/ON, LED/OFF don't send a response back
                    commandResponse = "Command sent.";
                    commandStatus = RequestStatusSuccess;
                    commandResponseScrollOffset = 0;
                    commandResponseMaxScroll = 0;
                }
                else
                {
                    loadingStarted = false;
                    commandStatus = RequestStatusWaiting;
                }
            }
            break;
        case InputKeyBack:
            shouldDebounce = true;
            currentView = AppViewMainMenu;
            break;
        default:
            break;
        }
        break;

    // ── All other views (Status, Connect, SaveWiFi) ───────────────────────────
    default:
        if (lastInput == InputKeyBack)
        {
            shouldDebounce = true;
            if (currentView == AppViewStatus)
            {
                connectionType = ConnectionTypeConnection;
            }
            currentView = AppViewMainMenu;
        }
        break;
    };
}


// ─────────────────────────────────────────────
//  Draw — Saved APs views
// ─────────────────────────────────────────────

void FlipperHTTPRun::drawSavedAPsView(Canvas *canvas)
{
    canvas_clear(canvas);

    // Keyboard is active — draw it instead of the list
    if (savedAPsStatus == RequestStatusKeyboard)
    {
        if (keyboard)
        {
            const char *header = (keyboardFlow == KeyboardFlowSavedAPAddSSID)
                                     ? "Enter SSID:"
                                     : "Enter password:";
            keyboard->draw(canvas, header);
        }
        return;
    }

    // Build a label array: [Add Network] followed by saved SSIDs
    const char *items[MAX_SAVED_APS + 1];
    size_t total = 1;
    items[0] = "[Add Network]";
    if (playlist)
    {
        for (size_t i = 0; i < playlist->count; i++)
        {
            items[i + 1] = playlist->ssids[i];
        }
        total = playlist->count + 1;
    }
    drawMenu(canvas, (uint8_t)savedAPIndex, items, (uint8_t)total);
}

void FlipperHTTPRun::drawSavedAPDetailView(Canvas *canvas)
{
    canvas_clear(canvas);

    // Guard: need a valid playlist entry (index 0 is [Add Network])
    if (!playlist || savedAPIndex == 0 || (savedAPIndex - 1) >= playlist->count)
    {
        return;
    }

    const size_t idx = savedAPIndex - 1;

    // Keyboard active — editing password
    if (savedAPDetailStatus == RequestStatusKeyboard)
    {
        if (keyboard)
        {
            keyboard->draw(canvas, "Edit password:");
        }
        return;
    }

    canvas_set_font(canvas, FontPrimary);

    // Waiting for board response to a Set command
    if (savedAPDetailStatus == RequestStatusWaiting)
    {
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Setting WiFi...");
            }
        }
        if (!httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
            return;
        }
        if (loading)
        {
            loading->stop();
        }
        loadingStarted = false;
        FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
        const char *response = app->getHttpResponse();
        savedAPDetailStatus = (response && strstr(response, "[SUCCESS]") != nullptr)
                                  ? RequestStatusSuccess
                                  : RequestStatusRequestError;
        return;
    }

    if (savedAPDetailStatus == RequestStatusSuccess)
    {
        canvas_draw_str(canvas, 0, 10, playlist->ssids[idx]);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 26, "WiFi credentials sent.");
        canvas_draw_str(canvas, 0, 36, "All FlipperHTTP apps");
        canvas_draw_str(canvas, 0, 46, "will use this network.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        return;
    }

    if (savedAPDetailStatus == RequestStatusRequestError)
    {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 20, "Failed to set WiFi.");
        canvas_draw_str(canvas, 0, 30, "Check board connection.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        return;
    }

    // Normal state — show AP info and button hints
    canvas_draw_str(canvas, 0, 10, playlist->ssids[idx]);
    canvas_set_font(canvas, FontSecondary);
    char pass_line[80];
    snprintf(pass_line, sizeof(pass_line), "Pass: %s", playlist->passwords[idx]);
    canvas_draw_str(canvas, 0, 22, pass_line);
    canvas_draw_str(canvas, 0, 63, "<Del [Back] [OK]=Set >Edit");
}

// ─────────────────────────────────────────────
//  Draw — Commands view
// ─────────────────────────────────────────────

void FlipperHTTPRun::drawCommandsView(Canvas *canvas)
{
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Keyboard active — entering a custom command
    if (commandStatus == RequestStatusKeyboard)
    {
        if (keyboard)
        {
            keyboard->draw(canvas, "Enter command:");
        }
        return;
    }

    // Waiting for board response
    if (commandStatus == RequestStatusWaiting)
    {
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Sending...");
            }
        }
        if (!httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
            return;
        }
        if (loading)
        {
            loading->stop();
        }
        loadingStarted = false;
        FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
        const char *response = app->getHttpResponse();
        commandResponse = (response && strlen(response) > 0) ? response : "No response.";
        commandStatus = RequestStatusSuccess;
        commandResponseScrollOffset = 0;
        commandResponseMaxScroll = 0;
        return;
    }

    // Show the response (success or error)
    if (commandStatus == RequestStatusSuccess || commandStatus == RequestStatusRequestError)
    {
        canvas_draw_str(canvas, 0, 10, "Response:");
        canvas_set_font(canvas, FontSecondary);

        // Split the full response into wrapped lines of ~21 chars (FontSecondary, 128px wide)
        // then render a window of 4 lines starting at commandResponseScrollOffset.
        const int CHARS_PER_LINE = 21;
        const int LINES_VISIBLE = 4;
        const int LINE_H = 10;

        // Build all lines by word-wrapping the response
        std::vector<std::string> lines;
        const std::string &resp = commandResponse;
        size_t pos = 0;
        while (pos < resp.size())
        {
            size_t end = pos + CHARS_PER_LINE;
            if (end >= resp.size())
            {
                lines.push_back(resp.substr(pos));
                break;
            }
            // Look for a space to break on within the window
            size_t break_at = resp.rfind(' ', end);
            if (break_at == std::string::npos || break_at <= pos)
            {
                break_at = end; // no space — hard break
            }
            lines.push_back(resp.substr(pos, break_at - pos));
            pos = break_at + (resp[break_at] == ' ' ? 1 : 0);
        }

        // Clamp scroll offset
        size_t max_scroll = lines.size() > (size_t)LINES_VISIBLE
                                ? lines.size() - (size_t)LINES_VISIBLE
                                : 0;
        commandResponseMaxScroll = max_scroll; // keep input handler in sync
        if (commandResponseScrollOffset > max_scroll)
        {
            commandResponseScrollOffset = max_scroll;
        }

        // Render LINES_VISIBLE lines starting at the scroll offset
        for (int i = 0; i < LINES_VISIBLE; i++)
        {
            size_t line_idx = commandResponseScrollOffset + (size_t)i;
            if (line_idx >= lines.size()) break;
            canvas_draw_str(canvas, 0, 22 + i * LINE_H, lines[line_idx].c_str());
        }

        // Up arrow — filled triangle, tip at top, drawn with horizontal scanlines
        if (commandResponseScrollOffset > 0)
        {
            canvas_draw_dot(canvas, 124, 22);
            canvas_draw_line(canvas, 123, 23, 125, 23);
            canvas_draw_line(canvas, 122, 24, 126, 24);
            canvas_draw_line(canvas, 121, 25, 127, 25);
        }

        // Down arrow — exact mirror: same scanlines, y-coordinates flipped
        if (commandResponseScrollOffset < max_scroll)
        {
            canvas_draw_line(canvas, 121, 50, 127, 50);
            canvas_draw_line(canvas, 122, 51, 126, 51);
            canvas_draw_line(canvas, 123, 52, 125, 52);
            canvas_draw_dot(canvas, 124, 53);
        }

        if (max_scroll > 0)
        {
            canvas_draw_str(canvas, 0, 63, "Up/Dn scroll Back=return");
        }
        else
        {
            canvas_draw_str(canvas, 0, 63, "Press Back to return.");
        }
        return;
    }

    // Normal state — show the command list
    const char *labels[] = {
        "[CUSTOM]",
        "PING",
        "BOARD/NAME",
        "VERSION",
        "LIST",
        "WIFI/STATUS",
        "WIFI/SSID",
        "WIFI/LIST",
        "IP/ADDRESS",
        "WIFI/IP",
        "WIFI/CONNECT",
        "WIFI/DISCONNECT",
        "LED/ON",
        "LED/OFF",
        "REBOOT",
    };
    const uint8_t label_count = sizeof(labels) / sizeof(labels[0]);
    drawMenu(canvas, (uint8_t)commandIndex, labels, label_count);
}


void FlipperHTTPRun::userRequest(RequestType requestType)
{
    // Get app context to access HTTP functionality
    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
    furi_check(app);

    app->clearHttpResponse();

    switch (requestType)
    {
    case RequestTypeStatusConnection:
        if (!app->sendHttpCommand(HTTP_CMD_STATUS))
        {
            statusStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeStatusSSID:
        if (!app->sendHttpCommand(HTTP_CMD_SSID))
        {
            statusStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeStatusIP:
        if (!app->sendHttpCommand(HTTP_CMD_IP_ADDRESS))
        {
            statusStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeConnect:
        if (!app->sendHttpCommand(HTTP_CMD_WIFI_CONNECT))
        {
            connectStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeScan:
        if (!app->sendHttpCommand(HTTP_CMD_SCAN))
        {
            scanStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeSaveWiFi:
    {
        char wifi_ssid[64] = {0};
        char wifi_pass[64] = {0};
        if (!app->loadChar("wifi_ssid", wifi_ssid, sizeof(wifi_ssid)))
        {
            FURI_LOG_E(TAG, "Failed to load wifi_ssid");
            connectStatus = RequestStatusRequestError;
            break;
        }
        if (!app->loadChar("wifi_pass", wifi_pass, sizeof(wifi_pass)))
        {
            FURI_LOG_E(TAG, "Failed to load wifi_pass");
            connectStatus = RequestStatusRequestError;
            break;
        }
        if (!app->sendWiFiCredentials(wifi_ssid, wifi_pass))
        {
            FURI_LOG_E(TAG, "Failed to send WiFi credentials");
            connectStatus = RequestStatusRequestError;
        }

        break;
    }
    default:
        FURI_LOG_E(TAG, "Unknown request type: %d", requestType);
        statusStatus = RequestStatusRequestError;
        connectStatus = RequestStatusRequestError;
        scanStatus = RequestStatusRequestError;
        break;
    }
}
