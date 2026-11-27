using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace PoliticalEditor {
    internal static class NativeValidation {
        internal const string Version = "0.9.2";
        internal static string LastSummary;
        internal static List<Problem> Run(Model model, string temporaryRoot) {
            var binary = Files.Resource("validator.exe");
            if (binary == null) throw new IOException(L.T("此构建缺少原生校验器资源。"));
            var problems = new List<Problem>();
            string root = Path.Combine(temporaryRoot,"pf-validate-" + Guid.NewGuid().ToString("N"));
            Files.NoLinks(root); Directory.CreateDirectory(root);
            try {
                var list = new StringBuilder();
                var filenames = new HashSet<string>(Files.Windows);
                int index = 0;
                Action<string,char> write = (path,kind) => {
                    if (String.IsNullOrEmpty(path) || !filenames.Add(path)) return;
                    if (Files.Utf8.GetByteCount(path) >= 260 || path.IndexOf('\t') >= 0) throw new IOException(L.T("文件路径超过框架校验范围 ") + path);
                    var entry = model.Mount.Get(path,true);
                    if (entry == null) return;
                    string local = "input_" + ++index;
                    // Feed the same bytes the game will mount.
                    File.WriteAllBytes(Path.Combine(root,local),entry.Data);
                    list.Append(kind).Append('\t').Append(path).Append('\t').Append(local).Append('\n');
                };
                foreach (var path in model.Mount.Paths.Where(p=>p.EndsWith(".txt"))) {
                    if (path.StartsWith(Model.TypeDir)) write(path,'D');
                    else if (path.StartsWith(Model.LayoutDir)) write(path,'L');
                    else if (path.StartsWith("common/political_systems/localisation/")) write(path,'P');
                    else if (path == "common/political_systems/instances.txt") write(path,'R');
                    else if (path.StartsWith(Model.HistoryDir)) write(path,'H');
                }
                foreach (var layout in model.Layouts.Values.Concat(model.Huds.Values)) {
                    write(layout.Node.Get("gui_file"),'G'); write(layout.Node.Get("gfx_file"),'G');
                }
                if (model.Huds.Count > 0) write("interface/ps_instance_selector.gui",'G');
                foreach (var path in model.Huds.Values.Select(h=>h.Node.Get("gfx_file")).Distinct(Files.Windows)) {
                    var doc = model.Doc(path);
                    var sprites = doc == null ? null : doc.Root.Child("spriteTypes");
                    if (sprites == null) continue;
                    foreach (var texture in sprites.Nodes.Select(n=>n.Get("texturefile")).Where(t=>t!="" && filenames.Add(t))) {
                        if (Files.Utf8.GetByteCount(texture) < 260 && model.Mount.Source(texture) != "未找到")
                            list.Append("T\t").Append(texture).Append("\t0\n");
                    }
                }
                File.WriteAllText(Path.Combine(root,"catalog.tsv"),list.ToString(),Files.Utf8);
                string exe = Path.Combine(root,"pf-content-check.exe");
                File.WriteAllBytes(exe,binary);
                string id = model.Type == null ? "-" : model.Type.Node.Key;
                if (id != "-" && !Files.IsId(id)) throw new InvalidDataException(L.T("类型 ID 超出验证器范围。"));
                var info = new ProcessStartInfo(exe,id) { WorkingDirectory=root, UseShellExecute=false, CreateNoWindow=true,
                    RedirectStandardOutput=true, RedirectStandardError=true, StandardOutputEncoding=Encoding.UTF8, StandardErrorEncoding=Encoding.UTF8 };
                using (var process = Process.Start(info)) {
                    var stdout = process.StandardOutput.ReadToEndAsync(); var stderr = process.StandardError.ReadToEndAsync();
                    if (!process.WaitForExit(18000)) { process.Kill(); process.WaitForExit(3000); throw new IOException(L.T("原生内容校验超时。")); }
                    Task.WaitAll(stdout,stderr);
                    LastSummary = stdout.Result.Trim();
                    foreach (var line in Document.ViewText(stdout.Result).Split('\n')) {
                        var split = line.Split(new[] { '\t' },4);
                        if (split.Length != 4) continue;
                        int row = Math.Max(1,Document.Int(split[2],1));
                        if (split[0] == "error") problems.Add(NativeDiagnostics.Read(model,split[1],row,split[3]));
                        else if (split[0] == "warning") problems.Add(NativeDiagnostics.Warning(model,split[1],row));
                    }
                    if (process.ExitCode != 0 && !problems.Any(p=>p.Error)) problems.Add(new Problem("",0,L.T("校验器返回 ") + process.ExitCode + " " + stderr.Result.Trim()));
                }
                return problems.GroupBy(p=>p.File+"\n"+p.Message,StringComparer.Ordinal).Select(g=>g.First()).ToList();
            } finally {
                Files.NoLinks(root);
                if (Directory.Exists(root) && Files.Under(root,temporaryRoot)) Directory.Delete(root,true);
            }
        }
    }

    public sealed class ExportFile {
        public string File { get; set; }
        public string Sha256 { get; set; }
        public string Destination { get; set; }
        public string ExpectedOriginal { get; set; }
    }
    public sealed class ExportRecord {
        public string Editor { get; set; }
        public string Pack { get; set; }
        public string Mode { get; set; }
        public string DateUtc { get; set; }
        public string Eu4Version { get; set; }
        public string Eu4ExeSha256 { get; set; }
        public string Runtime { get; set; }
        public string RuntimeSha256 { get; set; }
        public string Adapter { get; set; }
        public string Type { get; set; }
        public string Instance { get; set; }
        public string Country { get; set; }
        public string HistoricalDay { get; set; }
        public int Revision { get; set; }
        public string[] Dependencies { get; set; }
        public ExportFile[] Content { get; set; }
        public string[] Warnings { get; set; }
    }
    internal sealed class ExportPlan {
        internal readonly Project Project;
        internal readonly Model Model;
        internal readonly bool Full;
        internal readonly string Parent, Target, Id;
        internal readonly Dictionary<string, byte[]> Content = new Dictionary<string, byte[]>(Files.Windows);
        internal readonly List<Problem> Problems = new List<Problem>();
        internal readonly Dictionary<string, string> Destinations = new Dictionary<string, string>(Files.Windows);
        private readonly Dictionary<string, Entry> before;
        internal ExportPlan(Project project, Model model, string parent, string id, bool full, string temp, bool ignoreSource = false) {
            if (!Files.IsId(id) || id.Length > 50) throw new InvalidDataException(L.T("输出名请使用不超过 50 位的英文字母、数字或下划线。"));
            Project=project; Model=model; Full=full; Parent=Files.Normalize(parent); Target=Files.At(Parent,id); Id=id; before=project.Content;
            Problems.AddRange(model.Problems);
            if (model.Window==null) Problems.Add(new Problem("",0,L.T("当前体系缺少可导出的原生主窗。")));
            if (!Directory.Exists(model.Mount.Roots.Game)) Problems.Add(new Problem("",0,L.T("请选定 EU4 安装目录后再导出。")));
            if (model.Version != NativeValidation.Version) Problems.Add(new Problem("",0,L.T("本机构建 ") + model.Version + L.T("，校验适配版本 ") + NativeValidation.Version + L.T("。请在同版运行时验收。"),false));
            if (File.Exists(Target) || Directory.Exists(Target)) Problems.Add(new Problem("",0,L.T("输出目录已存在 ") + Target + L.T("。请换一个输出名或位置。")));
            project.ProtectOutput(Target);
            if (!String.IsNullOrWhiteSpace(project.Data.Roots.User)&&Files.Under(Target,Path.Combine(project.Data.Roots.User, "mod")))
                throw new IOException(L.T("请选择 EU4 用户 mod 目录以外的位置，核对后再由作者复制。"));
            foreach (var path in project.SourceConflicts())
                Problems.Add(new Problem(path,0,L.T("源文件已发生外部变化 ") + path + (ignoreSource ? L.T("。本次采用已确认的项目快照。") : L.T("。请查看差异并确认快照。")),!ignoreSource));
            if (!Problems.Any(p=>p.Error)) Problems.AddRange(NativeValidation.Run(model,temp));
            if (Full) {
                string prefix = "mod/" + id + "/";
                foreach (var path in model.Mount.Paths.Where(path=>Project.IsModFile(path) && !Files.Windows.Equals(path,"descriptor.mod"))) {
                    string key=prefix+path;
                    Content.Add(key,model.Mount.Get(path).Data); Destinations[key]=L.T("EU4 用户目录/")+key;
                }
                string descriptor = Description(model.Mount,id);
                string internalPath = prefix+"descriptor.mod", outerPath = "mod/"+id+".mod";
                Content.Add(internalPath,TextFile.Utf8(descriptor));
                Content.Add(outerPath,TextFile.Utf8(descriptor+"path="+Document.Quote("mod/"+id)+"\n"));
                Destinations[internalPath]=L.T("EU4 用户目录/")+internalPath; Destinations[outerPath]=L.T("EU4 用户目录/")+outerPath;
            } else {
                foreach (var pair in project.Content.Where(pair=>pair.Value.Changed && Project.IsModFile(pair.Key))) {
                    string key="files/"+pair.Key;
                    Content.Add(key,pair.Value.Data); Destinations[key]=L.T("目标 Mod/")+pair.Key;
                }
                if (Content.Count==0) Problems.Add(new Problem("",0,L.T("此项目没有新增或修改的内容。")));
            }
        }
        private static string Description(Mount mount, string id) {
            var existing = mount.Get("descriptor.mod");
            var d = new Document("descriptor.mod",existing==null?"":TextFile.Decode(existing.Data).Text);
            var filtered = d.Root.Nodes.Where(n => n.Key!="path" && n.Key!="archive" && n.Key!="remote_file_id" && n.Key!="dependencies" &&
                n.Key!="name" && n.Key!="replace_path").Select(d.At);
            string description = "name = " + Document.Quote("Political Editor Test " + id) + "\n" + String.Join("\n",filtered) + "\n";
            if (d.Root.Child("supported_version")==null) description+="supported_version = \"1.37.*\"\n";
            foreach (var mask in mount.Mask.Distinct(Files.Windows)) description += "replace_path = " + Document.Quote(mask.TrimEnd('/')) + "\n";
            return description;
        }
        internal string Diff(string key) {
            string path = Full ? key.Substring(("mod/"+Id).Length).TrimStart('/') : key.Substring("files/".Length);
            Entry source;
            if (!before.TryGetValue(path,out source)) source=Model.Mount.Get(path);
            byte[] old=source==null?null:source.Original;
            if (!Full || old == null || !Files.Same(old,Content[key])) return EditorDiff.Create(path,old,Content[key]);
            return L.T("原文件字节相同\n") + path + "\nSHA-256 " + Files.Hash(old);
        }
        internal string Write(string temp) {
            if (Problems.Any(p=>p.Error)) throw new InvalidDataException(L.T("请先修复导出预检中的错误。"));
            if (!ReferenceEquals(before,Project.Content)) throw new IOException(L.T("项目已变化。请刷新预检。"));
            Project.ProtectOutput(Target); Files.NoLinks(Parent);
            if (Directory.Exists(Target)||File.Exists(Target)) throw new IOException(L.T("同名输出已存在 ") + Target);
            Directory.CreateDirectory(Parent);
            string stage = Path.Combine(Parent,".pf-stage-"+Guid.NewGuid().ToString("N"));
            var roots = Project.Data.Roots;
            string exe = Path.Combine(roots.Game,"eu4.exe"), dll = Path.Combine(roots.Game,"anvil","plugins","great-khan-native","great_khan_native.dll");
            var record = new ExportRecord { Editor=Program.Version, Adapter=NativeValidation.Version, Pack=Id, Mode=Full?"full":"patch",
                DateUtc=DateTime.UtcNow.ToString("o"), Revision=Project.Data.Revision, Runtime=Model.Version, Eu4Version=Fingerprints.GameVersion(roots.Game),
                Eu4ExeSha256=File.Exists(exe)?Files.FileHash(exe):"",RuntimeSha256=File.Exists(dll)?Files.FileHash(dll):"",
                Type=Model.Type.Node.Key, Instance=Model.Instance==null?"":Model.Instance.Id, Country=Model.Country, HistoricalDay=Model.Date,
                Dependencies=(roots.Dependencies??new string[0]).Select(p=>Path.GetFileName(p.TrimEnd('\\','/'))).ToArray(),
                Warnings=Problems.Where(p=>!p.Error).Select(p=>p.File + " " + p.Message).ToArray(),
                Content=Content.OrderBy(p=>p.Key,Files.Windows).Select(pair=>{
                    string rel=Full?pair.Key.Substring(("mod/"+Id).Length).TrimStart('/'):pair.Key.Substring(6);
                    var source=Model.Mount.Get(rel);
                    return new ExportFile { File=pair.Key, Destination=Destinations[pair.Key], Sha256=Files.Hash(pair.Value),
                        ExpectedOriginal=source==null || source.Original==null?"":Files.Hash(source.Original) };
                }).ToArray() };
            try {
                Directory.CreateDirectory(stage); Files.NoLinks(stage);
                foreach (var pair in Content) {
                    string path = Files.At(stage,pair.Key); Files.NoLinks(path); Directory.CreateDirectory(Path.GetDirectoryName(path));
                    File.WriteAllBytes(path,pair.Value);
                }
                foreach (var file in record.Content) {
                    string path = Files.At(stage,file.File);
                    if (Files.FileHash(path)!=file.Sha256) throw new IOException(L.T("暂存后的文件校验失败 ") + file.File);
                    bool script = path.EndsWith(".gui") || path.EndsWith(".gfx") || path.EndsWith(".mod") ||
                        file.File.IndexOf("/political_system",StringComparison.OrdinalIgnoreCase)>=0 && path.EndsWith(".txt");
                    if (script) {
                        var d=new Document(file.File,TextFile.Decode(Files.Read(path)).Text);
                        if (d.Problems.Count!=0) throw new InvalidDataException(file.File + L.T(" 重读失败 ") + d.Problems[0].Message);
                    }
                }
                if(Full) {
                    var copiedRoots=roots.Copy();copiedRoots.Mod=Files.At(stage,"mod/"+Id);copiedRoots.Dependencies=new string[0];
                    var imported=Project.Import(copiedRoots,CancellationToken.None);
                    var stagedModel=new Model(new Mount(copiedRoots,imported.Content,CancellationToken.None),record.Type,record.Instance,record.Country,
                        record.HistoricalDay,Model.Language,CancellationToken.None,Model.Images);
                    var failed=stagedModel.Problems.Concat(NativeValidation.Run(stagedModel,temp)).FirstOrDefault(p=>p.Error);
                    if(failed!=null)throw new InvalidDataException(L.T("导出暂存的原生校验未通过 ")+failed.File+" "+failed.Message);
                }
                string manifest = Path.Combine(stage,"export-manifest.json");
                byte[] list = TextFile.Utf8(Files.Json().Serialize(record));
                File.WriteAllBytes(manifest,list);
                string instructions = L.T("EU4 政治体系编辑器 ") + Program.Version + "\r\n\r\n" +
                    (Full ? L.T("将 mod 下的 ") + Id + L.T(".mod 和 ") + Id + L.T(" 文件夹一起复制到 EU4 用户目录的 mod 目录。\r\n") +
                        L.T("外置 .mod 应指向 path=\"mod/") + Id + L.T("\"。如果使用不同的用户目录，请核对该目录下的文件夹。\r\n") +
                        L.T("请在启动器创建隔离播放集，仅启用本测试内容及必需的非重复依赖。避免和原框架 Mod 的同一组内容重复启用。\r\n")
                        : L.T("files 下的目录层级相对于目标 Mod 根目录。按清单核对原文件 SHA-256 后备份并复制。\r\n差异目标包含完整文件。若旧文件不同，请先合并并重做验证。\r\n")) +
                    L.T("运行时需使用本机授权安装的 Political Framework ") + NativeValidation.Version + L.T(" 兼容版。此包仅含内容文件。\r\n") +
                    L.T("预览国家 ") + Model.Country + L.T("，历史日期 ") + Model.Date + L.T("。用 1444 战役核对适用记录。\r\n") +
                    L.T("编辑器的 QA 可按同一导出指纹记录日志及窗口观察。\r\n");
                File.WriteAllBytes(Path.Combine(stage,"INSTALL.txt"),TextFile.Utf8(instructions,true));
                var verified = Files.Json().Deserialize<ExportRecord>(TextFile.Decode(Files.Read(manifest)).Text);
                if (verified.Revision!=record.Revision || verified.Content.Length!=record.Content.Length) throw new IOException(L.T("重读导出清单失败。"));
                Files.NoLinks(Target);
                Directory.Move(stage,Target);
                Project.Data.ExportFolder=Target; Project.Data.ExportFingerprint=Files.Hash(list); Project.Dirty=true;
                return Target;
            } finally {
                Files.NoLinks(stage);
                if (Directory.Exists(stage) && Files.Under(stage,Parent)) Directory.Delete(stage,true);
            }
        }
    }

    public sealed class LogCursor {
        public string Path { get; set; }
        public string CreatedUtc { get; set; }
        public long Offset { get; set; }
        public string HeadHash { get; set; }
        public int HeadLength { get; set; }
    }
    public sealed class QaItem {
        public string Id { get; set; }
        public string Outcome { get; set; }
        public string Note { get; set; }
        public string Evidence { get; set; }
    }
    public sealed class QaRecord {
        public List<QaSession> Sessions { get; set; }
        public QaRecord() { Sessions = new List<QaSession>(); }
    }
    public sealed class QaSession {
        public string Fingerprint { get; set; }
        public string ExportFolder { get; set; }
        public int Revision { get; set; }
        public string GameVersion { get; set; }
        public string RuntimeVersion { get; set; }
        public string Instance { get; set; }
        public string Country { get; set; }
        public string Resolution { get; set; }
        public string Scale { get; set; }
        public string EnabledMods { get; set; }
        public string StartUtc { get; set; }
        public string EndUtc { get; set; }
        public RootMap Roots { get; set; }
        public string Exit { get; set; }
        public List<LogCursor> Cursors { get; set; }
        public List<QaItem> Results { get; set; }
        public List<string> Logs { get; set; }
        public QaSession() { Cursors=new List<LogCursor>(); Results=new List<QaItem>(); Logs=new List<string>(); Exit="待核验"; }
    }
    internal static class QaEvidence {
        internal static readonly string[] Cases = { "Q01 入口和开关", "Q02 悬停和数值", "Q03 滑条、重叠与状态", "Q04 错配与失败恢复", "Q05 原版共存" };
        internal static readonly string[] How = {
            "以 1444 年足利进战役。点入口，关掉后重开。换未发现领袖首都的国家，观察入口是否隐藏。若有两个实例，打开选择器，点取消后原窗应保留。读档后再检查一次。",
            "停留在领袖盾徽、资源余额、月变和投票分上。核对国家、资源来源、支持对象和动态文字。分别在游戏实际 100% 和 130% 缩放检查并记录。",
            "在职位和成员框滚动，拖滑块到底。空缺役位的 REB 盾徽只显示提示。点实际盾徽跳外交，主窗保留。打开子窗和游戏消息，检查层级、Esc 和关闭按钮。",
            "记录旧版缺失控件或打开失败时最后一次成功的动作。导入本轮日志或崩溃报告，记下版本、复现步骤和鼠标位置。",
            "在同一战役切换原版神罗或中华帝国与自定义面板，检查各自状态及是否出现异常退出。"
        };
        private static IEnumerable<string> LogPaths(RootMap roots) {
            if (Directory.Exists(roots.User)) {
                foreach (var file in Files.Walk(roots.User,"logs",false).Where(p => Regex.IsMatch(p, @"/(error[^/]*|system[^/]*|game[^/]*|[^/]*anvil[^/]*)\.log$",RegexOptions.IgnoreCase)))
                    yield return Files.At(roots.User,file);
            }
            if (Directory.Exists(roots.Game)) {
                foreach (var file in Files.Walk(roots.Game,"anvil",false).Where(p=>p.EndsWith(".log",StringComparison.OrdinalIgnoreCase))) yield return Files.At(roots.Game,file);
            }
        }
        internal static QaSession Start(Project p, PreviewSettings settings, string mods) {
            if (p.Data.ExportFolder==null || p.Data.ExportFingerprint==null) throw new InvalidOperationException(L.T("请先导出一份测试包。"));
            string manifest=Path.Combine(p.Data.ExportFolder,"export-manifest.json");
            byte[] bytes=Files.Read(manifest,6*1024*1024);
            if (Files.Hash(bytes)!=p.Data.ExportFingerprint) throw new IOException(L.T("清单已与项目记录不同。请重做导出。"));
            var e=Files.Json().Deserialize<ExportRecord>(TextFile.Decode(bytes).Text);
            var q=new QaSession { Fingerprint=p.Data.ExportFingerprint, ExportFolder=p.Data.ExportFolder, Revision=e.Revision,
                GameVersion=e.Eu4Version, RuntimeVersion=e.Runtime, Country=e.Country, Instance=e.Instance, EnabledMods=mods,
                Roots=p.Data.Roots.Copy(), Resolution=settings.Resolution.Width+"×"+settings.Resolution.Height, Scale=settings.Scale.ToString("P0"), StartUtc=DateTime.UtcNow.ToString("o") };
            foreach (var path in LogPaths(q.Roots).Distinct(Files.Windows)) q.Cursors.Add(Cursor(path));
            foreach (string item in Cases) q.Results.Add(new QaItem { Id=item.Substring(0,3), Outcome="待实测", Note="", Evidence="" });
            p.Data.Qa.Sessions.Add(q); p.Dirty=true;
            return q;
        }
        private static LogCursor Cursor(string path) {
            Files.NoLinks(path);
            var f=new FileInfo(path);
            using (var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)) {
                int count=(int)Math.Min(256,stream.Length); var prefix=new byte[count]; stream.Read(prefix,0,count);
                return new LogCursor { Path=path, CreatedUtc=f.CreationTimeUtc.ToString("o"), Offset=stream.Length, HeadHash=Files.Hash(prefix), HeadLength=count };
            }
        }
        internal static string Collect(QaSession q) {
            var start=DateTime.Parse(q.StartUtc,CultureInfo.InvariantCulture,DateTimeStyles.RoundtripKind).ToUniversalTime();
            var output=new StringBuilder();
            foreach (var path in LogPaths(q.Roots).Distinct(Files.Windows)) {
                var info=new FileInfo(path);
                if (info.LastWriteTimeUtc<start) continue;
                var previous=q.Cursors.FirstOrDefault(x=>Files.Windows.Equals(x.Path,path));
                var now=Cursor(path);
                using (var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)) {
                    long begin=0;
                    if (previous!=null && previous.CreatedUtc==now.CreatedUtc && previous.Offset<=now.Offset && previous.HeadLength<=256) {
                        var oldHead=new byte[Math.Max(0,previous.HeadLength)];
                        if (stream.Read(oldHead,0,oldHead.Length)==oldHead.Length && Files.Hash(oldHead)==previous.HeadHash) begin=previous.Offset;
                    }
                    if (now.Offset<=begin) continue;
                    int length=(int)Math.Min(now.Offset-begin,256*1024);
                    long startOffset=now.Offset-length;
                    stream.Position=startOffset;
                    var buffer=new byte[length]; int size=0, count;
                    while (size<length && (count=stream.Read(buffer,size,length-size))>0) size+=count;
                    output.AppendLine(path+"  "+startOffset+"–"+(startOffset+size)+(begin==0?L.T("  新建／轮换文件"):L.T("  本轮追加")));
                    output.AppendLine(Encoding.UTF8.GetString(buffer,0,size));
                    if (previous!=null) q.Cursors.Remove(previous); q.Cursors.Add(now);
                }
            }
            if (Directory.Exists(q.Roots.User)) {
                var crash=Files.At(q.Roots.User,"crashes");
                if (Directory.Exists(crash)) foreach (var dir in Directory.GetDirectories(crash).Where(dir=>Directory.GetLastWriteTimeUtc(dir)>=start).OrderByDescending(Directory.GetLastWriteTimeUtc).Take(8)) {
                    Files.NoLinks(dir); output.AppendLine(L.T("新崩溃报告目录 ") + dir);
                    foreach (var entry in Directory.GetFiles(dir).Where(f=>Path.GetFileName(f).Equals("exception.txt",StringComparison.OrdinalIgnoreCase))) {
                        try { output.AppendLine(TextFile.Decode(Files.Read(entry,65536)).Text); } catch (IOException) { }
                    }
                }
            }
            return output.Length==0?L.T("当前时间窗未找到新增日志。\n"):L.T("读取于 ") + DateTime.Now.ToString("g") + "\n"+output;
        }
        internal static string Trace(string text) {
            string[] checks={@"(type|instance)[^\r\n]*(load|register|visible|loaded)|ps_.*instance",
                @"entry[^\r\n]*(click|pressed)|hud[^\r\n]*click",
                @"(panel|window)[^\r\n]*(created|opened|opening|create fail|missing)",
                @"(bound|bind|control|tooltip)[^\r\n]*(missing|failed|ok|assigned)|bind[^\r\n]*success",
                @"(hover|hit|draw|display)[^\r\n]*(visible|success|detected|missing)"};
            string[] stages={L.T("实例与可见性"),L.T("入口接收"),L.T("原生窗口创建"),L.T("控件绑定"),L.T("悬停与绘制")};
            var lines=Document.ViewText(text??"").Split('\n');
            return String.Join("\r\n\r\n",checks.Select((check,i)=>{
                var found=lines.LastOrDefault(line=>Regex.IsMatch(line,check,RegexOptions.IgnoreCase));
                return stages[i]+"\r\n"+(found==null?L.T("本轮日志未覆盖；留待实际观察。"):found.Trim());
            }));
        }
    }
}
