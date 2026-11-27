using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

namespace PoliticalEditor {
    internal sealed class Sprite {
        internal string Name, Texture, Mask, Path, SourceRoot;
        internal int Frames, Offset;
        public override string ToString() { return Name + "  ·  " + Path; }
    }

    internal sealed class Sprites {
        internal readonly Mount Mount;
        internal readonly List<Problem> Problems = new List<Problem>();
        private readonly Dictionary<string, List<Sprite>> items = new Dictionary<string, List<Sprite>>(StringComparer.Ordinal);
        private readonly Dictionary<string, Size> dimensions = new Dictionary<string, Size>(StringComparer.Ordinal);
        internal Sprites(Mount mount, CancellationToken token, bool stock) {
            Mount = mount;
            var paths = mount.Paths.Where(IsGfx);
            if (stock && Directory.Exists(mount.Roots.Game)) paths = paths.Concat(Files.Walk(mount.Roots.Game, "interface", false).Where(p => IsGfx(p) && mount.StockVisible(p)));
            foreach (var path in paths.Distinct(Files.Windows)) {
                token.ThrowIfCancellationRequested();
                try {
                    Entry source = mount.Get(path, stock);
                    if (source == null) continue;
                    var doc = new Document(path, TextFile.Decode(source.Data).Text);
                    if (mount.Own.ContainsKey(path)) Problems.AddRange(doc.Problems);
                    var root = doc.Root.Child("spriteTypes");
                    if (root == null) continue;
                    foreach (var item in root.Nodes.Where(n => n.Block)) {
                        bool shield=item.Key=="maskedShieldType";
                        string name = item.Get("name"), texture = item.Get("texturefile",item.Get("textureFile1"));
                        if (name.Length == 0 || texture.Length == 0) continue;
                        string location, mask=null;
                        try {
                            texture = MountedTexture(texture); location = mount.Source(texture);
                            if(shield&&item.Get("textureFile2")!="")mask=MountedTexture(item.Get("textureFile2"));
                        }
                        catch (InvalidDataException) {
                            if (mount.Own.ContainsKey(path)) Problems.Add(new Problem(path, item.Start, L.T("贴图路径无效 ") + texture));
                            continue;
                        }
                        var sprite = new Sprite { Name = name, Texture = texture, Mask=mask, SourceRoot = location, Path = path,
                            Offset = item.Start, Frames = shield?1:Math.Max(1, Document.Int(item.Get("noOfFrames"), 1)) };
                        List<Sprite> candidates;
                        if (!items.TryGetValue(name, out candidates)) items[name] = candidates = new List<Sprite>();
                        candidates.Add(sprite);
                    }
                } catch (IOException e) { Problems.Add(new Problem(path, 0, e.Message, mount.Own.ContainsKey(path))); }
            }
        }
        private string MountedTexture(string text) {
            text=text.Replace('\\','/');
            while(text.Contains("//"))text=text.Replace("//","/");
            text=Files.Relative(text);
            if(Mount.Source(text)=="未找到"&&text.EndsWith(".tga",StringComparison.OrdinalIgnoreCase)) {
                string dds=text.Substring(0,text.Length-4)+".dds";
                if(Mount.Source(dds)!="未找到")return dds;
            }
            return text;
        }
        private static bool IsGfx(string p) { return p.StartsWith("interface/", StringComparison.OrdinalIgnoreCase) && p.EndsWith(".gfx", StringComparison.OrdinalIgnoreCase); }
        internal IList<Sprite> Find(string id) { List<Sprite> found; return id != null && items.TryGetValue(id, out found) ? (IList<Sprite>)found : new Sprite[0]; }
        internal IEnumerable<Sprite> All { get { return items.Values.SelectMany(v => v); } }
        internal Raster Load(Sprite sprite) {
            var entry = Mount.Get(sprite.Texture, true);
            if (entry == null) throw new IOException(L.T("未找到贴图 ") + sprite.Texture);
            return Raster.Decode(entry.Data, Path.GetExtension(sprite.Texture));
        }
        internal Size SizeOf(string id, Size fallback) {
            if(String.IsNullOrEmpty(id))return fallback;
            Size size;
            lock(dimensions) {
                if(!dimensions.TryGetValue(id,out size)) {
                    var sprite=Find(id).FirstOrDefault();
                    byte[] bytes=null;
                    if(sprite!=null)try{var entry=Mount.Get(sprite.Texture,true);if(entry!=null)bytes=entry.Data;}catch(IOException){}
                    size=bytes==null?Size.Empty:Raster.HeaderSize(bytes,Path.GetExtension(sprite.Texture));
                    if(sprite!=null&&size.Width>0)size.Width=Math.Max(1,size.Width/sprite.Frames);
                    dimensions[id]=size;
                }
            }
            return size.Width==0||size.Height==0?fallback:size;
        }
    }

    internal sealed class Raster : IDisposable {
        internal readonly Bitmap Bitmap;
        internal readonly bool HasAlpha;
        internal int Width { get { return Bitmap.Width; } }
        internal int Height { get { return Bitmap.Height; } }
        private Raster(int w, int h, int[] pixels) {
            HasAlpha = pixels.Any(pixel => (uint)pixel >> 24 != 255);
            Bitmap = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            BitmapData locked = Bitmap.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            try {
                for (int y = 0; y < h; ++y) Marshal.Copy(pixels, y * w, IntPtr.Add(locked.Scan0, y * locked.Stride), w);
            } finally { Bitmap.UnlockBits(locked); }
        }
        private Raster(Bitmap bmp) { Bitmap = bmp; HasAlpha = Image.IsAlphaPixelFormat(bmp.PixelFormat); }
        public void Dispose() { Bitmap.Dispose(); }
        internal static Raster Decode(byte[] bytes, string extension) {
            if (extension.Equals(".dds", StringComparison.OrdinalIgnoreCase)) return Dds(bytes);
            if (extension.Equals(".tga", StringComparison.OrdinalIgnoreCase)) return Tga(bytes);
            if (extension.Equals(".png", StringComparison.OrdinalIgnoreCase) || extension.Equals(".bmp", StringComparison.OrdinalIgnoreCase)) {
                using (var stream = new MemoryStream(bytes, false))
                using (var image = Image.FromStream(stream, false, true)) { Dimensions(image.Width, image.Height); return new Raster(new Bitmap(image)); }
            }
            throw new InvalidDataException(L.T("贴图格式未支持 ") + extension);
        }
        private static void Dimensions(int width, int height) {
            if (width < 1 || height < 1 || width > 4096 || height > 4096 || (long)width * height > 9000000L)
                throw new InvalidDataException(L.T("贴图像素尺寸超出 4096 × 4096 或总面积限制。"));
        }
        internal static Size HeaderSize(byte[] bytes,string extension) {
            int w=0,h=0;
            if(extension.Equals(".dds",StringComparison.OrdinalIgnoreCase)&&bytes.Length>=20&&U32(bytes,0)==0x20534444){ w=(int)U32(bytes,16);h=(int)U32(bytes,12); }
            if(extension.Equals(".tga",StringComparison.OrdinalIgnoreCase)&&bytes.Length>=18){ w=U16(bytes,12);h=U16(bytes,14); }
            return w<1||h<1||w>4096||h>4096?Size.Empty:new Size(w,h);
        }
        private static void Need(byte[] bytes, int offset, int count) {
            if (offset < 0 || count < 0 || offset > bytes.Length - count) throw new InvalidDataException(L.T("贴图数据不完整。"));
        }
        private static int Color(int a, int r, int g, int b) { return unchecked((int)((uint)a << 24)) | r << 16 | g << 8 | b; }
        private static int U16(byte[] b, int at) { return b[at] | b[at + 1] << 8; }
        private static uint U32(byte[] b, int at) { return (uint)(b[at] | b[at+1] << 8 | b[at+2] << 16 | b[at+3] << 24); }
        private static Raster Tga(byte[] b) {
            Need(b, 0, 18);
            int kind = b[2], w = U16(b, 12), h = U16(b, 14), bits = b[16], step = bits / 8;
            bool gray = kind == 3 || kind == 11, rle = kind == 10 || kind == 11;
            Dimensions(w, h);
            if (b[1] != 0 || !(kind == 2 || kind == 3 || kind == 10 || kind == 11) ||
                (gray ? bits != 8 : bits != 16 && bits != 24 && bits != 32))
                throw new InvalidDataException(L.T("TGA 需为真彩或灰度的原始/RLE 图像。"));
            int source = 18 + b[0], target = 0;
            bool top = (b[17] & 32) != 0, left = (b[17] & 16) == 0;
            var pixels = new int[w * h];
            while (target < pixels.Length) {
                int count = 1; bool repeat = false;
                if (rle) { Need(b, source, 1); int packet = b[source++]; repeat = (packet & 128) != 0; count = 1 + (packet & 127); }
                if (count > pixels.Length - target) throw new InvalidDataException(L.T("TGA 连续像素数超过图幅。"));
                Need(b, source, (repeat ? 1 : count) * step);
                for (int i = 0; i < count; ++i, ++target) {
                    int at = source + (repeat ? 0 : i * step);
                    int c;
                    if (gray) c = Color(255, b[at], b[at], b[at]);
                    else if (bits == 16) {
                        int rgb = U16(b, at);
                        c = Color((b[17] & 15) == 0 || (rgb & 32768) != 0 ? 255 : 0,
                            ((rgb >> 10) & 31) * 255 / 31, ((rgb >> 5) & 31) * 255 / 31, (rgb & 31) * 255 / 31);
                    } else c = Color(bits == 32 ? b[at+3] : 255, b[at+2], b[at+1], b[at]);
                    int x = target % w, y = target / w;
                    pixels[(top ? y : h - 1 - y) * w + (left ? x : w - 1 - x)] = c;
                }
                source += (repeat ? 1 : count) * step;
            }
            return new Raster(w, h, pixels);
        }
        private static Raster Dds(byte[] b) {
            Need(b, 0, 128);
            if (U32(b, 0) != 0x20534444 || U32(b, 4) != 124 || U32(b, 76) != 32)
                throw new InvalidDataException(L.T("DDS 文件头不完整或格式不符。"));
            int w = (int)U32(b, 16), h = (int)U32(b, 12); Dimensions(w, h);
            uint code = U32(b, 84), flags = U32(b, 80);
            int offset = 128, bc = 0, bits = (int)U32(b, 88);
            uint r = U32(b, 92), g = U32(b, 96), bl = U32(b, 100), a = U32(b, 104);
            if ((flags & 4) != 0) {
                switch (code) {
                    case 0x31545844: bc = 1; break;
                    case 0x33545844: bc = 2; break;
                    case 0x35545844: bc = 3; break;
                    case 0x30315844:
                        Need(b, 128, 20); offset = 148;
                        if (U32(b, 132) != 3 || U32(b, 140) != 1 || (U32(b, 136) & 4) != 0)
                            throw new InvalidDataException(L.T("DDS DX10 需为单张二维贴图。"));
                        uint format = U32(b, 128);
                        if (format == 71 || format == 72) bc = 1;
                        else if (format == 74 || format == 75) bc = 2;
                        else if (format == 77 || format == 78) bc = 3;
                        else if (format == 28 || format == 29) { bits = 32; r = 255; g = 65280; bl = 16711680; a = 4278190080; }
                        else if (format == 87 || format == 91) { bits = 32; r = 16711680; g = 65280; bl = 255; a = 4278190080; }
                        else throw new InvalidDataException(L.T("DDS DX10 像素格式未支持。"));
                        break;
                    default: throw new InvalidDataException(L.T("DDS 请使用 DXT1、DXT3、DXT5 或未压缩 RGB。"));
                }
            }
            var pixels = new int[w*h];
            if (bc == 0) {
                if ((bits != 16 && bits != 24 && bits != 32) || r == 0 || g == 0 || bl == 0)
                    throw new InvalidDataException(L.T("DDS 缺少兼容的 RGB 通道。"));
                int step = bits/8, pitch = w*step;
                if ((U32(b, 8) & 8) != 0 && U32(b, 20) >= pitch && U32(b, 20) <= 65536) pitch = (int)U32(b, 20);
                Need(b, offset, checked(pitch*h));
                for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                    int pos = offset + y*pitch + x*step;
                    uint v = (uint)U16(b, pos);
                    if (step >= 3) v |= (uint)b[pos+2] << 16;
                    if (step == 4) v |= (uint)b[pos+3] << 24;
                    pixels[y*w+x] = Color(a == 0 ? 255 : Scale(v, a), Scale(v, r), Scale(v, g), Scale(v, bl));
                }
            } else {
                int step = bc == 1 ? 8 : 16;
                Need(b, offset, ((w+3)/4)*((h+3)/4)*step);
                var colors = new int[4]; var alpha = new int[8];
                for (int y = 0; y < h; y += 4) for (int x = 0; x < w; x += 4) {
                    int rgb = offset + (bc == 1 ? 0 : 8), c0 = U16(b, rgb), c1 = U16(b, rgb+2);
                    colors[0] = Rgb565(c0); colors[1] = Rgb565(c1);
                    bool four = c0 > c1 || bc != 1;
                    colors[2] = Mix(colors[0], colors[1], four ? 2 : 1, four ? 1 : 1); colors[3] = four ? Mix(colors[0], colors[1], 1, 2) : 0;
                    uint ix = U32(b, rgb+4); ulong ax = 0;
                    if (bc == 3) {
                        alpha[0] = b[offset]; alpha[1] = b[offset+1];
                        int den = alpha[0] > alpha[1] ? 7 : 5;
                        for (int i = 1; i < den; ++i) alpha[i+1] = ((den-i)*alpha[0]+i*alpha[1])/den;
                        if (den == 5) { alpha[6] = 0; alpha[7] = 255; }
                        for (int i = 0; i < 6; ++i) ax |= (ulong)b[offset+2+i] << (i*8);
                    }
                    for (int i = 0; i < 16; ++i) {
                        int px = x+i%4, py = y+i/4;
                        if (px >= w || py >= h) continue;
                        int color = colors[(int)(ix >> (i*2) & 3)];
                        if (bc == 2) color = (color & 0xffffff) | ((b[offset+i/2] >> (4*(i%2)) & 15) * 17 << 24);
                        if (bc == 3) color = (color & 0xffffff) | (alpha[(int)(ax >> (i*3) & 7)] << 24);
                        pixels[py*w+px] = color;
                    }
                    offset += step;
                }
            }
            return new Raster(w, h, pixels);
        }
        private static int Scale(uint number, uint mask) {
            while ((mask & 1) == 0) { mask >>= 1; number >>= 1; }
            return (int)(((ulong)(number & mask) * 255) / mask);
        }
        private static int Rgb565(int x) { return Color(255, ((x >> 11) & 31)*255/31, ((x >> 5) & 63)*255/63, (x & 31)*255/31); }
        private static int Mix(int a, int b, int first, int second) {
            return Color(255, Channel(a,b,16,first,second), Channel(a,b,8,first,second), Channel(a,b,0,first,second));
        }
        private static int Channel(int a, int b, int shift, int first, int second) {
            return ((a >> shift & 255)*first + (b >> shift & 255)*second)/(first+second);
        }
    }
}
