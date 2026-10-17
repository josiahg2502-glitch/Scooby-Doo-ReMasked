using System.Buffers.Binary;
using System.Text;

namespace ScoobyReMasked;

// A GameCube disc image (.iso/.gcm or .ciso): unencrypted, so it is read straight from the container and extracted to
// the layout the game reads (sys/ and files/). Same output as projects/scooby tooling (gcextract.py).
sealed class GcDisc : IDisposable
{
    public const string WantedId = "G5DE78";
    readonly Container disc;
    public string GameId { get; }
    public string Title { get; }
    public int Revision { get; }
    public int Holes => disc.Holes;
    readonly bool gameCube;

    public static GcDisc Open(string path) => new(path);

    GcDisc(string path)
    {
        disc = Container.Open(path);
        var h = disc.Read(0, 0x440);
        GameId = Encoding.ASCII.GetString(h, 0, 6);
        Revision = h[7];
        Title = Encoding.ASCII.GetString(h, 0x20, 0x40).TrimEnd('\0', ' ');
        gameCube = BinaryPrimitives.ReadUInt32BigEndian(h.AsSpan(0x1C, 4)) == 0xC2339F3D;
    }

    // Empty when this is the right disc; otherwise what is wrong, in words for the player.
    public string Problem()
    {
        if (!gameCube) return "That file is not a GameCube disc image (Wii, compressed .rvz/.nkit and other formats don't work: use a plain .iso or .ciso).";
        if (GameId != WantedId) return $"That is {GameId} ({Title}), not Scooby-Doo! Unmasked for the USA ({WantedId}).";
        return "";
    }

    // Extracts sys/ (boot.bin, bi2.bin, apploader.img, main.dol, fst.bin) and files/. Returns the number of files.
    public int Extract(string outDir, Action<int, int, string>? progress = null, CancellationToken cancel = default)
    {
        var sys = Path.Combine(outDir, "sys");
        var files = Path.Combine(outDir, "files");
        Directory.CreateDirectory(sys);
        Directory.CreateDirectory(files);
        var hdr = disc.Read(0, 0x440);
        uint U(byte[] b, int o) => BinaryPrimitives.ReadUInt32BigEndian(b.AsSpan(o, 4));
        uint dolOff = U(hdr, 0x420), fstOff = U(hdr, 0x424), fstSize = U(hdr, 0x428);
        File.WriteAllBytes(Path.Combine(sys, "boot.bin"), hdr);
        File.WriteAllBytes(Path.Combine(sys, "bi2.bin"), disc.Read(0x440, 0x2000));
        var ap = disc.Read(0x2440, 0x20);
        File.WriteAllBytes(Path.Combine(sys, "apploader.img"), disc.Read(0x2440, (int)(0x20 + U(ap, 0x14) + U(ap, 0x18))));
        var dh = disc.Read(dolOff, 0x100);
        long dolSize = 0;
        for (int i = 0; i < 18; i++) dolSize = Math.Max(dolSize, U(dh, i * 4) + (long)U(dh, 0x90 + i * 4));
        File.WriteAllBytes(Path.Combine(sys, "main.dol"), disc.Read(dolOff, (int)dolSize));
        var fst = disc.Read(fstOff, (int)fstSize);
        File.WriteAllBytes(Path.Combine(sys, "fst.bin"), fst);

        int count = (int)U(fst, 8), strtab = 12 * count, done = 0, total = 0;
        for (int i = 1; i < count; i++) if (fst[i * 12] == 0) total++;
        string Name(int i)
        {
            int s = strtab + (int)(U(fst, i * 12) & 0xFFFFFF), e = s;
            while (fst[e] != 0) e++;
            return Encoding.Latin1.GetString(fst, s, e - s);
        }
        // Directory entries hold the index one past their last child; walk with a stack of (end, path).
        var stack = new Stack<(int end, string path)>();
        stack.Push((count, files));
        for (int i = 1; i < count; i++)
        {
            cancel.ThrowIfCancellationRequested();
            while (i >= stack.Peek().end) stack.Pop();
            var path = Path.Combine(stack.Peek().path, Name(i));
            if (fst[i * 12] != 0)
            {
                Directory.CreateDirectory(path);
                stack.Push(((int)U(fst, i * 12 + 8), path));
                continue;
            }
            long off = U(fst, i * 12 + 4), size = U(fst, i * 12 + 8);
            using (var o = File.Create(path))
            {
                for (long p = 0; p < size; p += 1 << 22)
                    o.Write(disc.Read(off + p, (int)Math.Min(1 << 22, size - p)));
            }
            progress?.Invoke(++done, total, Name(i));
        }
        return done;
    }

    public void Dispose() => disc.Dispose();

    // ---- containers (from the Super Paper Mario launcher's WiiDisc) ----
    abstract class Container : IDisposable
    {
        protected readonly FileStream File;
        public int Holes;  // reads that landed on blocks the image does not store (read as zeros)

        protected Container(string path) => File = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1, FileOptions.RandomAccess);

        public static Container Open(string path)
        {
            using var f = System.IO.File.OpenRead(path);
            var magic = new byte[4];
            f.ReadExactly(magic, 0, 4);
            return Encoding.ASCII.GetString(magic) == "CISO" ? new Ciso(path) : new Iso(path);
        }

        public byte[] Read(long off, int count)
        {
            var dst = new byte[count];
            ReadInto(off, dst);
            return dst;
        }

        protected abstract void ReadInto(long off, Span<byte> dst);

        protected void ReadFile(long fileOff, Span<byte> dst)
        {
            int n = RandomAccess.Read(File.SafeFileHandle, dst, fileOff);
            if (n < dst.Length) { Holes++; dst[n..].Clear(); }  // past the end of a truncated image
        }

        public void Dispose() => File.Dispose();
    }

    sealed class Iso : Container
    {
        public Iso(string path) : base(path) { }
        protected override void ReadInto(long off, Span<byte> dst) => ReadFile(off, dst);
    }

    // CISO: 0x8000-byte header ("CISO", block size LE, one byte per block: 1 = stored), then the stored blocks in order.
    sealed class Ciso : Container
    {
        readonly long blockSize;
        readonly int[] stored;  // block -> index among stored blocks, or -1
        public Ciso(string path) : base(path)
        {
            var h = new byte[0x8000];
            ReadFile(0, h);
            blockSize = BinaryPrimitives.ReadUInt32LittleEndian(h.AsSpan(4, 4));
            if (blockSize < 0x8000 || blockSize % 0x8000 != 0) throw new InvalidDataException("Unsupported CISO block size.");
            stored = new int[0x8000 - 8];
            for (int i = 0, k = 0; i < stored.Length; i++) stored[i] = h[8 + i] == 1 ? k++ : -1;
        }
        protected override void ReadInto(long off, Span<byte> dst)
        {
            while (dst.Length > 0)
            {
                long i = Math.DivRem(off, blockSize, out long o);
                int n = (int)Math.Min(dst.Length, blockSize - o);
                int idx = i < stored.Length ? stored[i] : -1;
                // A CISO leaves out blocks that are all zero: they read as zeros, which is the disc's content.
                if (idx < 0) dst[..n].Clear();
                else ReadFile(0x8000 + idx * blockSize + o, dst[..n]);
                off += n; dst = dst[n..];
            }
        }
    }
}
