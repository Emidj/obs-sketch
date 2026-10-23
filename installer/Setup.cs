// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Windows.Forms;

internal static class Setup
{
    static string L(string key) { return LocaleText.Text(key); }
    static string Title { get { return L("setup.title"); } }
    static readonly string Target = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "obs-studio", "plugins", "obs-sketch");
    static readonly string[] Files = { "bin/64bit/obs-sketch.dll", "data/LEEME.txt", "data/COPYING.txt" };
    static readonly string[] Resources = { "plugin", "readme", "license" };

    static byte[] ReadResource(string name)
    {
        using (Stream s = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
        using (MemoryStream m = new MemoryStream()) { if (s == null) throw new IOException(L("setup.resource_missing") + name); s.CopyTo(m); return m.ToArray(); }
    }
    static void ValidatePlugin(byte[] bytes)
    {
        string expected = System.Text.Encoding.ASCII.GetString(ReadResource("checksum")).Trim();
        using (SHA256 sha = SHA256.Create()) {
            string actual = BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
            if (actual != expected) throw new IOException(L("setup.checksum_failed"));
        }
    }
    static void InstallTo(string destination)
    {
        byte[][] payloads = { ReadResource("plugin"), ReadResource("readme"), ReadResource("license") };
        ValidatePlugin(payloads[0]);
        string root = Path.GetFullPath(destination) + Path.DirectorySeparatorChar;
        string[] paths = new string[Files.Length];
        byte[][] previous = new byte[Files.Length][];
        for (int i = 0; i < Files.Length; i++) {
            paths[i] = Path.GetFullPath(Path.Combine(destination, Files[i]));
            if (!paths[i].StartsWith(root, StringComparison.OrdinalIgnoreCase)) throw new IOException(L("setup.invalid_path"));
            if (File.Exists(paths[i])) previous[i] = File.ReadAllBytes(paths[i]);
        }
        int written = 0;
        try {
            for (int i = 0; i < paths.Length; i++) {
                Directory.CreateDirectory(Path.GetDirectoryName(paths[i]));
                string temp = paths[i] + ".new";
                try {
                    File.WriteAllBytes(temp, payloads[i]);
                    if (File.Exists(paths[i])) File.Replace(temp, paths[i], null); else File.Move(temp, paths[i]);
                    written++;
                } finally { if (File.Exists(temp)) File.Delete(temp); }
            }
        } catch {
            for (int i = written-1; i >= 0; i--) {
                if (previous[i] != null) File.WriteAllBytes(paths[i], previous[i]); else File.Delete(paths[i]);
            }
            throw;
        }
    }
    static void Uninstall()
    {
        // Only remove the three files belonging to this package. Never recurse.
        foreach (string name in Files) {
            string p = Path.Combine(Target, name);
            if (File.Exists(p)) File.Delete(p);
        }
        foreach (string sub in new[] { "bin/64bit", "bin", "data", "" }) {
            string d = Path.Combine(Target, sub);
            if (Directory.Exists(d) && Directory.GetFileSystemEntries(d).Length == 0) Directory.Delete(d, false);
        }
    }
    static bool IsAdmin()
    {
        return new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator);
    }
    static void Execute(string action)
    {
        if (Process.GetProcessesByName("obs64").Length != 0) throw new IOException(L("setup.close_obs"));
        if (!Environment.Is64BitOperatingSystem) throw new IOException(L("setup.x64"));
        if (!IsAdmin()) {
            ProcessStartInfo start = new ProcessStartInfo(Application.ExecutablePath, action);
            start.UseShellExecute = true; start.Verb = "runas";
            using (Process process = Process.Start(start)) { process.WaitForExit(); if (process.ExitCode != 0) throw new IOException(L("setup.failed")); }
            return;
        }
        if (action == "/uninstall") Uninstall(); else InstallTo(Target);
    }
    [STAThread]
    static int Main(string[] args)
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        try {
            if (args.Length == 2 && args[0] == "/extract") { InstallTo(Path.GetFullPath(args[1])); return 0; }
            if (args.Length == 1 && (args[0] == "/install" || args[0] == "/uninstall")) { Execute(args[0]); return 0; }
            Form f = new Form { Text = Title, ClientSize = new Size(740, 520), StartPosition = FormStartPosition.CenterScreen, FormBorderStyle = FormBorderStyle.FixedDialog, MaximizeBox = false, Font = new Font("Segoe UI", 10) };
            using (MemoryStream iconStream = new MemoryStream(ReadResource("author_icon")))
            using (Icon icon = new Icon(iconStream)) { f.Icon = (Icon)icon.Clone(); }
            PictureBox logo = new PictureBox { Location = new Point(28, 350), Size = new Size(72, 72), SizeMode = PictureBoxSizeMode.Zoom };
            using (MemoryStream logoStream = new MemoryStream(ReadResource("author_logo")))
            using (Image original = Image.FromStream(logoStream)) { logo.Image = new Bitmap(original); }
            Label credit = new Label { Text = "A Plugin By DjEmiMas", Location = new Point(112, 350), Size = new Size(600, 72), TextAlign = ContentAlignment.MiddleLeft, Font = new Font("Segoe UI", 12, FontStyle.Bold) };
            Label title = new Label { Text = L("setup.heading"), Location = new Point(26, 24), Size = new Size(686, 44), Font = new Font("Segoe UI", 18, FontStyle.Bold) };
            Label body = new Label { Text = L("setup.body"), Location = new Point(28, 80), Size = new Size(684, 200) };
            Label path = new Label { Text = L("setup.destination") + Target, Location = new Point(28, 290), Size = new Size(684, 55), Font = new Font("Segoe UI", 9) };
            Button install = new Button { Text = L("setup.install"), Location = new Point(28, 454), Size = new Size(150, 38) };
            Button remove = new Button { Text = L("setup.uninstall"), Location = new Point(190, 454), Size = new Size(150, 38) };
            Button close = new Button { Text = L("setup.close"), Location = new Point(562, 454), Size = new Size(150, 38) };
            install.Click += delegate {
                try { Execute("/install"); MessageBox.Show(L("setup.installed"), Title, MessageBoxButtons.OK, MessageBoxIcon.Information); f.Close(); }
                catch (Exception e) { MessageBox.Show(e.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error); }
            };
            remove.Click += delegate {
                try { Execute("/uninstall"); MessageBox.Show(L("setup.uninstalled"), Title); f.Close(); }
                catch (Exception e) { MessageBox.Show(e.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error); }
            };
            close.Click += delegate { f.Close(); };
            f.Controls.AddRange(new Control[] { title, body, path, logo, credit, install, remove, close });
#if PREVIEW
            if (args.Length == 2 && args[0] == "/preview") {
                f.Opacity = 0; f.ShowInTaskbar = false;
                f.Show(); Application.DoEvents(); f.PerformLayout();
                using (Bitmap bitmap = new Bitmap(f.Width, f.Height)) {
                    f.DrawToBitmap(bitmap, new Rectangle(Point.Empty, bitmap.Size));
                    bitmap.Save(args[1], System.Drawing.Imaging.ImageFormat.Png);
                }
                f.Dispose(); return 0;
            }
#endif
            Application.Run(f); return 0;
        } catch (Exception e) {
            if (args.Length > 0 && args[0] == "/extract") Console.Error.WriteLine(e.Message);
            else MessageBox.Show(e.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}

