#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <storage/storage.h>

#define WIFI_LIST_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flipper_http/data/wifi_list.txt"
#define MAX_SAVED_APS 25
#define MAX_AP_SSID_LENGTH 64

typedef struct WiFiPlaylist
{
    char ssids[MAX_SAVED_APS][MAX_AP_SSID_LENGTH];
    char passwords[MAX_SAVED_APS][MAX_AP_SSID_LENGTH];
    size_t count;
} WiFiPlaylist;

#ifdef __cplusplus
extern "C"
{
#endif

    // Save the playlist to WIFI_LIST_PATH as JSON
    void saved_aps_save(WiFiPlaylist *playlist);

    // Load the playlist from WIFI_LIST_PATH. Returns false if file is absent or empty.
    bool saved_aps_load(WiFiPlaylist *playlist);

#ifdef __cplusplus
}
#endif
