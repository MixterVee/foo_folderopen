# FolderOpen prototype

A small Windows launcher for testing the requested foobar2000 workflow:

1. Double-click one audio file in Windows Explorer.
2. FolderOpen passes that file's **containing folder** to foobar2000.
3. foobar2000 populates the playlist with the folder.
4. FolderOpen then asks foobar2000 to run its **Play** context command on the originally clicked file.

The important part of this prototype is step 4. It tests whether the built-in
`/context_command:"Play"` route can start the clicked track while leaving the
folder playlist intact. If it does, no custom foobar SDK component is needed.

## Build automatically on GitHub

Upload this project to your repository. Open the **Actions** tab and run
**Build FolderOpen**, or simply push a commit. When the run finishes, download
the `FolderOpen-win-x64` artifact.

## First test (do NOT change Windows file associations yet)

1. Extract `FolderOpen.exe`.
2. Drag an MP3/FLAC/etc. from a multi-song folder onto `FolderOpen.exe`.
3. Expected result:
   - foobar2000's playlist contains the whole folder;
   - the dragged song becomes the playing song.

If foobar loads the folder but plays the wrong track, try a longer delay:

    set FOLDEROPEN_DELAY_MS=1500
    FolderOpen.exe "D:\Music\Album\05 - Song.flac"

If foobar is installed somewhere unusual:

    set FOLDEROPEN_FOOBAR=C:\Path\To\foobar2000.exe
    FolderOpen.exe "D:\Music\Album\05 - Song.flac"

## Important

This is deliberately a feasibility prototype, not an installer. Do not make it
the default app for your music files until the two-stage behavior is confirmed
on your foobar2000 v2.25.x setup.
