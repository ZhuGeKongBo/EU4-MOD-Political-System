using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace PoliticalEditor {
    internal static class Files {
        internal static readonly StringComparer Windows = StringComparer.OrdinalIgnoreCase;
        internal static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
        internal static string Relative(string path) {
            if (String.IsNullOrWhiteSpace(path) || Path.IsPathRooted(path)) throw new InvalidDataException(L.T("需要项目相对路径。"));
            path = path.Replace('\\', '/');
            foreach (var part in path.Split('/')) {
                if (part.Length == 0 || part == "." || part == ".." || part.EndsWith(".") || part.EndsWith(" ") ||
                    part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 ||
                    Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])(\.|$)", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant))
                    throw new InvalidDataException(L.T("Windows 路径无效 ") + path);
            }
            return path;
        }
        internal static bool IsId(string id) { return id != null && Regex.IsMatch(id, @"^[A-Za-z_][A-Za-z0-9_]{0,79}$"); }
        internal static string Normalize(string path) { return Path.GetFullPath(path); }
        internal static bool Under(string path, string root) {
            if (String.IsNullOrWhiteSpace(root)) return false;
            var p = Normalize(path).TrimEnd('\\', '/');
            var r = Normalize(root).TrimEnd('\\', '/');
            return Windows.Equals(p, r) || p.StartsWith(r + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
        }
        internal static void NoLinks(string path) {
            for (string p = Normalize(path); !String.IsNullOrWhiteSpace(p); p = Path.GetDirectoryName(p)) {
                FileAttributes attributes;
                try { attributes = File.GetAttributes(p); }
                catch (FileNotFoundException) { continue; }
                catch (DirectoryNotFoundException) { continue; }
                if ((attributes & FileAttributes.ReparsePoint) != 0) throw new IOException(L.T("路径包含链接或联接点 ") + p);
            }
        }
        internal static string At(string root, string relative) {
            var path = Normalize(Path.Combine(root, Relative(relative)));
            if (!Under(path, root)) throw new IOException(L.T("路径超出允许目录。"));
            return path;
        }
        internal static IEnumerable<string> Walk(string root, string relative, bool recurse = true) {
            string first = String.IsNullOrEmpty(relative) ? Normalize(root) : At(root, relative);
            if (!Directory.Exists(first)) yield break;
            NoLinks(first);
            var pending = new Stack<string>(); pending.Push(first);
            while (pending.Count > 0) {
                var folder = pending.Pop();
                foreach (var item in Directory.EnumerateFileSystemEntries(folder).OrderBy(p => p, Windows)) {
                    var flags = File.GetAttributes(item);
                    if ((flags & FileAttributes.ReparsePoint) != 0) continue;
                    if ((flags & FileAttributes.Directory) != 0) { if (recurse) pending.Push(item); }
                    else yield return Relative(item.Substring(Normalize(root).TrimEnd('\\', '/').Length + 1));
                }
            }
        }
        internal static byte[] Read(string path, long max = 32 * 1024 * 1024) {
            NoLinks(path);
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete)) {
                if (stream.Length > max) throw new IOException(L.T("文件超过编辑器读取范围 ") + path);
                var bytes = new byte[(int)stream.Length];
                for (int offset = 0; offset < bytes.Length;) {
                    int count = stream.Read(bytes, offset, bytes.Length - offset);
                    if (count == 0) throw new IOException(L.T("读取时文件发生变化 ") + path);
                    offset += count;
                }
                return bytes;
            }
        }
        internal static string Hash(byte[] bytes) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", ""); }
        internal static string FileHash(string path) {
            NoLinks(path);
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
            using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "");
        }
        internal static bool Same(byte[] a, byte[] b) { return ReferenceEquals(a, b) || (a != null && b != null && a.SequenceEqual(b)); }
        internal static void Atomic(string path, byte[] bytes) {
            NoLinks(path);
            string parent = Path.GetDirectoryName(Normalize(path));
            Directory.CreateDirectory(parent);
            string temporary = Path.Combine(parent, ".pf-" + Guid.NewGuid().ToString("N") + ".tmp");
            try {
                using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { stream.Write(bytes, 0, bytes.Length); stream.Flush(true); }
                if (File.Exists(path)) File.Replace(temporary, path, null); else File.Move(temporary, path);
            } finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
        internal static byte[] Resource(string name) {
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name)) {
                if (stream == null) return null;
                using (var bytes = new MemoryStream()) { stream.CopyTo(bytes); return bytes.ToArray(); }
            }
        }
        internal static JavaScriptSerializer Json() { return new JavaScriptSerializer { MaxJsonLength = 320 * 1024 * 1024, RecursionLimit = 64 }; }
    }

    public sealed class RootMap {
        public string Game { get; set; }
        public string User { get; set; }
        public string Mod { get; set; }
        public string[] Dependencies { get; set; }
        public RootMap() { Game = User = Mod = ""; Dependencies = new string[0]; }
        internal RootMap Copy() { return new RootMap { Game = Game, User = User, Mod = Mod, Dependencies = (Dependencies ?? new string[0]).ToArray() }; }
        internal static RootMap Detect() {
            string user = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Paradox Interactive", "Europa Universalis IV");
            var steam = new List<string> { Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam") };
            try {
                using (var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam")) {
                    var path = key == null ? null : key.GetValue("SteamPath") as string;
                    if (!String.IsNullOrWhiteSpace(path)) steam.Insert(0, path);
                }
            } catch (System.Security.SecurityException) { } catch (UnauthorizedAccessException) { }
            var libraries = new List<string>(steam);
            foreach (var client in steam) {
                try {
                    var index = Path.Combine(client, "steamapps", "libraryfolders.vdf");
                    if (!File.Exists(index)) continue;
                    foreach (Match entry in Regex.Matches(File.ReadAllText(index), @"(?im)^\s*""path""\s*""([^""]+)"""))
                        libraries.Add(entry.Groups[1].Value.Replace(@"\\", @"\"));
                } catch (IOException) { } catch (UnauthorizedAccessException) { }
            }
            foreach (var drive in DriveInfo.GetDrives().Where(candidate => candidate.DriveType == DriveType.Fixed)) {
                libraries.Add(Path.Combine(drive.Name, "Steam"));
                libraries.Add(Path.Combine(drive.Name, "SteamLibrary"));
            }
            var game = libraries.Distinct(Files.Windows)
                .Select(path => Path.Combine(path, "steamapps", "common", "Europa Universalis IV"))
                .FirstOrDefault(path => File.Exists(Path.Combine(path, "eu4.exe")));
            return new RootMap { User = user, Game = game ?? "" };
        }
    }

    public sealed class PackedFile {
        public string Path { get; set; }
        public string Data { get; set; }
        public string Original { get; set; }
        public bool BaseMod { get; set; }
        public bool HasOriginal { get; set; }
    }
    public sealed class ProjectData {
        public int Schema { get; set; }
        public string Title { get; set; }
        public RootMap Roots { get; set; }
        public string Type { get; set; }
        public string Instance { get; set; }
        public string Country { get; set; }
        public string Date { get; set; }
        public string Language { get; set; }
        public int Revision { get; set; }
        public string TargetHash { get; set; }
        public string ExportFolder { get; set; }
        public string ExportFingerprint { get; set; }
        public PackedFile[] Files { get; set; }
        public QaRecord Qa { get; set; }
    }

    internal sealed class Entry {
        internal readonly byte[] Data, Original;
        internal readonly bool BaseMod;
        internal Entry(byte[] data, byte[] original, bool baseMod) { Data = data; Original = original; BaseMod = baseMod; }
        internal bool Changed { get { return !Files.Same(Data, Original); } }
    }

    internal sealed class Project {
        internal ProjectData Data;
        internal Dictionary<string, Entry> Content = new Dictionary<string, Entry>(Files.Windows);
        private readonly Stack<Tuple<Dictionary<string, Entry>, string>> undo = new Stack<Tuple<Dictionary<string, Entry>, string>>();
        private readonly Stack<Tuple<Dictionary<string, Entry>, string>> redo = new Stack<Tuple<Dictionary<string, Entry>, string>>();
        internal string Pathname, SavedHash;
        internal bool Dirty;
        internal bool CanUndo { get { return undo.Count > 0; } }
        internal bool CanRedo { get { return redo.Count > 0; } }
        internal Project(RootMap roots) {
            Data = new ProjectData { Schema = 1, Roots = roots.Copy(), Title = L.T("政治体系项目"), Type = "", Instance = "", Country = "ASK", Date = "1444.1.1",
                Language = "l_english", Qa = new QaRecord() };
        }
        internal static Project Import(RootMap roots, CancellationToken cancel) {
            if (!Directory.Exists(roots.Mod) || !File.Exists(Files.At(roots.Mod, "descriptor.mod"))) throw new IOException(L.T("请选择包含 descriptor.mod 的 Mod 文件夹。"));
            var project = new Project(roots);
            long size = 0;
            foreach (var key in Files.Walk(roots.Mod, "")) {
                cancel.ThrowIfCancellationRequested();
                if (!IsModFile(key)) continue;
                var bytes = Files.Read(Files.At(roots.Mod, key));
                size += bytes.Length;
                if (size > 128L * 1024 * 1024 || project.Content.Count >= 8192) throw new IOException(L.T("此 Mod 超过单项目快照容量。请选择框架工作副本。"));
                project.Content.Add(key, new Entry(bytes, bytes, true));
            }
            return project;
        }
        internal static bool IsModFile(string key) {
            if (Files.Windows.Equals(key, "descriptor.mod")) return true;
            if (key.StartsWith(".", StringComparison.Ordinal) || key.EndsWith(".pfproject.json", StringComparison.OrdinalIgnoreCase)) return false;
            switch (System.IO.Path.GetExtension(key).ToLowerInvariant()) {
                case ".exe": case ".dll": case ".ps1": case ".cmd": case ".bat": case ".lnk": case ".dmp": return false;
            }
            return key.IndexOf('/') > 0;
        }
        internal static Project Open(string path) {
            byte[] bytes = Files.Read(path, 300 * 1024 * 1024);
            var data = Files.Json().Deserialize<ProjectData>(TextFile.Decode(bytes).Text);
            if (data == null || data.Schema != 1 || data.Roots == null || data.Files == null || data.Files.Length > 8192) throw new InvalidDataException(L.T("项目文件版本或内容不受支持。"));
            var project = new Project(data.Roots) { Data = data, Pathname = Files.Normalize(path), SavedHash = Files.Hash(bytes) };
            if (project.Data.Qa == null) project.Data.Qa = new QaRecord();
            long size = 0;
            foreach (var file in data.Files) {
                string key = Files.Relative(file.Path);
                byte[] content = Convert.FromBase64String(file.Data);
                byte[] original = file.Original == null ? (file.BaseMod || file.HasOriginal ? content : null) : Convert.FromBase64String(file.Original);
                size += content.Length + (file.Original == null ? 0 : original.Length);
                if (size > 240L * 1024 * 1024) throw new IOException(L.T("此项目快照超出可编辑容量。"));
                project.Content.Add(key, new Entry(content, original, file.BaseMod));
            }
            data.Files = null;
            return project;
        }
        internal void ProtectOutput(string path) {
            Files.NoLinks(path);
            var roots = new[] { Data.Roots.Game, Data.Roots.Mod }.Concat(Data.Roots.Dependencies ?? new string[0]);
            if (roots.Any(root => Files.Under(path, root))) throw new IOException(L.T("请选择源安装树和源 Mod 以外的工作位置。"));
        }
        internal void Save(string path, bool autosave = false) {
            ProtectOutput(path);
            string full = Files.Normalize(path);
            if (!autosave && File.Exists(full) && Files.Windows.Equals(Pathname, full) && SavedHash != Files.FileHash(full))
                throw new IOException(L.T("此项目已由其他程序修改。请另存工作副本。"));
            Data.Files = Content.Select(pair => new PackedFile { Path = pair.Key, Data = Convert.ToBase64String(pair.Value.Data),
                Original = Files.Same(pair.Value.Data, pair.Value.Original) ? null : pair.Value.Original == null ? null : Convert.ToBase64String(pair.Value.Original),
                BaseMod = pair.Value.BaseMod, HasOriginal = pair.Value.Original != null }).ToArray();
            try {
                var bytes = Files.Utf8.GetBytes(Files.Json().Serialize(Data));
                Files.Atomic(full, bytes);
                if (!autosave) { Pathname = full; SavedHash = Files.Hash(bytes); Dirty = false; }
            } finally { Data.Files = null; }
        }
        internal Dictionary<string, Entry> Draft() { return new Dictionary<string, Entry>(Content, Files.Windows); }
        internal void Apply(Dictionary<string, Entry> draft, string name) {
            if (Content.Count == draft.Count && Content.All(p => draft.ContainsKey(p.Key) && Files.Same(p.Value.Data, draft[p.Key].Data))) return;
            if (undo.Count >= 110) {
                var recent = undo.Take(100).Reverse().ToArray(); undo.Clear(); foreach (var entry in recent) undo.Push(entry);
            }
            undo.Push(Tuple.Create(Content, name)); redo.Clear(); Content = draft; Touch();
        }
        internal string Undo(bool forward) {
            var source = forward ? redo : undo; var target = forward ? undo : redo;
            if (source.Count == 0) return "";
            var next = source.Pop(); target.Push(Tuple.Create(Content, next.Item2)); Content = next.Item1; Touch(); return next.Item2;
        }
        internal void Touch() { ++Data.Revision; Dirty = true; }
        internal static void Put(Dictionary<string, Entry> draft, string path, byte[] data, Entry source = null) {
            path = Files.Relative(path);
            Entry old;
            bool inDraft = draft.TryGetValue(path, out old);
            draft[path] = new Entry(data, inDraft ? old.Original : source == null ? null : source.Data, inDraft ? old.BaseMod : source != null && source.BaseMod);
        }
        internal static void PutText(Dictionary<string, Entry> draft, string path, string text, Entry source) {
            Put(draft, path, source == null ? TextFile.Utf8(text, path.EndsWith(".yml", StringComparison.OrdinalIgnoreCase)) : TextFile.Decode(source.Data).Encode(text), source);
        }
        internal IEnumerable<string> SourceConflicts() {
            if (!Directory.Exists(Data.Roots.Mod)) yield break;
            foreach(var path in Files.Walk(Data.Roots.Mod,"").Where(IsModFile))
                if(!Content.ContainsKey(path)||!Content[path].BaseMod)yield return path;
            foreach (var pair in Content) {
                if (!pair.Value.BaseMod || pair.Value.Original == null) continue;
                string file = Files.At(Data.Roots.Mod, pair.Key);
                if (!File.Exists(file) || Files.FileHash(file) != Files.Hash(pair.Value.Original)) yield return pair.Key;
            }
        }
    }

    internal sealed class Mount {
        internal readonly RootMap Roots;
        internal readonly Dictionary<string, Entry> Own;
        internal readonly Dictionary<string, string> Dependencies = new Dictionary<string, string>(Files.Windows);
        internal readonly List<string> Removed = new List<string>();
        internal readonly List<Problem> Problems = new List<Problem>();
        internal readonly List<string> Mask = new List<string>();
        internal Mount(RootMap roots, Dictionary<string, Entry> own, CancellationToken token) {
            Roots = roots.Copy(); Own = own;
            foreach (var folder in Roots.Dependencies ?? new string[0]) {
                token.ThrowIfCancellationRequested();
                if (!Directory.Exists(folder)) { Problems.Add(new Problem("", 0, L.T("依赖目录不存在 ") + folder)); continue; }
                var descriptor = Files.At(folder, "descriptor.mod");
                if (File.Exists(descriptor)) ApplyMask(new Document("descriptor.mod", TextFile.Decode(Files.Read(descriptor)).Text));
                foreach (var path in Files.Walk(folder, "").Where(p => Project.IsModFile(p) && !Files.Windows.Equals(p, "descriptor.mod"))) {
                    token.ThrowIfCancellationRequested();
                    if (Dependencies.ContainsKey(path)) Removed.Add(path + L.T("  路径覆盖"));
                    Dependencies[path] = folder;
                }
            }
            Entry local;
            if (Own.TryGetValue("descriptor.mod", out local)) ApplyMask(new Document("descriptor.mod", TextFile.Decode(local.Data).Text));
        }
        private void ApplyMask(Document description) {
            foreach (var node in description.Root.Nodes.Where(n => n.Key == "replace_path" && n.Value != null)) {
                var prefix = Files.Relative(node.Value.TrimEnd('/', '\\')) + "/";
                Mask.Add(prefix);
                var hidden = Dependencies.Keys.Where(key => key.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)).ToArray();
                foreach (var key in hidden) { Dependencies.Remove(key); Removed.Add(key + "  replace_path"); }
            }
        }
        internal bool StockVisible(string path) { return !Mask.Any(prefix => path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)); }
        internal IEnumerable<string> Paths { get { return Own.Keys.Concat(Dependencies.Keys).Distinct(Files.Windows).OrderBy(p => p, Files.Windows); } }
        internal Entry Get(string relative, bool stock = false) {
            relative = Files.Relative(relative); Entry value;
            if (Own.TryGetValue(relative, out value)) return value;
            string root;
            if (!Dependencies.TryGetValue(relative, out root)) root = stock && StockVisible(relative) ? Roots.Game : "";
            if (String.IsNullOrEmpty(root)) return null;
            string absolute = Files.At(root, relative);
            if (!File.Exists(absolute)) return null;
            var bytes = Files.Read(absolute); return new Entry(bytes, bytes, false);
        }
        internal string Source(string path) {
            Entry entry; string root;
            if (Own.TryGetValue(path, out entry)) return entry.BaseMod && !entry.Changed ? Roots.Mod : "项目草稿";
            if (Dependencies.TryGetValue(path, out root)) return root;
            if (!String.IsNullOrEmpty(Roots.Game) && StockVisible(path) && File.Exists(Files.At(Roots.Game, path))) return Roots.Game;
            return "未找到";
        }
    }
}
