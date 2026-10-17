using System.Diagnostics;

namespace ScoobyReMasked;

// Scooby-Doo: Re-Masked launcher: set up the player's own disc, pick display options, read the controls, toggle mods,
// and start Play\Scooby-Doo Re-Masked.exe. Settings go to Play\UserData\Config.toml (the keys the game's F10 menu uses).
sealed class MainForm : Form
{
    const string GameExe = "Scooby-Doo Re-Masked.exe";
    static readonly Color Back = Color.FromArgb(24, 28, 22), Panel = Color.FromArgb(36, 44, 32), Accent = Color.FromArgb(124, 196, 64),
        Text1 = Color.FromArgb(236, 236, 224), Text2 = Color.FromArgb(176, 186, 164);

    readonly string root;  // folder holding Play\<GameExe>
    string ConfigPath => Path.Combine(root, "Play", "UserData", "Config.toml");
    string DiscData => Path.Combine(root, "Disc", "DATA");

    readonly Label discStatus = new() { AutoSize = false, Dock = DockStyle.Top, Height = 64, ForeColor = Text1 };
    readonly Button play = MakeButton("PLAY", big: true), chooseDisc = MakeButton("Choose disc image (.iso, .gcm, .ciso)...");
    readonly ProgressBar progress = new() { Dock = DockStyle.Top, Height = 18, Visible = false };

    public MainForm()
    {
        root = FindRoot();
        Text = "Scooby-Doo: Re-Masked";
        try { Icon = Icon.ExtractAssociatedIcon(Environment.ProcessPath!); } catch { }
        BackColor = Back; ForeColor = Text1; Font = new Font("Segoe UI", 10f);
        ClientSize = new Size(760, 560); MinimumSize = new Size(640, 480); StartPosition = FormStartPosition.CenterScreen;

        var header = new Label
        {
            Text = "SCOOBY-DOO: RE-MASKED", Dock = DockStyle.Top, Height = 64, TextAlign = ContentAlignment.MiddleCenter,
            Font = new Font("Segoe UI", 24f, FontStyle.Bold), ForeColor = Accent,
        };
        var sub = new Label
        {
            Text = "Scooby-Doo! Unmasked (GameCube) on PC  -  made by JosiahG", Dock = DockStyle.Top, Height = 26,
            TextAlign = ContentAlignment.TopCenter, ForeColor = Text2,
        };
        var tabs = new TabControl { Dock = DockStyle.Fill, Padding = new Point(16, 6) };
        tabs.TabPages.Add(PlayPage());
        tabs.TabPages.Add(DisplayPage());
        tabs.TabPages.Add(ControlsPage());
        tabs.TabPages.Add(ModsPage());
        Controls.Add(tabs); Controls.Add(sub); Controls.Add(header);
        RefreshDisc();
    }

    // The folder holding Play\<GameExe>: beside the launcher, or (while developing) above launcher\publish.
    static string FindRoot()
    {
        for (var d = new DirectoryInfo(AppContext.BaseDirectory); d != null; d = d.Parent)
            if (File.Exists(Path.Combine(d.FullName, "Play", GameExe))) return d.FullName;
        return AppContext.BaseDirectory;
    }

    static Button MakeButton(string text, bool big = false) => new()
    {
        Text = text, AutoSize = !big, Height = big ? 64 : 36, Dock = DockStyle.Top, FlatStyle = FlatStyle.Flat,
        BackColor = big ? Accent : Panel, ForeColor = big ? Back : Text1, Cursor = Cursors.Hand,
        Font = new Font("Segoe UI", big ? 20f : 10f, FontStyle.Bold), Margin = new Padding(0, 6, 0, 6),
    };

    static TabPage Page(string title) => new(title) { BackColor = Back, ForeColor = Text1, Padding = new Padding(18) };

    static Label Note(string text) => new()
    {
        Text = text, AutoSize = false, Dock = DockStyle.Top, ForeColor = Text2, Height = 20 * (1 + text.Count(c => c == '\n')) + 24,
    };

    // ---------------- Play ----------------
    TabPage PlayPage()
    {
        var page = Page("Play");
        play.Click += (_, _) => Launch();
        chooseDisc.Click += async (_, _) => await ChooseDisc();
        var how = Note(
            "Scooby-Doo: Re-Masked runs your own copy of Scooby-Doo! Unmasked for the GameCube (USA, G5DE78).\n" +
            "Dump the disc you own (for example with CleanRip on a Wii, or from Dolphin's game list: right-click,\n" +
            "Convert File..., ISO or GCZ is not needed: plain .iso or .ciso), then choose the file above.\n" +
            "The launcher extracts it once into the Disc folder next to it; the image file is not needed afterwards.");
        foreach (var c in new Control[] { how, progress, chooseDisc, Spacer(8), discStatus, Spacer(8), play }) page.Controls.Add(c);
        return page;
    }

    static Control Spacer(int h) => new Panel { Dock = DockStyle.Top, Height = h };

    (bool ok, string text) DiscState()
    {
        var boot = Path.Combine(DiscData, "sys", "boot.bin");
        if (!File.Exists(boot)) return (false, "No game disc set up yet. Choose your Scooby-Doo! Unmasked disc image below.");
        var h = File.ReadAllBytes(boot);
        var id = System.Text.Encoding.ASCII.GetString(h, 0, 6);
        return id == GcDisc.WantedId ? (true, "Scooby-Doo! Unmasked (G5DE78) is ready.") : (false, $"The Disc folder holds {id}, not Scooby-Doo! Unmasked USA (G5DE78).");
    }

    void RefreshDisc()
    {
        var (ok, text) = DiscState();
        discStatus.Text = text;
        discStatus.ForeColor = ok ? Accent : Color.FromArgb(240, 180, 90);
        play.Enabled = ok && File.Exists(Path.Combine(root, "Play", GameExe));
        chooseDisc.Text = ok ? "Change disc image..." : "Choose disc image (.iso, .gcm, .ciso)...";
    }

    async Task ChooseDisc()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Choose your Scooby-Doo! Unmasked disc image",
            Filter = "GameCube disc images (*.iso;*.gcm;*.ciso)|*.iso;*.gcm;*.ciso|All files (*.*)|*.*",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        GcDisc disc;
        try { disc = GcDisc.Open(dialog.FileName); }
        catch (Exception e) { MessageBox.Show(this, "That file can't be read: " + e.Message, Text); return; }
        using (disc)
        {
            var problem = disc.Problem();
            if (problem.Length > 0) { MessageBox.Show(this, problem, Text, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            var staging = DiscData + ".new";
            if (Directory.Exists(staging)) Directory.Delete(staging, true);
            chooseDisc.Enabled = play.Enabled = false;
            progress.Visible = true; progress.Value = 0;
            var report = new Progress<(int done, int total, string name)>(p =>
            {
                progress.Maximum = Math.Max(1, p.total); progress.Value = Math.Min(p.done, progress.Maximum);
                discStatus.Text = $"Extracting {p.done}/{p.total}: {p.name}";
            });
            try
            {
                await Task.Run(() => disc.Extract(staging, (d, t, n) => ((IProgress<(int, int, string)>)report).Report((d, t, n))));
                if (Directory.Exists(DiscData)) Directory.Delete(DiscData, true);
                Directory.Move(staging, DiscData);
                var cfg = new GameConfig(ConfigPath);
                cfg.Set("paths", "dvd_root", "'../../Disc/DATA'");  // relative to Play\UserData, so the folder can move
                cfg.Save();
            }
            catch (Exception e) { MessageBox.Show(this, "Extraction failed: " + e.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error); }
            finally { progress.Visible = false; chooseDisc.Enabled = true; RefreshDisc(); }
        }
    }

    void Launch()
    {
        var exe = Path.Combine(root, "Play", GameExe);
        if (!File.Exists(exe)) { MessageBox.Show(this, $"Play\\{GameExe} is missing. Re-extract the download.", Text); return; }
        Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = Path.GetDirectoryName(exe)!, UseShellExecute = true });
        WindowState = FormWindowState.Minimized;
    }

    // ---------------- Display ----------------
    TabPage DisplayPage()
    {
        var page = Page("Display");
        var cfg = new GameConfig(ConfigPath);
        var table = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 2 };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 220));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        void Row(string label, string key, (string text, string raw)[] options, string fallback)
        {
            var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 300, BackColor = Panel, ForeColor = Text1, FlatStyle = FlatStyle.Flat };
            foreach (var o in options) box.Items.Add(o.text);
            var current = cfg.Get("video", key) ?? fallback;
            int index = Array.FindIndex(options, o => o.raw.Trim('"') == current);
            box.SelectedIndex = index >= 0 ? index : Array.FindIndex(options, o => o.raw.Trim('"') == fallback);
            box.SelectedIndexChanged += (_, _) =>
            {
                var c = new GameConfig(ConfigPath);
                c.Set("video", key, options[box.SelectedIndex].raw);
                c.Save();
            };
            table.Controls.Add(new Label { Text = label, AutoSize = true, ForeColor = Text1, Margin = new Padding(0, 8, 0, 8) });
            table.Controls.Add(box);
        }
        Row("Window", "display_mode", new[] { ("Borderless fullscreen", "\"borderless\""), ("Window", "\"windowed\""), ("Exclusive fullscreen", "\"exclusive\"") }, "borderless");
        Row("Resolution", "resolution_multiplier", new[] { ("720p", "1.5"), ("1080p", "2.25"), ("4K", "4.5"), ("Your screen's size", "0") }, "2.25");
        Row("Picture shape", "aspect_mode", new[] { ("Original 4:3", "0"), ("Widescreen 16:9", "1"), ("Fill the screen", "2"), ("Cinematic 2.39:1", "3") }, "3");
        Row("HDR", "hdr", new[] { ("Off", "false"), ("On (needs HDR on in Windows display settings)", "true") }, "false");
        Row("HDR brightness", "hdr_paper_white_nits", new[] { ("Dim (120 nits)", "120.0"), ("Normal (200 nits)", "200.0"), ("Bright (300 nits)", "300.0"), ("Brightest (400 nits)", "400.0") }, "200.0");
        Row("HDR highlights", "hdr_peak_nits", new[] { ("Subtle (400 nits)", "400.0"), ("Standard (1000 nits)", "1000.0"), ("Strong (1500 nits)", "1500.0") }, "1000.0");
        Row("Frame rate", "frame_interpolation_fps", new[] { ("60 fps (original)", "0"), ("120 fps (smoothed)", "120"), ("180 fps (smoothed)", "180") }, "120");
        page.Controls.Add(Note("Changes are saved at once and apply the next time the game starts. In game, F10 opens the same settings\n" +
                               "and F11 switches between fullscreen and a window."));
        page.Controls.Add(table);
        return page;
    }

    // ---------------- Controls ----------------
    static TabPage ControlsPage()
    {
        var page = Page("Controls");
        page.Controls.Add(Note(
            "Gamepads (Xbox, PlayStation, Switch Pro, GameCube adapter...) work right away as a GameCube controller.\n\n" +
            "Keyboard:\n" +
            "    Move                  W A S D\n" +
            "    Camera (C-stick)      Arrow keys\n" +
            "    A (jump)              Space\n" +
            "    B                     Left Shift\n" +
            "    X / Y                 R / F\n" +
            "    Z                     C\n" +
            "    L / R                 Q / E\n" +
            "    Start (pause)         Enter\n" +
            "    D-pad                 I J K L\n\n" +
            "To change any of these, press F10 in the game and open Controller settings."));
        return page;
    }

    // ---------------- Mods ----------------
    TabPage ModsPage()
    {
        var page = Page("Mods");
        var mods = Path.Combine(root, "Play", "UserData", "mods");
        var list = new CheckedListBox { Dock = DockStyle.Top, Height = 220, BackColor = Panel, ForeColor = Text1, CheckOnClick = true, BorderStyle = BorderStyle.None };
        void Fill()
        {
            list.Items.Clear();
            if (!Directory.Exists(mods)) return;
            foreach (var d in Directory.GetDirectories(mods).OrderBy(d => d, StringComparer.OrdinalIgnoreCase))
                list.Items.Add(Path.GetFileName(d), !File.Exists(Path.Combine(d, "disabled")));
        }
        list.ItemCheck += (_, e) =>
        {
            var flag = Path.Combine(mods, (string)list.Items[e.Index], "disabled");
            if (e.NewValue == CheckState.Checked) File.Delete(flag); else File.WriteAllText(flag, "");
        };
        var open = MakeButton("Open the mods folder");
        open.Click += (_, _) => { Directory.CreateDirectory(mods); Process.Start(new ProcessStartInfo(mods) { UseShellExecute = true }); };
        page.Controls.Add(list);
        page.Controls.Add(open);
        page.Controls.Add(Note(
            "A mod is a folder in Play\\UserData\\mods. Its files\\ folder mirrors the disc's files\\ (replacing game files),\n" +
            "and its textures\\ folder holds texture replacements. Untick a mod to switch it off; later names win."));
        Fill();
        return page;
    }
}
