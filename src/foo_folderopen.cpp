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

    const int needed = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (needed <= 1) return {};

    std::wstring out(static_cast<size_t>(needed), L'\0');

    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), needed) <= 0) {
        return {};
    }

    out.resize(static_cast<size_t>(needed - 1));
    return out;
}

std::string wide_to_utf8(const std::wstring& s) {
    if (s.empty()) return {};

    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr
    );

    if (needed <= 1) return {};

    std::string out(static_cast<size_t>(needed), '\0');

    if (WideCharToMultiByte(
            CP_UTF8,
            0,
            s.c_str(),
            -1,
            out.data(),
            needed,
            nullptr,
            nullptr) <= 0) {
        return {};
    }

    out.resize(static_cast<size_t>(needed - 1));
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

std::wstring foobar_path_to_local(const std::wstring& input) {
    std::wstring path = input;

    if (path.rfind(L"file://", 0) == 0) {
        path.erase(0, 7);

        // file:///D:/Music/... -> D:/Music/...
        while (path.size() >= 3 &&
               path[0] == L'/' &&
               !(iswalpha(path[1]) && path[2] == L':')) {
            path.erase(path.begin());
        }
    }

    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
}

std::vector<std::wstring> enumerate_audio_files(const std::wstring& folder) {
    std::vector<std::wstring> result;

    std::wstring pattern = folder;
    if (!pattern.empty() && pattern.back() != L'\\') pattern += L'\\';
    pattern += L"*";

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);

    if (h == INVALID_HANDLE_VALUE) {
        return result;
    }

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        std::wstring name = fd.cFileName;

        if (!is_audio_extension(file_extension(name))) {
            continue;
        }

        std::wstring full = folder;

        if (!full.empty() && full.back() != L'\\') {
            full += L'\\';
        }

        full += name;
        result.push_back(std::move(full));

    } while (FindNextFileW(h, &fd));

    FindClose(h);

    std::sort(
        result.begin(),
        result.end(),
        [](const std::wstring& a, const std::wstring& b) {
            return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
        }
    );

    return result;
}

void preload_metadata_later(metadb_handle_list handles) {
    // We are called from a playback callback. The SDK recommends deferring
    // operations which can dispatch callbacks / start UI work until after
    // the current global callback has returned.
    fb2k::inMainThread([handles]() mutable {
        try {
            static_api_ptr_t<metadb_io_v2> metaio;

            metaio->load_info_async(
                handles,
                metadb_io::load_info_default,
                nullptr,
                metadb_io_v2::op_flag_background |
                    metadb_io_v2::op_flag_delay_ui |
                    metadb_io_v2::op_flag_silent,
                nullptr
            );

            console::print("foo_folderopen: metadata preload requested");
        }
        catch (...) {
            console::print("foo_folderopen: metadata preload failed");
        }
    });
}

class folderopen_callback : public play_callback_impl_base {
public:
    folderopen_callback()
        : play_callback_impl_base(play_callback::flag_on_playback_new_track) {
        console::print("foo_folderopen: playback callback registered");
    }

    void on_playback_new_track(metadb_handle_ptr track) override {
        console::print("foo_folderopen: on_playback_new_track fired");

        if (g_expanding || track.is_empty()) {
            return;
        }

        static_api_ptr_t<playlist_manager> pm;

        const t_size playlist = pm->get_active_playlist();

        if (playlist == pfc::infinite_size) {
            console::print("foo_folderopen: no active playlist");
            return;
        }

        const t_size itemCount = pm->playlist_get_item_count(playlist);

        console::printf(
            "foo_folderopen: item count = %u",
            static_cast<unsigned>(itemCount)
        );

        // Only expand the Explorer-style case where foobar has received
        // one file and created/replaced the active playlist with that item.
        if (itemCount != 1) {
            console::print("foo_folderopen: not a one-item playlist; ignoring");
            return;
        }

        metadb_handle_ptr only;

        if (!pm->playlist_get_item_handle(only, playlist, 0) ||
            only.is_empty()) {
            console::print("foo_folderopen: could not read playlist item");
            return;
        }

        if (strcmp(track->get_path(), only->get_path()) != 0) {
            console::print(
                "foo_folderopen: playing item does not match playlist item"
            );
            return;
        }

        console::printf(
            "foo_folderopen: raw path = %s",
            track->get_path()
        );

        const std::wstring rawPath = utf8_to_wide(track->get_path());

        if (rawPath.empty()) {
            return;
        }

        // Ignore streams, but allow local file:// paths.
        if (rawPath.find(L"://") != std::wstring::npos &&
            rawPath.rfind(L"file://", 0) != 0) {
            console::print("foo_folderopen: non-local URI; ignoring");
            return;
        }

        const std::wstring localPath = foobar_path_to_local(rawPath);
        const std::wstring folder = parent_folder(localPath);

        if (folder.empty()) {
            return;
        }

        const std::string folderUtf8 = wide_to_utf8(folder);

        console::printf(
            "foo_folderopen: folder = %s",
            folderUtf8.c_str()
        );

        const auto files = enumerate_audio_files(folder);

        console::printf(
            "foo_folderopen: found %u files",
            static_cast<unsigned>(files.size())
        );

        if (files.size() <= 1) {
            return;
        }

        metadb_handle_list handles;
        t_size clickedIndex = pfc::infinite_size;

        static_api_ptr_t<metadb> db;

        for (const auto& file : files) {
            const std::string utf8 = wide_to_utf8(file);

            if (utf8.empty()) {
                continue;
            }

            metadb_handle_ptr h;

            db->handle_create(
                h,
                make_playable_location(utf8.c_str(), 0)
            );

            if (h.is_empty()) {
                continue;
            }

            if (_wcsicmp(file.c_str(), localPath.c_str()) == 0) {
                clickedIndex = handles.get_count();
            }

            handles.add_item(h);
        }

        console::printf(
            "foo_folderopen: created %u playlist handles",
            static_cast<unsigned>(handles.get_count())
        );

        if (handles.get_count() <= 1 ||
            clickedIndex == pfc::infinite_size) {
            console::print(
                "foo_folderopen: could not build folder playlist"
            );
            return;
        }

        g_expanding = true;

        try {
            pm->activeplaylist_undo_backup();

            pm->playlist_remove_items(
                playlist,
                bit_array_true()
            );

            pm->playlist_insert_items(
                playlist,
                0,
                handles,
                bit_array_false()
            );

            pm->playlist_set_focus_item(
                playlist,
                clickedIndex
            );

            // Clear selection.
            pm->playlist_set_selection(
                playlist,
                bit_array_true(),
                bit_array_false()
            );

            // Select only the originally clicked track.
            pm->playlist_set_selection(
                playlist,
                bit_array_one(clickedIndex),
                bit_array_true()
            );

            // Do not call playlist_execute_default_action() here.
            // The clicked track is already playing. Starting playback again
            // from inside on_playback_new_track() caused the earlier crash.
            console::printf(
                "foo_folderopen: folder loaded; keeping current playback at item %u",
                static_cast<unsigned>(clickedIndex)
            );

            // Ask foobar to read Artist / Album / Title / Duration after
            // this playback callback has returned.
            preload_metadata_later(handles);
        }
        catch (...) {
            console::print(
                "foo_folderopen: exception while expanding folder"
            );
        }

        g_expanding = false;
    }
};

folderopen_callback* g_callback = nullptr;

class folderopen_initquit : public initquit {
public:
    void on_init() override {
        console::print(
            "foo_folderopen: component initialized"
        );

        if (g_callback == nullptr) {
            g_callback = new folderopen_callback();
        }
    }

    void on_quit() override {
        console::print(
            "foo_folderopen: component shutting down"
        );

        delete g_callback;
        g_callback = nullptr;
    }
};

static initquit_factory_t<folderopen_initquit> g_initquit_factory;

} // namespace

DECLARE_COMPONENT_VERSION(
    "Folder Open",
    "0.2.2",
    "When playback begins from a one-item local playlist, "
    "populate the playlist with audio files from the same folder, "
    "keep the originally selected file playing, and preload metadata "
    "for the other folder tracks.\n\n"
    "Prototype for foobar2000 v2.x."
);

VALIDATE_COMPONENT_FILENAME("foo_folderopen.dll");
