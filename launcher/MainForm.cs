using System.Diagnostics;
using System.Drawing.Drawing2D;
using System.Media;

namespace ScoobyReMasked;

// The launcher window: a spooky animated header (haunted mansion, bats, the Mystery Machine), a nav column
// with Scooby catching snacks, and one card per page (Play, Display, Controls, Mods). Everything it changes goes to
// Play\UserData\Config.toml, the keys the game's F10 menu uses. All art is drawn here; nothing from the game is shipped.
sealed class MainForm : Form
{
    const string GameExe = "Scooby-Doo Re-Masked.exe", AppTitle = "Scooby-Doo: Re-Masked";
    static Color Hex(string s) => ColorTranslator.FromHtml(s);
    static readonly Color Cream = Hex("#FFF4DC"), Paper = Hex("#FFFDF5"), Ink = Hex("#2A1B3D"), Teal = Hex("#2FB5A8"),
        TealLight = Hex("#CFF1EC"), Lime = Hex("#8CC63F"), Orange = Hex("#F7941D"), Gold = Hex("#FFD23F"), Brown = Hex("#A8692E"),
        Night = Hex("#1C1238"), Dusk = Hex("#4A2F7A"), Moon = Hex("#FFF2B0"), Shadow = Hex("#120A26");

    // Fonts are in points and grow with Windows display scaling; every pixel size goes through S() so it grows too.
    static float k = 1f;
    static int S(int v) => (int)Math.Round(v * k);
    static Size S(int w, int h) => new(S(w), S(h));
    static Padding S(int l, int t, int r, int b) => new(S(l), S(t), S(r), S(b));

    readonly string root, play, userData, configPath, modsDir, discDir;
    readonly string? shotDir;
    readonly Panel content = new() { Dock = DockStyle.Fill };
    readonly List<(Button button, Control page)> pages = new();
    Label playStatus = null!, dumpHelp = null!;
    Button chooseDisc = null!, playButton = null!;
    ProgressBar progress = null!;

    public MainForm(string? shotDir = null)
    {
        this.shotDir = shotDir;
        k = DeviceDpi / 96f;
        root = FindRoot();
        play = Path.Combine(root, "Play");
        userData = Path.Combine(play, "UserData");
        configPath = Path.Combine(userData, "Config.toml");
        modsDir = Path.Combine(userData, "mods");
        discDir = Path.Combine(root, "Disc", "DATA");

        Text = AppTitle;
        try { Icon = Icon.ExtractAssociatedIcon(Environment.ProcessPath!); } catch { }
        glassCursor = MakeGlassCursor(clickable: false);
        glassHand = MakeGlassCursor(clickable: true);
        Cursor = glassCursor;
        var area = Screen.PrimaryScreen?.WorkingArea.Size ?? new Size(S(1060), S(740));
        ClientSize = new Size(Math.Min(S(1060), area.Width - 40), Math.Min(S(740), area.Height - 60));
        MinimumSize = new Size(Math.Min(S(900), area.Width), Math.Min(S(620), area.Height));
        BackColor = Cream;
        Font = new Font("Segoe UI", 10.5f);
        StartPosition = FormStartPosition.CenterScreen;
        DoubleBuffered = true;
        content.Padding = S(16, 12, 20, 16);

        var header = new BufferedPanel { Dock = DockStyle.Top, Height = S(170) };
        header.Paint += PaintHeader;
        var nav = new BufferedFlow { Dock = DockStyle.Left, Width = S(220), FlowDirection = FlowDirection.TopDown, Padding = S(16, 18, 0, 0), WrapContents = false };
        nav.Paint += PaintNav;
        header.MouseClick += (_, e) =>
        {
            if (vanRect.Contains(e.Location)) { startAt["van"] = T; SystemSounds.Asterisk.Play(); }
        };
        header.MouseMove += (_, e) => header.Cursor = vanRect.Contains(e.Location) ? glassHand : glassCursor;
        nav.MouseClick += (_, e) => { if (snackRect.Contains(e.Location) && Progress("snack", 1.1) < 0) { startAt["snack"] = T; snacks++; } };
        nav.MouseMove += (_, e) => nav.Cursor = snackRect.Contains(e.Location) ? glassHand : glassCursor;
        var frame = new System.Windows.Forms.Timer { Interval = 33 };
        frame.Tick += (_, _) =>
        {
            if (!animate || WindowState == FormWindowState.Minimized) return;
            header.Invalidate();
            nav.Invalidate(new Rectangle(0, navSpriteTop, nav.Width, Math.Max(0, nav.Height - navSpriteTop)));
            float glow = (float)(Math.Sin(T * 3) + 1) / 2;  // PLAY breathes between orange and gold
            if (playButton.Enabled) playButton.BackColor = Color.FromArgb(Lerp(Orange.R, Gold.R, glow), Lerp(Orange.G, Gold.G, glow), Lerp(Orange.B, Gold.B, glow));
        };
        frame.Start();
        Controls.Add(content);
        Controls.Add(nav);
        Controls.Add(header);

        AddPage(nav, "Play", BuildPlayPage());
        AddPage(nav, "Display", BuildDisplayPage());
        AddPage(nav, "Controls", BuildControlsPage());
        AddPage(nav, "Mods", BuildModsPage());
        ShowPage(pages[0].button);
        Shown += (_, _) =>
        {
            if (shotDir == null) return;
            // Headless visual check: every page drawn to <dir>\<page>.png, plus the two pointers.
            Directory.CreateDirectory(shotDir);
            shotShift = 9;
            foreach (var p in pages)
            {
                ShowPage(p.button);
                Application.DoEvents();
                using var bmp = new Bitmap(Width, Height);
                DrawToBitmap(bmp, new Rectangle(Point.Empty, Size));
                bmp.Save(Path.Combine(shotDir, p.button.Text + ".png"));
            }
            using (var cur = new Bitmap(S(80), S(40)))
            using (var g = Graphics.FromImage(cur))
            {
                g.Clear(Cream);
                for (int i = 0; i < cursorBitmaps.Count; i++) g.DrawImageUnscaled(cursorBitmaps[i], i * S(40), 0);
                cur.Save(Path.Combine(shotDir, "cursor.png"));
            }
            Close();
        };
    }

    // The folder holding Play\<GameExe>: beside the launcher, or (while developing) above launcher\publish.
    static string FindRoot()
    {
        for (var d = new DirectoryInfo(AppContext.BaseDirectory); d != null; d = d.Parent)
            if (File.Exists(Path.Combine(d.FullName, "Play", GameExe))) return d.FullName;
        return AppContext.BaseDirectory;
    }

    void AddPage(FlowLayoutPanel nav, string title, Control page)
    {
        var button = new Button
        {
            Text = title, Size = S(188, 50), FlatStyle = FlatStyle.Flat, BackColor = Paper, ForeColor = Ink,
            Font = new Font("Segoe UI", 13f, FontStyle.Bold), TextAlign = ContentAlignment.MiddleLeft, Padding = S(12, 0, 0, 0),
            Margin = S(0, 0, 0, 10), Cursor = glassHand,
        };
        button.FlatAppearance.BorderColor = Ink;
        button.FlatAppearance.BorderSize = S(2);
        button.Click += (_, _) => ShowPage(button);
        button.MouseEnter += (_, _) => { if (button.BackColor == Paper) button.BackColor = TealLight; };
        button.MouseLeave += (_, _) => { if (button.BackColor == TealLight) button.BackColor = Paper; };
        page.Dock = DockStyle.Fill;
        page.Visible = false;
        content.Controls.Add(page);
        nav.Controls.Add(button);
        pages.Add((button, page));
    }

    void ShowPage(Button button)
    {
        foreach (var (b, page) in pages)
        {
            bool on = b == button;
            b.BackColor = on ? Teal : Paper;
            b.ForeColor = on ? Color.White : Ink;
            page.Visible = on;
        }
        RefreshDisc();
    }

    // ----- The fun bits: animated header and side column -----
    // Everything moves on one clock; with Windows animations turned off the scene stands still.
    readonly bool animate = SystemInformation.UIEffectsEnabled;
    readonly Stopwatch clock = Stopwatch.StartNew();
    double T => (animate ? clock.Elapsed.TotalSeconds : 1.0) + shotShift;
    double shotShift;  // --shots moves the clock on so the van is mid-road
    readonly Dictionary<string, double> startAt = new();
    Bitmap? headerBack, headerFront, headerTitle;
    readonly List<RectangleF> windows = new();
    readonly List<(float x, float y, float r, float phase)> stars = new();
    Rectangle vanRect, snackRect;
    int navSpriteTop, snacks;

    static int Lerp(int a, int b, float f) => (int)(a + (b - a) * f);

    // How far into a click-triggered move (0..1), or -1 when none is playing.
    double Progress(string name, double seconds) => startAt.TryGetValue(name, out var at) && T - at < seconds ? (T - at) / seconds : -1;

    // The pointer: a detective's magnifying glass, "?" in the lens, "!" over anything clickable. Static so Btn() can use it.
    static Cursor glassCursor = Cursors.Default, glassHand = Cursors.Hand;
    readonly List<Bitmap> cursorBitmaps = new();

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    struct IconInfo { public bool fIcon; public int xHotspot, yHotspot; public IntPtr hbmMask, hbmColor; }
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern IntPtr CreateIconIndirect(ref IconInfo info);
    [System.Runtime.InteropServices.DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr obj);

    Cursor MakeGlassCursor(bool clickable)
    {
        int size = S(32);
        float lens = size * 0.58f, c = lens / 2 + S(2);  // lens centre = the click point
        using var bmp = new Bitmap(size, size);
        using (var g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using (var handleInk = new Pen(Ink, size * 0.16f + S(3)) { StartCap = LineCap.Round, EndCap = LineCap.Round })
            using (var handle = new Pen(Brown, size * 0.16f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
            {
                g.DrawLine(handleInk, c + lens * 0.36f, c + lens * 0.36f, size - S(4), size - S(4));
                g.DrawLine(handle, c + lens * 0.36f, c + lens * 0.36f, size - S(4), size - S(4));
            }
            var lensRect = new RectangleF(c - lens / 2, c - lens / 2, lens, lens);
            using (var glass = new SolidBrush(Color.FromArgb(220, 214, 244, 255))) g.FillEllipse(glass, lensRect);
            using (var rim = new Pen(Ink, Math.Max(2f, size / 11f))) g.DrawEllipse(rim, lensRect);
            using var f = new Font("Segoe UI", lens * 0.55f, FontStyle.Bold, GraphicsUnit.Pixel);
            var fmt = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
            using var mark = new SolidBrush(clickable ? Orange : Teal);
            g.DrawString(clickable ? "!" : "?", f, mark, new PointF(c, c + S(1)), fmt);
        }
        var info = new IconInfo { fIcon = false, xHotspot = (int)c, yHotspot = (int)c, hbmColor = bmp.GetHbitmap(Color.FromArgb(0)), hbmMask = new Bitmap(size, size).GetHbitmap() };
        cursorBitmaps.Add((Bitmap)bmp.Clone());
        var h = CreateIconIndirect(ref info);
        DeleteObject(info.hbmColor);
        DeleteObject(info.hbmMask);
        return h == IntPtr.Zero ? (clickable ? Cursors.Hand : Cursors.Default) : new Cursor(h);
    }

    // The title face: the embedded title.ttf when the build has one (a Scooby-Doo logo look-alike), else a stock face.
    static readonly System.Drawing.Text.PrivateFontCollection titleFonts = new();
    static FontFamily TitleFont()
    {
        try
        {
            using var s = typeof(MainForm).Assembly.GetManifestResourceStream("ScoobyReMasked.title.ttf");
            if (s != null && titleFonts.Families.Length == 0)
            {
                var data = ReadAll(s);
                var mem = System.Runtime.InteropServices.Marshal.AllocCoTaskMem(data.Length);  // kept for the app's life
                System.Runtime.InteropServices.Marshal.Copy(data, 0, mem, data.Length);
                titleFonts.AddMemoryFont(mem, data.Length);
            }
            if (titleFonts.Families.Length > 0) return titleFonts.Families[0];
        }
        catch { }
        foreach (var name in new[] { "Cooper Black", "Segoe UI Black" })
            if (FontFamily.Families.Any(f => f.Name == name)) return new FontFamily(name);
        return new FontFamily("Segoe UI");
    }

    // Static parts of the header, drawn once per size: sky and moon behind; hills, mansion, road and title in front.
    void BuildHeaderLayers(Size size)
    {
        headerBack?.Dispose();
        headerFront?.Dispose();
        var r = new Rectangle(Point.Empty, size);
        headerBack = new Bitmap(size.Width, size.Height);
        using (var g = Graphics.FromImage(headerBack))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using (var sky = new LinearGradientBrush(r, Night, Dusk, LinearGradientMode.Vertical)) g.FillRectangle(sky, r);
            var moon = new RectangleF(r.Width - S(330), S(16), S(78), S(78));
            using (var halo = new SolidBrush(Color.FromArgb(18, Moon)))
                for (int i = 4; i >= 1; i--) g.FillEllipse(halo, RectangleF.Inflate(moon, S(9 * i), S(9 * i)));
            using (var m = new SolidBrush(Moon)) g.FillEllipse(m, moon);
            using (var crater = new SolidBrush(Color.FromArgb(40, 160, 130, 60)))
            {
                g.FillEllipse(crater, moon.X + S(16), moon.Y + S(18), S(16), S(14));
                g.FillEllipse(crater, moon.X + S(44), moon.Y + S(40), S(20), S(16));
                g.FillEllipse(crater, moon.X + S(26), moon.Y + S(52), S(10), S(9));
            }
        }
        var rng = new Random(7);
        stars.Clear();
        for (int i = 0; i < size.Width / S(14); i++)
            stars.Add((rng.Next(size.Width), rng.Next(S(100)), S(1) + (float)rng.NextDouble() * S(2), (float)rng.NextDouble() * 6));

        headerFront = new Bitmap(size.Width, size.Height);
        windows.Clear();
        using (var g = Graphics.FromImage(headerFront))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using var dark = new SolidBrush(Shadow);
            int ground = r.Height - S(22);
            // Rolling hills, with the big one on the right carrying the mansion.
            for (int i = 0; i < 7; i++)
            {
                int rad = S(80 + 25 * (i % 3));
                g.FillEllipse(dark, r.Width * i / 6 - rad, ground - rad / 3, rad * 2, rad * 2);
            }
            float hx = r.Width - S(180);
            g.FillEllipse(dark, hx - S(150), ground - S(52), S(300), S(160));
            // The mansion: a hall, two towers with pointed roofs, a crooked chimney.
            float mx = hx - S(70), my = ground - S(108);
            g.FillRectangle(dark, mx, my + S(30), S(140), S(80));
            g.FillPolygon(dark, new[] { new PointF(mx - S(8), my + S(32)), new PointF(mx + S(70), my), new PointF(mx + S(148), my + S(32)) });
            g.FillRectangle(dark, mx - S(22), my + S(6), S(34), S(104));
            g.FillPolygon(dark, new[] { new PointF(mx - S(28), my + S(8)), new PointF(mx - S(5), my - S(34)), new PointF(mx + S(18), my + S(8)) });
            g.FillRectangle(dark, mx + S(124), my + S(18), S(30), S(92));
            g.FillPolygon(dark, new[] { new PointF(mx + S(118), my + S(20)), new PointF(mx + S(139), my - S(18)), new PointF(mx + S(160), my + S(20)) });
            g.FillPolygon(dark, new[] { new PointF(mx + S(92), my + S(14)), new PointF(mx + S(104), my + S(8)), new PointF(mx + S(108), my - S(6)), new PointF(mx + S(96), my - S(4)) });
            foreach (var (wx, wy) in new[] { (mx + S(14), my + S(44)), (mx + S(48), my + S(44)), (mx + S(82), my + S(44)), (mx + S(116), my + S(44)),
                                             (mx + S(14), my + S(76)), (mx + S(116), my + S(76)), (mx - S(12), my + S(26)), (mx + S(132), my + S(36)) })
                windows.Add(new RectangleF(wx, wy, S(12), S(16)));
            using (var door = new SolidBrush(Hex("#2A1B10"))) g.FillRectangle(door, mx + S(62), my + S(80), S(18), S(30));
            // A bare crooked tree on the left.
            using (var tree = new Pen(Shadow, S(6)) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round })
            {
                float tx = S(250);
                g.DrawLines(tree, new[] { new PointF(tx, ground), new PointF(tx + S(6), ground - S(40)), new PointF(tx - S(4), ground - S(70)) });
                tree.Width = S(3);
                g.DrawLine(tree, tx + S(4), ground - S(36), tx + S(30), ground - S(58));
                g.DrawLine(tree, tx + S(1), ground - S(56), tx - S(24), ground - S(74));
                g.DrawLine(tree, tx + S(22), ground - S(52), tx + S(34), ground - S(50));
            }
            // The road the van drives on.
            g.FillRectangle(dark, 0, ground, r.Width, r.Height - ground);
            using (var line = new Pen(Color.FromArgb(120, Gold), S(2)) { DashPattern = new[] { 6f, 5f } })
                g.DrawLine(line, 0, r.Height - S(9), r.Width, r.Height - S(9));
        }
        // Title and credit on their own layer, drawn last so the van and windows pass behind them.
        headerTitle?.Dispose();
        headerTitle = new Bitmap(size.Width, size.Height);
        using (var g = Graphics.FromImage(headerTitle))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
            // Title in a groovy face, yellow to orange with the logo's red-orange rim.
            using var path = new GraphicsPath();
            var fmt = new StringFormat { Alignment = StringAlignment.Center };
            float em = Math.Min(S(46), (r.Width - S(2 * 190)) / 12f);
            path.AddString("SCOOBY-DOO: RE-MASKED", TitleFont(), (int)FontStyle.Bold, em, new Point(r.Width / 2, S(30)), fmt);
            using (var outline = new Pen(Ink, S(9)) { LineJoin = LineJoin.Round }) g.DrawPath(outline, path);
            using (var rim = new Pen(Hex("#E2491B"), S(4)) { LineJoin = LineJoin.Round }) g.DrawPath(rim, path);
            using (var fill = new LinearGradientBrush(new Rectangle(0, S(16), 10, (int)em + S(10)), Hex("#FFE85C"), Hex("#F7941D"), LinearGradientMode.Vertical)) g.FillPath(fill, path);
            using (var credit = new Font("Segoe UI", 11f, FontStyle.Bold))
            using (var shadow = new SolidBrush(Color.FromArgb(160, 0, 0, 0)))
            using (var light = new SolidBrush(Moon))
            {
                var at = new PointF(r.Width / 2f, S(30) + em * 1.28f);
                g.DrawString("Made by JosiahG", credit, shadow, at.X + S(1), at.Y + S(1), fmt);
                g.DrawString("Made by JosiahG", credit, light, at, fmt);
            }
        }
    }

    void PaintHeader(object? sender, PaintEventArgs e)
    {
        var g = e.Graphics;
        var r = ((Control)sender!).ClientRectangle;
        if (r.Width <= 0 || r.Height <= 0) return;
        if (headerBack == null || headerBack.Size != r.Size) BuildHeaderLayers(r.Size);
        double t = T;
        g.DrawImageUnscaled(headerBack!, 0, 0);
        g.SmoothingMode = SmoothingMode.AntiAlias;

        foreach (var (x, y, rad, phase) in stars)  // twinkle
        {
            int a = (int)(110 + 110 * Math.Sin(t * 2 + phase));
            using var b = new SolidBrush(Color.FromArgb(Math.Clamp(a, 0, 255), 255, 250, 220));
            g.FillEllipse(b, x - rad / 2, y - rad / 2, rad, rad);
        }
        // Thin clouds drift across the moon.
        using (var cloud = new SolidBrush(Color.FromArgb(120, 150, 130, 190)))
            for (int i = 0; i < 3; i++)
            {
                float span = r.Width + S(260);
                float x = (float)((i * span / 3 + t * S(8 + 4 * i)) % span) - S(130);
                float y = S(30 + i * 22);
                for (int j = 0; j < 3; j++) g.FillEllipse(cloud, x + j * S(34), y - (j == 1 ? S(8) : 0), S(64), S(22));
            }
        // Bats flap across the sky on long loops.
        for (int i = 0; i < 3; i++)
        {
            double p = t * (0.07 + 0.02 * i) + i * 0.37;
            float x = (float)((p % 1.0) * (r.Width + S(200)) - S(100));
            float y = (float)(S(40 + 22 * i) + Math.Sin(t * 1.7 + i) * S(14));
            DrawBat(g, x, y, S(16 - 2 * i), (float)Math.Sin(t * 14 + i * 2));
        }

        g.DrawImageUnscaled(headerFront!, 0, 0);
        // Mansion windows flicker like candles, now and then going dark.
        for (int i = 0; i < windows.Count; i++)
        {
            double n = Math.Sin(t * 3.1 + i * 1.7) + Math.Sin(t * 7.3 + i * 4.1) * 0.5;
            int a = n < -1.1 ? 30 : (int)(170 + 50 * Math.Sin(t * 5 + i));
            using var lit = new SolidBrush(Color.FromArgb(Math.Clamp(a, 0, 255), 255, 214, 90));
            g.FillRectangle(lit, windows[i]);
        }

        // The Mystery Machine drives along the road, headlights on; a click makes it hop.
        if (VanArt != null)
        {
            float vw = S(100), vh = vw * VanArt.Height / VanArt.Width, ground = r.Height - S(2);
            double hop = Progress("van", 0.6);
            float bounce = (float)(Math.Abs(Math.Sin(t * 9)) * S(2) + (hop < 0 ? 0 : Math.Sin(hop * Math.PI) * S(26)));
            float vx = (float)((t * S(60)) % (r.Width + vw * 2)) - vw, vy = ground - vh - bounce;
            vanRect = new Rectangle((int)vx, (int)vy, (int)vw, (int)vh);
            using (var beam = new SolidBrush(Color.FromArgb(55, 255, 240, 170)))
                g.FillPolygon(beam, new[] { new PointF(vx + vw * 0.97f, vy + vh * 0.68f), new PointF(vx + vw * 2f, vy + vh * 0.38f), new PointF(vx + vw * 2f, vy + vh * 1.05f) });
            if (vanSized == null || vanSized.Height != (int)vh) vanSized = SizedArt(VanArt, (int)vh);
            g.DrawImageUnscaled(vanSized, (int)vx, (int)vy);
        }
        g.DrawImageUnscaled(headerTitle!, 0, 0);
    }

    static void DrawBat(Graphics g, float x, float y, float s, float flap)
    {
        using var b = new SolidBrush(Hex("#0B0618"));
        float lift = s * 0.7f * flap;
        g.FillPolygon(b, new[] { new PointF(x, y), new PointF(x - s * 0.6f, y - lift - s * 0.2f), new PointF(x - s * 1.2f, y - lift * 0.6f),
                                 new PointF(x - s * 0.9f, y + s * 0.1f), new PointF(x - s * 0.5f, y + s * 0.05f), new PointF(x - s * 0.25f, y + s * 0.3f) });
        g.FillPolygon(b, new[] { new PointF(x, y), new PointF(x + s * 0.6f, y - lift - s * 0.2f), new PointF(x + s * 1.2f, y - lift * 0.6f),
                                 new PointF(x + s * 0.9f, y + s * 0.1f), new PointF(x + s * 0.5f, y + s * 0.05f), new PointF(x + s * 0.25f, y + s * 0.3f) });
        g.FillEllipse(b, x - s * 0.22f, y - s * 0.25f, s * 0.44f, s * 0.55f);
        g.FillPolygon(b, new[] { new PointF(x - s * 0.18f, y - s * 0.18f), new PointF(x - s * 0.12f, y - s * 0.42f), new PointF(x - s * 0.02f, y - s * 0.2f) });
        g.FillPolygon(b, new[] { new PointF(x + s * 0.18f, y - s * 0.18f), new PointF(x + s * 0.12f, y - s * 0.42f), new PointF(x + s * 0.02f, y - s * 0.2f) });
    }

    // A bone-shaped Scooby Snack biscuit.
    static void DrawSnack(Graphics g, float cx, float cy, float s)
    {
        using var b = new SolidBrush(Brown);
        using var ink = new Pen(Ink, Math.Max(1.5f, s / 14f));
        float r = s * 0.22f;
        var ends = new[] { (cx - s * 0.42f, cy - r * 0.8f), (cx - s * 0.42f, cy + r * 0.8f), (cx + s * 0.42f, cy - r * 0.8f), (cx + s * 0.42f, cy + r * 0.8f) };
        foreach (var (ex, ey) in ends) g.DrawEllipse(ink, ex - r, ey - r, r * 2, r * 2);
        g.DrawRectangle(ink, cx - s * 0.42f, cy - r * 0.7f, s * 0.84f, r * 1.4f);
        foreach (var (ex, ey) in ends) g.FillEllipse(b, ex - r, ey - r, r * 2, r * 2);
        g.FillRectangle(b, cx - s * 0.42f, cy - r * 0.7f, s * 0.84f, r * 1.4f);
    }

    // Scooby's picture (embedded scooby.png), resized once per size and kept.
    static readonly Image? ScoobyArt = LoadArt("scooby.png"), VanArt = LoadArt("mystery_machine.png");
    Bitmap? vanSized;
    static readonly PointF ScoobyMouth = new(0.43f, 0.54f);  // his open mouth, as a fraction of the picture
    readonly Dictionary<int, Bitmap> scoobySized = new();

    static Image? LoadArt(string name)
    {
        using var s = typeof(MainForm).Assembly.GetManifestResourceStream("ScoobyReMasked." + name);
        return s == null ? null : Image.FromStream(new MemoryStream(ReadAll(s)));
    }

    static byte[] ReadAll(Stream s) { using var m = new MemoryStream(); s.CopyTo(m); return m.ToArray(); }

    static Bitmap SizedArt(Image art, int height)
    {
        var b = new Bitmap(Math.Max(1, art.Width * height / art.Height), Math.Max(1, height));
        using var g = Graphics.FromImage(b);
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.DrawImage(art, new Rectangle(Point.Empty, b.Size));
        return b;
    }

    // Scooby waits under the menu; click the floating snack and he catches it.
    void PaintNav(object? sender, PaintEventArgs e)
    {
        var nav = (Control)sender!;
        var g = e.Graphics;
        double t = T;
        navSpriteTop = pages.Count > 0 ? pages[^1].button.Bottom + S(10) : 0;
        int bottom = nav.ClientSize.Height - S(10);
        int room = bottom - navSpriteTop - S(70);
        if (room < S(80) || ScoobyArt == null) { snackRect = Rectangle.Empty; return; }
        int height = Math.Min(S(190), room);
        if (!scoobySized.TryGetValue(height, out var art)) scoobySized[height] = art = SizedArt(ScoobyArt, height);
        int cx = (nav.Width + S(16)) / 2;
        double eat = Progress("snack", 1.1);
        // He sways gently; once the snack lands he does a happy hop.
        double hop = eat < 0.7 ? 0 : Math.Sin((eat - 0.7) / 0.3 * Math.PI);
        int lift = (int)(Math.Sin(t * 1.8) * S(3) + hop * S(14));
        var face = new Rectangle(cx - art.Width / 2, bottom - art.Height - S(22) - lift, art.Width, art.Height);
        g.DrawImageUnscaled(art, face.Location);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        float s = S(38);
        float mouthX = face.X + face.Width * ScoobyMouth.X, mouthY = face.Y + face.Height * ScoobyMouth.Y;
        float sx = cx, sy = (float)(face.Y - S(34) + Math.Sin(t * 2.4) * S(6));
        if (eat >= 0)  // the snack arcs into his mouth, then is gone
        {
            double k2 = Math.Min(1, eat / 0.7);
            sx = (float)(cx + (mouthX - cx) * k2);
            sy = (float)(face.Y - S(34) + (mouthY - face.Y + S(34)) * k2 - Math.Sin(k2 * Math.PI) * S(30));
            s *= (float)(1 - k2 * 0.85);
            if (eat >= 0.7) s = 0;
        }
        snackRect = new Rectangle(cx - S(24), face.Y - S(60), S(48), S(52));
        if (s > 0) DrawSnack(g, sx, sy, s);
        using var f = new Font("Segoe UI", 9.5f, FontStyle.Bold);
        using var text = new SolidBrush(Ink);
        var fmt = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString(snacks == 0 ? "Click the Scooby Snack!" : $"Scooby Snacks: {snacks}", f, text, cx, bottom - S(18), fmt);
    }

    // ----- Shared pieces -----
    // A white card holding one page's controls, top to bottom.
    static (Panel card, FlowLayoutPanel body) NewCard(string heading)
    {
        var card = new BufferedPanel { Padding = S(22, 16, 22, 16) };
        card.Paint += (sender, e) =>
        {
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            var r = ((Control)sender!).ClientRectangle;
            r.Width -= S(8); r.Height -= S(8);
            r.Offset(S(2), S(2));
            using var path = Rounded(r, S(14));
            using var shadow = Rounded(new Rectangle(r.X + S(5), r.Y + S(5), r.Width, r.Height), S(14));
            using (var shade = new SolidBrush(Color.FromArgb(50, 0, 0, 0))) g.FillPath(shade, shadow);
            using (var paper = new SolidBrush(Paper)) g.FillPath(paper, path);
            using (var border = new Pen(Ink, S(3))) g.DrawPath(border, path);
        };
        var body = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, FlowDirection = FlowDirection.TopDown, WrapContents = false, BackColor = Paper };
        card.AutoScroll = true;
        body.Controls.Add(new Label { Text = heading, Font = new Font("Segoe UI", 18f, FontStyle.Bold), ForeColor = Ink, AutoSize = true, Margin = S(0, 0, 0, 10) });
        card.Controls.Add(body);
        return (card, body);
    }

    static GraphicsPath Rounded(Rectangle r, int radius)
    {
        var p = new GraphicsPath();
        int d = Math.Max(2, radius * 2);
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    static Label Note(string text, int width = 680) => new()
    {
        Text = text, AutoSize = true, MaximumSize = new Size(S(width), 0), ForeColor = Ink, Margin = S(0, 6, 0, 6),
    };

    static Button Btn(string text, EventHandler onClick, Color? back = null)
    {
        var b = new Button
        {
            Text = text, AutoSize = true, FlatStyle = FlatStyle.Flat, BackColor = back ?? Gold, ForeColor = Ink,
            Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Padding = S(10, 4, 10, 4), Margin = S(0, 6, 10, 6), Cursor = glassHand,
        };
        b.FlatAppearance.BorderColor = Ink;
        b.FlatAppearance.BorderSize = S(2);
        b.Click += onClick;
        return b;
    }

    // ----- Play -----
    Control BuildPlayPage()
    {
        var (card, body) = NewCard("Ready, gang?");
        playButton = Btn("PLAY", (_, _) => Launch(), Orange);
        playButton.Font = new Font("Segoe UI", 24f, FontStyle.Bold);
        playButton.Padding = S(60, 8, 60, 8);
        body.Controls.Add(playButton);
        playStatus = Note("");
        playStatus.Font = new Font("Segoe UI", 10.5f, FontStyle.Bold);
        body.Controls.Add(playStatus);
        chooseDisc = Btn("Choose disc image (.iso, .gcm, .ciso)...", async (_, _) => await ChooseDisc(), Paper);
        body.Controls.Add(chooseDisc);
        progress = new ProgressBar { Width = S(520), Height = S(16), Visible = false, Margin = S(0, 4, 0, 6) };
        body.Controls.Add(progress);
        body.Controls.Add(dumpHelp = Note(
            "Scooby-Doo: Re-Masked runs your own copy of Scooby-Doo! Unmasked for the GameCube (USA, G5DE78). " +
            "Dump the disc you own (for example with CleanRip on a Wii) as a plain .iso or .ciso, then choose it above. " +
            "The launcher copies the game files into the Disc folder once; the image file isn't needed afterwards."));
        body.Controls.Add(Note("In the game, F10 opens the settings menu (resolution, picture shape, controls, audio) and F11 switches between fullscreen and a window."));
        // Scooby's tips: click the bubble for another one.
        var tipRow = new FlowLayoutPanel { AutoSize = true, Margin = S(0, 10, 0, 6) };
        if (ScoobyArt != null)
        {
            var pic = SizedArt(ScoobyArt, S(60));
            tipRow.Controls.Add(new PictureBox { Image = pic, Size = pic.Size, Margin = S(0, 0, 8, 0) });
        }
        tipLabel = new Label
        {
            AutoSize = true, MaximumSize = new Size(S(600), 0), BackColor = TealLight, ForeColor = Ink,
            BorderStyle = BorderStyle.FixedSingle, Padding = S(10, 8, 10, 8), Cursor = glassHand, Margin = S(0, 4, 0, 0),
        };
        tipLabel.Click += (_, _) => NextTip();
        tipRow.Controls.Add(tipLabel);
        tipIndex = Random.Shared.Next(Tips.Length) - 1;
        NextTip();
        body.Controls.Add(tipRow);
        var row = new FlowLayoutPanel { AutoSize = true, Margin = Padding.Empty };
        row.Controls.Add(Btn("Open game folder", (_, _) => Process.Start("explorer.exe", play), Paper));
        row.Controls.Add(Btn("Open saves and settings", (_, _) => Process.Start("explorer.exe", Directory.CreateDirectory(userData).FullName), Paper));
        body.Controls.Add(row);
        return card;
    }

    static readonly string[] Tips =
    {
        "Press F10 in the game to change resolution, picture shape or controls without quitting.",
        "F11 flips the game between fullscreen and a window.",
        "Got an HDR screen? Switch HDR on in Windows, then on the Display page.",
        "Any gamepad works as a GameCube controller right away.",
        "Cinematic 2.39:1 shows more of every level than the GameCube ever could.",
        "Something look odd at 120 fps? Set the frame rate to 60 on the Display page.",
        "Saves live in Play\\UserData. A backup now and then never hurts.",
        "Hungry? Click the Scooby Snack by the menu.",
        "Give the van a click. Beep beep!",
    };
    Label? tipLabel;
    int tipIndex;

    void NextTip()
    {
        tipIndex = (tipIndex + 1) % Tips.Length;
        if (tipLabel != null) tipLabel.Text = "Scooby says: " + Tips[tipIndex];
    }

    (bool ok, string text) DiscState()
    {
        var boot = Path.Combine(discDir, "sys", "boot.bin");
        if (!File.Exists(boot)) return (false, "No game disc set up yet. Choose your Scooby-Doo! Unmasked disc image below.");
        var id = System.Text.Encoding.ASCII.GetString(File.ReadAllBytes(boot), 0, 6);
        return id == GcDisc.WantedId ? (true, "Scooby-Doo! Unmasked (G5DE78) is ready. Let's go, gang!") : (false, $"The Disc folder holds {id}, not Scooby-Doo! Unmasked USA (G5DE78).");
    }

    void RefreshDisc()
    {
        if (playStatus == null) return;
        var (ok, text) = DiscState();
        playStatus.Text = text;
        playStatus.ForeColor = ok ? Hex("#1E7F55") : Hex("#B4561A");
        playButton.Enabled = ok && File.Exists(Path.Combine(play, GameExe));
        if (!playButton.Enabled) playButton.BackColor = Hex("#D8CFC0");
        chooseDisc.Text = ok ? "Change disc image..." : "Choose disc image (.iso, .gcm, .ciso)...";
        dumpHelp.Visible = !ok;
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
        catch (Exception e) { MessageBox.Show(this, "That file can't be read: " + e.Message, AppTitle); return; }
        using (disc)
        {
            var problem = disc.Problem();
            if (problem.Length > 0) { MessageBox.Show(this, problem, AppTitle, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            var staging = discDir + ".new";
            if (Directory.Exists(staging)) Directory.Delete(staging, true);
            chooseDisc.Enabled = playButton.Enabled = false;
            progress.Visible = true; progress.Value = 0;
            var report = new Progress<(int done, int total, string name)>(p =>
            {
                progress.Maximum = Math.Max(1, p.total); progress.Value = Math.Min(p.done, progress.Maximum);
                playStatus.Text = $"Extracting {p.done}/{p.total}: {p.name}";
            });
            try
            {
                await Task.Run(() => disc.Extract(staging, (d, t, n) => ((IProgress<(int, int, string)>)report).Report((d, t, n))));
                if (Directory.Exists(discDir)) Directory.Delete(discDir, true);
                Directory.Move(staging, discDir);
                var cfg = new GameConfig(configPath);
                cfg.Set("paths", "dvd_root", "'../../Disc/DATA'");  // relative to Play\UserData, so the folder can move
                cfg.Save();
            }
            catch (Exception e) { MessageBox.Show(this, "Extraction failed: " + e.Message, AppTitle, MessageBoxButtons.OK, MessageBoxIcon.Error); }
            finally { progress.Visible = false; chooseDisc.Enabled = true; RefreshDisc(); }
        }
    }

    void Launch()
    {
        var exe = Path.Combine(play, GameExe);
        if (!File.Exists(exe)) { MessageBox.Show(this, $"Play\\{GameExe} is missing. Re-extract the download next to this launcher.", AppTitle); return; }
        Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = play, UseShellExecute = true });
        WindowState = FormWindowState.Minimized;
    }

    // ----- Display -----
    Control BuildDisplayPage()
    {
        var (card, body) = NewCard("Display");
        var cfg = new GameConfig(configPath);
        var table = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, BackColor = Paper, Margin = S(0, 4, 0, 8) };
        void Row(string label, string key, (string text, string raw)[] options, string fallback)
        {
            var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = S(360), Cursor = glassHand };
            foreach (var o in options) box.Items.Add(o.text);
            var current = cfg.Get("video", key) ?? fallback;
            int index = Array.FindIndex(options, o => o.raw.Trim('"') == current.Trim('"'));
            box.SelectedIndex = index >= 0 ? index : Array.FindIndex(options, o => o.raw.Trim('"') == fallback);
            box.SelectedIndexChanged += (_, _) =>
            {
                var c = new GameConfig(configPath);
                c.Set("video", key, options[box.SelectedIndex].raw);
                c.Save();
            };
            table.Controls.Add(new Label { Text = label, AutoSize = true, ForeColor = Ink, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Margin = S(0, 10, 16, 8) });
            table.Controls.Add(box);
        }
        Row("Window", "display_mode", new[] { ("Borderless fullscreen", "\"borderless\""), ("Window", "\"windowed\""), ("Exclusive fullscreen", "\"exclusive\"") }, "borderless");
        Row("Resolution", "resolution_multiplier", new[] { ("720p", "1.5"), ("1080p", "2.25"), ("4K", "4.5"), ("Your screen's size", "0") }, "2.25");
        Row("Picture shape", "aspect_mode", new[] { ("Original 4:3", "0"), ("Widescreen 16:9", "1"), ("Fill the screen", "2"), ("Cinematic 2.39:1", "3") }, "3");
        Row("Frame rate", "frame_interpolation_fps", new[] { ("60 fps (original)", "0"), ("120 fps (smoothed)", "120"), ("180 fps (smoothed)", "180") }, "120");
        Row("HDR", "hdr", new[] { ("Off", "false"), ("On (needs HDR on in Windows display settings)", "true") }, "false");
        Row("HDR brightness", "hdr_paper_white_nits", new[] { ("Dim (120 nits)", "120.0"), ("Normal (200 nits)", "200.0"), ("Bright (300 nits)", "300.0"), ("Brightest (400 nits)", "400.0") }, "200.0");
        Row("HDR highlights", "hdr_peak_nits", new[] { ("Subtle (400 nits)", "400.0"), ("Standard (1000 nits)", "1000.0"), ("Strong (1500 nits)", "1500.0") }, "1000.0");
        body.Controls.Add(table);
        body.Controls.Add(Note("Changes are saved at once and apply the next time the game starts. In the game, F10 opens the same " +
                               "settings and F11 switches between fullscreen and a window."));
        return card;
    }

    // ----- Controls -----
    Control BuildControlsPage()
    {
        var (card, body) = NewCard("Controls");
        body.Controls.Add(Note("Gamepads (Xbox, PlayStation, Switch Pro, GameCube adapter...) work right away as a GameCube controller. " +
                               "To change any button, press F10 in the game and open Controller settings."));
        body.Controls.Add(new Label { Text = "Keyboard", Font = new Font("Segoe UI", 13f, FontStyle.Bold), ForeColor = Ink, AutoSize = true, Margin = S(0, 10, 0, 0) });
        var table = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, BackColor = Paper, Margin = S(0, 8, 0, 8), CellBorderStyle = TableLayoutPanelCellBorderStyle.Single };
        foreach (var (what, key) in new[] { ("Move", "W A S D"), ("Camera (C-stick)", "Arrow keys"), ("A (jump)", "Space"), ("B", "Left Shift"),
                     ("X / Y", "R / F"), ("Z", "C"), ("L / R", "Q / E"), ("Start (pause)", "Enter"), ("D-pad", "I J K L") })
        {
            table.Controls.Add(new Label { Text = what, AutoSize = true, ForeColor = Ink, Margin = S(8, 6, 24, 6) });
            table.Controls.Add(new Label { Text = key, AutoSize = true, ForeColor = Ink, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Margin = S(8, 6, 24, 6) });
        }
        body.Controls.Add(table);
        return card;
    }

    // ----- Mods -----
    Control BuildModsPage()
    {
        var (card, body) = NewCard("Mods");
        body.Controls.Add(Note("A mod is a folder in Play\\UserData\\mods. Its files\\ folder mirrors the disc's files\\ (replacing game files), " +
                               "and its textures\\ folder holds texture replacements. Untick a mod to switch it off; when two mods change the same file, the later name wins."));
        var list = new CheckedListBox { Width = S(520), Height = S(220), CheckOnClick = true, BorderStyle = BorderStyle.FixedSingle, Margin = S(0, 8, 0, 8) };
        void Fill()
        {
            list.Items.Clear();
            if (!Directory.Exists(modsDir)) return;
            foreach (var d in Directory.GetDirectories(modsDir).OrderBy(d => d, StringComparer.OrdinalIgnoreCase))
                list.Items.Add(Path.GetFileName(d), !File.Exists(Path.Combine(d, "disabled")));
        }
        list.ItemCheck += (_, e) =>
        {
            var flag = Path.Combine(modsDir, (string)list.Items[e.Index], "disabled");
            if (e.NewValue == CheckState.Checked) File.Delete(flag); else File.WriteAllText(flag, "");
        };
        body.Controls.Add(list);
        var row = new FlowLayoutPanel { AutoSize = true, Margin = Padding.Empty };
        row.Controls.Add(Btn("Open the mods folder", (_, _) => Process.Start("explorer.exe", Directory.CreateDirectory(modsDir).FullName), Paper));
        row.Controls.Add(Btn("Refresh", (_, _) => Fill(), Paper));
        body.Controls.Add(row);
        Fill();
        return card;
    }
}

sealed class BufferedPanel : Panel
{
    public BufferedPanel() { DoubleBuffered = true; ResizeRedraw = true; }
}

sealed class BufferedFlow : FlowLayoutPanel
{
    public BufferedFlow() { DoubleBuffered = true; ResizeRedraw = true; }
}
