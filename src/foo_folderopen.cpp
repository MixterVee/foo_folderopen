#include <foobar2000/SDK/foobar2000.h>
#include <windows.h>
#include <shlwapi.h>
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")

namespace {

bool g_expanding = false;

// First prototype: local audio formats commonly handled by foobar2000.
// We can later replace this list with foobar's input-service enumeration.
bool is_audio_extension(const std::wstring& ext) {
    static const wchar_t* exts[] = {
        L".mp3", L".flac", L".wav", L".wave", L".aiff", L".aif",
        L".m4a", L".mp4", L".aac", L".ogg", L".oga", L".opus",
        L".wv", L".mpc", L".ape", L".tak", L".tta", L".weba"
    };
    for (auto* e : exts) {
        if (_wcsicmp(ext.c_str(), e) == 0) return true;
    }
    return false;
}

std::wstring utf8_to_wide(const char* s) {
    if (!s || !*s) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring out((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    return out;
}

std::string wide_to_utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring parent_folder(const std::wstring& path) {
    const auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return {};
    return path.substr(0, pos);
}

std::wstring file_extension(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    const auto dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return {};
    if (slash != std::wstring::npos && dot < slash) return {};
    return path.substr(dot);
}

std::vector<std::wstring> enumerate_audio_files(const std::wstring& folder) {
    std::vector<std::wstring> result;
    std::wstring pattern = folder;
    if (!pattern.empty() && pattern.back() != L'\\') pattern += L'\\';
    pattern += L"*";

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return result;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring name = fd.cFileName;
        if (!is_audio_extension(file_extension(name))) continue;

        std::wstring full = folder;
        if (!full.empty() && full.back() != L'\\') full += L'\\';
        full += name;
        result.push_back(std::move(full));
    } while (FindNextFileW(h, &fd));

    FindClose(h);

    std::sort(result.begin(), result.end(), [](const std::wstring& a, const std::wstring& b) {
        return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
    });
    return result;
}

class folderopen_callback final : public play_callback_static {
public:
    unsigned get_flags() override {
        return flag_on_playback_new_track;
    }

    void on_playback_new_track(metadb_handle_ptr track) override {
        if (g_expanding || track.is_empty()) return;

        static_api_ptr_t<playlist_manager> pm;
        const t_size playlist = pm->get_active_playlist();
        if (playlist == pfc::infinite_size) return;

        // The key heuristic: Explorer opening one file creates/replaces a playlist
        // containing just that one item, then starts it. We only expand that case.
        if (pm->playlist_get_item_count(playlist) != 1) return;

        metadb_handle_ptr only;
        if (!pm->playlist_get_item_handle(only, playlist, 0) || only.is_empty()) return;

        // Ensure the callback is reacting to that single playlist item.
        if (strcmp(track->get_path(), only->get_path()) != 0) return;

        const std::wstring clicked = utf8_to_wide(track->get_path());
        if (clicked.empty()) return;

        // Skip streams / non-local paths.
        if (clicked.find(L"://") != std::wstring::npos) return;

        const std::wstring folder = parent_folder(clicked);
        if (folder.empty()) return;

        const auto files = enumerate_audio_files(folder);
        if (files.size() <= 1) return;

        metadb_handle_list handles;
        t_size clickedIndex = pfc::infinite_size;

        static_api_ptr_t<metadb> db;
        for (const auto& file : files) {
            const std::string utf8 = wide_to_utf8(file);
            if (utf8.empty()) continue;

            metadb_handle_ptr h;
            db->handle_create(h, make_playable_location(utf8.c_str(), 0));
            if (h.is_empty()) continue;

            if (_wcsicmp(file.c_str(), clicked.c_str()) == 0) {
                clickedIndex = handles.get_count();
            }
            handles.add_item(h);
        }

        if (handles.get_count() <= 1 || clickedIndex == pfc::infinite_size) return;

        g_expanding = true;
        try {
            pm->activeplaylist_undo_backup();

            pm->playlist_remove_items(playlist, bit_array_true());
            pm->playlist_insert_items(
                playlist,
                0,
                handles,
                bit_array_false()
            );

            pm->playlist_set_focus_item(playlist, clickedIndex);
            pm->playlist_set_selection(playlist, bit_array_true(), false);
            pm->playlist_set_selection(playlist, bit_array_one(clickedIndex), true);

            // Same action foobar normally performs when a playlist row is double-clicked.
            pm->playlist_execute_default_action(playlist, clickedIndex);
        }
        catch (...) {
            console::print("foo_folderopen: failed while expanding the folder.");
        }
        g_expanding = false;
    }

    void on_playback_starting(play_control::t_track_command, bool) override {}
    void on_playback_stop(play_control::t_stop_reason) override {}
    void on_playback_seek(double) override {}
    void on_playback_pause(bool) override {}
    void on_playback_edited(metadb_handle_ptr) override {}
    void on_playback_dynamic_info(const file_info&) override {}
    void on_playback_dynamic_info_track(const file_info&) override {}
    void on_playback_time(double) override {}
    void on_volume_change(float) override {}
};

static play_callback_static_factory_t<folderopen_callback> g_callback_factory;

} // namespace

DECLARE_COMPONENT_VERSION(
    "Folder Open",
    "0.1.0",
    "When a single local audio file is opened and begins playing, "
    "replace that one-item playlist with all audio files from the same folder "
    "and keep playing the originally opened file.\n\n"
    "Prototype for foobar2000 v2.x."
);

VALIDATE_COMPONENT_FILENAME("foo_folderopen.dll");
