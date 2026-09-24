# foo_folderopen

**Folder Open** is a native foobar2000 component for people who browse music in Windows Explorer.

Double-click one audio file and foobar2000 will:

- start the file you selected
- populate the active playlist with the other playable files in the same folder
- keep the originally selected track playing and focused
- sort the folder naturally by filename
- load metadata and durations for the added tracks
- use foobar2000's own registered input services to decide which file formats are supported

No helper EXE is required.

## Requirements

- Windows
- foobar2000 v2.x 64-bit
- Tested with foobar2000 v2.25.10 x64

## Installation

1. Download the latest `foo_folderopen.fb2k-component` from the GitHub Releases page.
2. In foobar2000, open **File > Preferences > Components**.
3. Click **Install...**
4. Select `foo_folderopen.fb2k-component`.
5. Apply the change and restart foobar2000.

## Usage

In Windows Explorer, double-click an audio file that is associated with foobar2000.

Example:

```text
Album Folder
├─ 01 - Track One.flac
├─ 02 - Track Two.flac
├─ 03 - Track Three.flac   <- double-click
├─ 04 - Track Four.flac
└─ 05 - Track Five.flac
```

foobar2000 will play **03 - Track Three.flac** and populate the playlist with the other supported audio files from that folder.

## Notes

Folder Open is designed around the Windows Explorer double-click workflow. Internally, version 1.0.0 expands a local track when playback begins from a one-item active playlist, so other one-item-playlist workflows may also trigger folder expansion.

Files are sorted using Windows natural filename ordering. Metadata is loaded asynchronously after the folder is added.

## Build

The repository includes a GitHub Actions workflow that builds the x64 component against the foobar2000 SDK.

## Version

Current release: **1.0.0**

## License

MIT License. See [LICENSE](LICENSE).
