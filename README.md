# foo_folderopen

Native foobar2000 component prototype.

## Intended behavior

When Windows Explorer opens one audio file in foobar2000:

1. foobar2000 starts the selected file normally.
2. `foo_folderopen` notices that the active playlist contains exactly one item.
3. It scans that item's folder.
4. It replaces the one-item playlist with the other audio files in that folder.
5. It keeps the originally opened file selected and starts it again.

No helper EXE is required.

## Prototype limitation

Version 0.1 intentionally uses a conservative list of common audio extensions.
It also expands any *one-item active playlist when playback begins*, not only
Explorer launches. That distinction can be tightened later if needed.

## Build

The GitHub Actions workflow downloads the official foobar2000 SDK 2025-03-07
and builds an x64 component with Visual Studio/MSBuild.

The build artifact is:

`foo_folderopen.fb2k-component`

## Test

Install the component in:

`foobar2000 > File > Preferences > Components > Install...`

Restart foobar2000.

Then double-click a song in Windows Explorer from a folder containing several
audio files. The entire folder should appear in the playlist, while the song
you double-clicked remains the playing track.
