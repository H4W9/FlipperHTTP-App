// Pure C file — jsmn_furi.h and furi_string_cat use _Generic (C11), valid here.
#include "saved_aps_storage.h"
#include <flipper_http/flipper_http.h>
#include <jsmn/jsmn_furi.h>

#define TAG "FlipperHTTP"

void saved_aps_save(WiFiPlaylist *playlist)
{
    if (!playlist)
    {
        FURI_LOG_E(TAG, "saved_aps_save: playlist is NULL");
        return;
    }

    Storage *storage = furi_record_open(RECORD_STORAGE);
    if (!storage)
    {
        FURI_LOG_E(TAG, "saved_aps_save: Failed to open storage");
        return;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    if (!storage_file_open(file, WIFI_LIST_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E(TAG, "saved_aps_save: Failed to open file for writing");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FuriString *json = furi_string_alloc();
    if (!json)
    {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    furi_string_cat(json, "{\"ssids\":[\n");
    for (size_t i = 0; i < playlist->count; i++)
    {
        furi_string_cat_printf(json, "{\"ssid\":\"%s\",\"password\":\"%s\"}",
                               playlist->ssids[i], playlist->passwords[i]);
        if (i < playlist->count - 1)
        {
            furi_string_cat(json, ",\n");
        }
    }
    furi_string_cat(json, "\n]}");

    size_t len = furi_string_size(json);
    if (storage_file_write(file, furi_string_get_cstr(json), len) != len)
    {
        FURI_LOG_E(TAG, "saved_aps_save: Failed to write JSON");
    }

    furi_string_free(json);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool saved_aps_load(WiFiPlaylist *playlist)
{
    if (!playlist)
    {
        FURI_LOG_E(TAG, "saved_aps_load: playlist is NULL");
        return false;
    }

    playlist->count = 0;

    char path[] = WIFI_LIST_PATH;
    FuriString *json = flipper_http_load_from_file(path);
    if (!json)
    {
        return false;
    }

    for (size_t i = 0; i < MAX_SAVED_APS; i++)
    {
        FuriString *entry = get_json_array_value_furi("ssids", i, json);
        if (!entry)
        {
            break;
        }

        FuriString *ssid = get_json_value_furi("ssid", entry);
        FuriString *pass = get_json_value_furi("password", entry);
        furi_string_free(entry);

        if (!ssid || !pass)
        {
            if (ssid) furi_string_free(ssid);
            if (pass) furi_string_free(pass);
            break;
        }

        snprintf(playlist->ssids[i], MAX_AP_SSID_LENGTH, "%s", furi_string_get_cstr(ssid));
        snprintf(playlist->passwords[i], MAX_AP_SSID_LENGTH, "%s", furi_string_get_cstr(pass));
        playlist->count++;

        furi_string_free(ssid);
        furi_string_free(pass);
    }

    furi_string_free(json);
    return true;
}
