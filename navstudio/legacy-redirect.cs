// Kleiner Weiterleiter am alten dist-Pfad. Keine zweite Studio-Installation.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

internal static class NavStudioRedirect
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int MessageBox(IntPtr owner, string text, string title, uint flags);

    // Windows-Argumentquotierung, ohne Shell oder zusammengesetzte Befehle.
    private static string Quote(string value)
    {
        StringBuilder result = new StringBuilder("\"");
        int slashes = 0;
        foreach (char c in value)
        {
            if (c == '\\') { slashes++; continue; }
            if (c == '"') { result.Append('\\', slashes * 2 + 1); result.Append(c); }
            else { result.Append('\\', slashes); result.Append(c); }
            slashes = 0;
        }
        result.Append('\\', slashes * 2);
        result.Append('"');
        return result.ToString();
    }

    private static void CheckFiles(string root, object records)
    {
        string prefix = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        object[] files = records as object[];
        if (files == null || files.Length == 0) throw new InvalidDataException("Das Desktop-Paketmanifest ist unvollstÃ¤ndig.");
        foreach (object value in files)
        {
            Dictionary<string, object> record = value as Dictionary<string, object>;
            if (record == null) throw new InvalidDataException("UngÃ¼ltiger Manifest-Eintrag.");
            string relative = Convert.ToString(record["path"]), expected = Convert.ToString(record["sha256"]);
            if (Path.IsPathRooted(relative) || relative.Contains("\\") || relative.Contains(":"))
                throw new InvalidDataException("UngÃ¼ltiger relativer Paketpfad.");
            foreach (string part in relative.Split('/'))
                if (part == ".." || part == "." || part.Length == 0) throw new InvalidDataException("UngÃ¼ltiger relativer Paketpfad.");
            string filename = Path.GetFullPath(Path.Combine(root, relative));
            if (!filename.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) || !File.Exists(filename))
                throw new FileNotFoundException("Desktop-Paketdatei fehlt: " + relative);
            string actual;
            using (SHA256 digest = SHA256.Create())
            using (FileStream stream = File.OpenRead(filename))
                actual = BitConverter.ToString(digest.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
            if (!String.Equals(actual, expected, StringComparison.Ordinal))
                throw new InvalidDataException("NavStudio-Paket ist veraltet oder verÃ¤ndert: " + relative + "\nBitte das aktuelle Desktop-Paket bauen/installieren oder navstudio/Start.cmd starten.");
        }
    }

    private static void CheckPackage(string target, string workspace)
    {
        string root = Path.GetDirectoryName(target), manifest = Path.Combine(root, "navstudio-package.json");
        if (!File.Exists(manifest)) throw new FileNotFoundException("Dieses NavStudio-Paket ist veraltet. Bitte das aktuelle Desktop-Paket bauen/installieren oder navstudio/Start.cmd starten.");
        Dictionary<string, object> value = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(manifest));
        if (Convert.ToInt32(value["schema"]) != 1 || Convert.ToString(value["entry_kind"]) != "desktop-projects" || Convert.ToString(value["package_name"]) + ".exe" != Path.GetFileName(target))
            throw new InvalidDataException("Ein aktuelles Desktop-Paket mit Projekt-Wizard wird benÃ¶tigt.");
        CheckFiles(root, value["package_files"]);
        if (File.Exists(Path.Combine(workspace, "navstudio", "entry.py"))) CheckFiles(workspace, value["source_files"]);
    }

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            string location = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
            string workspace = Path.GetFullPath(Path.Combine(location, "..", ".."));
            string target = Path.Combine(workspace, "build", "navgen-overkill", "TCE2-NavStudio.exe");
            if (!File.Exists(target))
                throw new FileNotFoundException("NavStudio liegt jetzt zusammen mit der Testinstanz. Erwarteter Pfad:\n" + target);
            CheckPackage(target, workspace);
            StringBuilder arguments = new StringBuilder();
            bool hasWorkspace = false;
            foreach (string arg in args)
            {
                if (arg == "--workspace" || arg.StartsWith("--workspace=", StringComparison.Ordinal)) hasWorkspace = true;
                if (arguments.Length > 0) arguments.Append(' ');
                arguments.Append(Quote(arg));
            }
            if (!hasWorkspace)
            {
                if (arguments.Length > 0) arguments.Append(' ');
                arguments.Append("--workspace ");
                arguments.Append(Quote(workspace));
            }
            Process.Start(new ProcessStartInfo(target, arguments.ToString()) {
                UseShellExecute = false, CreateNoWindow = true,
                WorkingDirectory = Path.GetDirectoryName(target)
            });
            return 0;
        }
        catch (Exception error)
        {
            MessageBox(IntPtr.Zero, error.Message, "TCE2 NavStudio", 0x10);
            return 1;
        }
    }
}
