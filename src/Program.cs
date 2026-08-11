using System.Diagnostics;
using Microsoft.Win32;

namespace FolderOpen;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Length < 1)
        {
            MessageBox("Drag an audio file onto FolderOpen.exe, or associate audio files with it.");
            return 2;
        }

        string clicked = Path.GetFullPath(args[0].Trim('"'));
        if (!File.Exists(clicked))
        {
            MessageBox($"File not found:\n{clicked}");
            return 3;
        }

        string? foobar = FindFoobar();
        if (foobar is null)
        {
            MessageBox("foobar2000.exe was not found.\n\nSet the FOLDEROPEN_FOOBAR environment variable to its full path.");
            return 4;
        }

        string folder = Path.GetDirectoryName(clicked)!;

        // Stage 1: ask foobar2000 to open the containing folder.
        // Per foobar's CLI behavior, opening a folder replaces the active
        // playlist and begins playback.
        StartFoobar(foobar, Quote(folder));

        // Stage 2: invoke the context-menu Play command on the file that
        // Explorer originally opened. This is the feasibility test:
        // on a normal foobar install it should switch playback to this file
        // while leaving the folder playlist populated.
        Thread.Sleep(GetDelayMs());
        StartFoobar(foobar, $"/context_command:\"Play\" {Quote(clicked)}");

        return 0;
    }

    private static int GetDelayMs()
    {
        var value = Environment.GetEnvironmentVariable("FOLDEROPEN_DELAY_MS");
        return int.TryParse(value, out int ms) && ms >= 0 ? ms : 800;
    }

    private static void StartFoobar(string exe, string arguments)
    {
        Process.Start(new ProcessStartInfo
        {
            FileName = exe,
            Arguments = arguments,
            UseShellExecute = false
        });
    }

    private static string? FindFoobar()
    {
        string? env = Environment.GetEnvironmentVariable("FOLDEROPEN_FOOBAR");
        if (!string.IsNullOrWhiteSpace(env) && File.Exists(env)) return env;

        string[] candidates =
        {
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "foobar2000", "foobar2000.exe"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "foobar2000", "foobar2000.exe"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "foobar2000", "foobar2000.exe")
        };
        foreach (var p in candidates)
            if (File.Exists(p)) return p;

        // Common App Paths registration, if present.
        foreach (var hive in new[] { Registry.CurrentUser, Registry.LocalMachine })
        {
            using var key = hive.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\foobar2000.exe");
            if (key?.GetValue(null) is string p && File.Exists(p)) return p;
        }
        return null;
    }

    private static string Quote(string s) => "\"" + s.Replace("\"", "\\\"") + "\"";

    private static void MessageBox(string text)
    {
        // Avoid a WinForms dependency in the prototype.
        Process.Start(new ProcessStartInfo
        {
            FileName = "powershell.exe",
            Arguments = $"-NoProfile -Command \"Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('{text.Replace("'", "''")}','FolderOpen')\"",
            UseShellExecute = false,
            CreateNoWindow = true
        })?.WaitForExit();
    }
}
