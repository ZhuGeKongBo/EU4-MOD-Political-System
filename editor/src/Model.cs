using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace PoliticalEditor {
    internal sealed class Source {
        internal readonly Document Doc;
        internal readonly Node Node;
        internal string Path { get { return Doc.File; } }
        internal Source(Document doc, Node node) { Doc = doc; Node = node; }
        internal Problem At(string message, bool error = true) { return new Problem(Path, Node.Start, message, error); }
        public override string ToString() { return Node.Key + "  ·  " + Path; }
    }

    internal sealed class Field {
        internal Source From;
        internal readonly List<Field> Children = new List<Field>();
        internal string Key { get { return From.Node.Key; } }
        internal string Value { get { return From.Node.Value; } }
        internal bool Block { get { return From.Node.Block; } }
        internal Field(Source source) { From = source; }
        internal Field Get(params string[] path) {
            Field result = this;
            foreach (var key in path) { result = result.Children.LastOrDefault(n => n.Key == key); if (result == null) break; }
            return result;
        }
        internal string Read(string fallback, params string[] path) { var result = Get(path); return result == null || result.Value == null ? fallback : result.Value; }
        private static readonly string[] ReplaceBlocks = { "effect", "on_apply", "on_remove", "on_expire", "on_pass", "on_gain", "on_loss",
            "requires", "potential", "allow", "trigger", "visible", "enabled", "permissions", "show_trigger", "allow_trigger", "monthly_growth" };
        internal void Overlay(Source source) {
            foreach (var node in source.Node.Nodes) {
                if (node.Key == "copy_from" || node.Key == "default") continue;
                var inner = new Source(source.Doc, node);
                var present = Children.FindLastIndex(f => f.Key == node.Key);
                if (node.Value == "clear" || node.Value == "no" && present >= 0 && Children[present].Block && node.Operator != null) {
                    if (present >= 0) Children.RemoveAt(present);
                    continue;
                }
                var existing = present < 0 ? null : Children[present];
                if (node.Block && existing != null && existing.Block && !node.Key.EndsWith("_trigger", StringComparison.Ordinal) &&
                    !ReplaceBlocks.Contains(node.Key) && node.Nodes.Any(n => n.Operator != null)) {
                    existing.From = inner;
                    existing.Overlay(inner);
                } else {
                    var field = new Field(inner);
                    if (node.Block) field.Children.AddRange(node.Nodes.Select(n => Unchanged(new Source(source.Doc, n))));
                    if (present >= 0) Children[present] = field; else Children.Add(field);
                }
            }
        }
        private static Field Unchanged(Source s) {
            var field = new Field(s);
            field.Children.AddRange(s.Node.Nodes.Select(n => Unchanged(new Source(s.Doc, n))));
            return field;
        }
        internal string Render(int depth) {
            var n = From.Node;
            if (!Block || (Children.Count == n.Nodes.Count() && Children.Select(x => x.From.Node).SequenceEqual(n.Nodes) &&
                Children.All(x => x.SameOrigin()))) return From.Doc.At(n);
            string tab = new string(' ', depth * 4);
            return Key + " = {\n" + String.Join("\n", Children.Select(f => tab + "    " + f.Render(depth + 1))) + "\n" + tab + "}";
        }
        private bool SameOrigin() { return Children.Select(x => x.From.Node).SequenceEqual(From.Node.Nodes) && Children.All(x => x.SameOrigin()); }
    }

    internal sealed class LocText {
        internal string Text, Path;
        internal int Offset;
    }
    internal sealed class Localisation {
        private readonly Dictionary<string, LocText> values = new Dictionary<string, LocText>(StringComparer.Ordinal);
        private static readonly Regex Line = new Regex("^\\s*([^\\s:#]+):(?:\\s*\\d+)?\\s+\"((?:\\\\.|[^\"\\\\])*)\"\\s*(?:#.*)?$");
        private static readonly Regex Parameter = new Regex(@"\$([A-Za-z_][A-Za-z0-9_]*)(?:\|([^$\r\n]*))?\$");
        internal static readonly string[] FrameworkHeaders = { "l_english", "l_french", "l_german", "l_spanish" };
        internal Localisation(Mount mount, string language, List<Problem> problems, CancellationToken token, bool stock = false) {
            var paths = mount.Paths.Where(p => p.StartsWith("localisation/") && p.EndsWith(".yml", StringComparison.OrdinalIgnoreCase));
            if (stock && Directory.Exists(mount.Roots.Game)) {
                string countryNames="localisation/countries_"+language+".yml";
                if(mount.StockVisible(countryNames))paths=new[]{countryNames}.Concat(paths);
            }
            foreach (var path in paths.Distinct(Files.Windows).OrderBy(p => p.IndexOf("/replace/", StringComparison.OrdinalIgnoreCase) >= 0 ? 1 : 0)
                .ThenBy(p => mount.Dependencies.ContainsKey(p) && !mount.Own.ContainsKey(p) ? 0 : mount.Own.ContainsKey(p) ? 1 : -1).ThenBy(p => p, Files.Windows)) {
                token.ThrowIfCancellationRequested();
                var entry = mount.Get(path, stock);
                if (entry == null) continue;
                string raw = TextFile.Decode(entry.Data).Text;
                bool active = false, gotHeader = false, replacing = path.IndexOf("/replace/", StringComparison.OrdinalIgnoreCase) >= 0;
                using (var reader = new StringReader(raw)) {
                    int offset = 0;
                    for (string line; (line = reader.ReadLine()) != null;) {
                        string trim = line.Trim();
                        if (Regex.IsMatch(trim, @"^l_[a-z_]+:$")) { active = trim == language + ":"; gotHeader = true; }
                        else if (active && trim.Length > 0 && !trim.StartsWith("#")) {
                            var match = Line.Match(line);
                            if (!match.Success) problems.Add(new Problem(path, offset, L.T("本地化应写作 键:0 \"文本\"。")));
                            else {
                                string key = match.Groups[1].Value;
                                if (values.ContainsKey(key) && !replacing) problems.Add(new Problem(path, offset, L.T("本地化键 ") + key + L.T(" 亦在 ") + values[key].Path + L.T(" 定义。"), false));
                                values[key] = new LocText { Path = path, Offset = offset, Text = match.Groups[2].Value.Replace("\\n", "\n").Replace("\\\"", "\"").Replace("\\\\", "\\") };
                            }
                        }
                        offset += line.Length;
                        if (offset < raw.Length && raw[offset] == '\r') ++offset;
                        if (offset < raw.Length && raw[offset] == '\n') ++offset;
                    }
                }
                if (!gotHeader && mount.Own.ContainsKey(path)) problems.Add(new Problem(path, 0, L.T("本地化文件缺少语言头。")));
            }
        }
        internal LocText Find(string key) { LocText value; return key != null && values.TryGetValue(key, out value) ? value : null; }
        internal string Caption(string key) { var text = Find(key); return Pretty(text == null ? key ?? "" : text.Text); }
        internal string Format(string key, IDictionary<string, object> data) {
            return Parameter.Replace(Caption(key), match => {
                object item; if (!data.TryGetValue(match.Groups[1].Value, out item) || item == null) return match.Value;
                if (!(item is decimal) && !(item is int)) return Convert.ToString(item, CultureInfo.InvariantCulture);
                string format = match.Groups[2].Value;
                var number = Convert.ToDecimal(item, CultureInfo.InvariantCulture);
                bool percent = format.Contains("%");
                var digits = Regex.Match(format, @"[0-6]");
                int precision = digits.Success ? Int32.Parse(digits.Value, CultureInfo.InvariantCulture) : 2;
                string result = (percent ? number * 100 : number).ToString("F" + precision, CultureInfo.InvariantCulture);
                return (number >= 0 && format.Contains("+") ? "+" : "") + result + (percent ? "%" : "");
            });
        }
        private static string Pretty(string text) { return Regex.Replace(text, "§[A-Za-z!]", ""); }
    }

    internal sealed class Snapshot {
        internal string Id, Leader = "", Date = "", Name, Type, MemberRule;
        internal Source From;
        internal readonly List<string> Members = new List<string>();
        internal readonly Dictionary<string, List<string>> Roles = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        internal readonly Dictionary<string, HashSet<string>> RoleExclusions = new Dictionary<string, HashSet<string>>(StringComparer.Ordinal);
        internal readonly HashSet<int> Territory = new HashSet<int>();
        internal readonly Dictionary<string, decimal> Balances = new Dictionary<string, decimal>(StringComparer.Ordinal);
        internal bool SampleMembers, Future;
    }

    internal sealed class Model {
        internal const string TypeDir = "common/political_systems/types/";
        internal const string LayoutDir = "common/political_system_layouts/";
        internal const string HistoryDir = "history/political_systems/";
        internal readonly Mount Mount;
        internal readonly Dictionary<string, Document> Docs = new Dictionary<string, Document>(Files.Windows);
        internal readonly Dictionary<string, Source> Types = new Dictionary<string, Source>(StringComparer.Ordinal);
        internal readonly Dictionary<string, Source> Layouts = new Dictionary<string, Source>(StringComparer.Ordinal);
        internal readonly Dictionary<string, Source> Huds = new Dictionary<string, Source>(StringComparer.Ordinal);
        internal readonly List<Problem> Problems = new List<Problem>();
        internal readonly List<Source> Chain = new List<Source>();
        internal readonly List<Snapshot> Instances = new List<Snapshot>();
        internal readonly Dictionary<string, Field> Effective = new Dictionary<string, Field>(StringComparer.Ordinal);
        private readonly Dictionary<string, Document> countrySources = new Dictionary<string, Document>(StringComparer.Ordinal);
        private Dictionary<string, string> countryFiles;
        internal Source Type, Layout, Window, Hud;
        internal Field Rule;
        internal Snapshot Instance;
        internal Localisation Loc;
        internal Sprites Images;
        internal string GuiPath = "", HudGuiPath = "", GfxPath = "", EntrySprite = "", Background = "", Title = "", PrimaryId = "";
        internal string Country, Date, Language, Version;
        internal decimal Amount, Growth;
        internal int Precision = 2;
        internal Field Primary, ResourceGroup;

        internal Model(Mount mount, string wantedType, string wantedInstance, string country, string date, string language,
                       CancellationToken cancel, Sprites sprites = null, bool useStockGfx = true) {
            Mount = mount; Problems.AddRange(mount.Problems);
            Country = country; Date = date; Language = language;
            Version = Fingerprints.Framework(mount.Roots.Game);
            foreach (var path in mount.Paths) {
                cancel.ThrowIfCancellationRequested();
                if (path.StartsWith(TypeDir) && path.EndsWith(".txt")) Index(Doc(path), Types, n => true, null);
                if (path.StartsWith(LayoutDir) && path.EndsWith(".txt")) {
                    var doc = Doc(path);
                    Index(doc, Layouts, n => n.Key != "hud_regions", null);
                    Index(doc, Huds, n => true, doc.Root.Child("hud_regions"), false);
                }
            }
            string id = wantedType;
            if (String.IsNullOrEmpty(id) || !Types.ContainsKey(id)) id = Types.Keys.FirstOrDefault(k => k!="default"&&Types[k].Node.Get("abstract") != "yes") ?? Types.Keys.FirstOrDefault();
            Loc = new Localisation(mount, language, Problems, cancel, true);
            if (id == null) { Problems.Add(new Problem("", 0, L.T("尚无政治体系定义。"))); return; }
            Type = Types[id];
            Rule = Resolve(id, new List<string>(), true);
            if (Rule == null) return;
            Locate();
            foreach (var path in mount.Paths.Where(p => (p.StartsWith(HistoryDir) || p.StartsWith("common/political_systems/instances/") || p == "common/political_systems/instances.txt") && p.EndsWith(".txt"))) {
                var doc = Doc(path);
                foreach (var item in doc.Root.Nodes.Where(n => n.Block && n.Get("type") == id)) {
                    if (Instances.Any(i => i.Id == item.Key)) { Problems.Add(new Problem(path, item.Start, L.T("实例 ID 重复 ") + item.Key)); continue; }
                    Instances.Add(ReadHistory(new Source(doc, item)));
                }
            }
            Instance = Instances.FirstOrDefault(i => i.Id == wantedInstance) ?? Instances.FirstOrDefault();
            if (Instance == null && Type.Node.Get("abstract") != "yes") Problems.Add(Type.At(L.T("此体系尚无历史实例。"), false));
            Title = Loc.Caption(Instance != null && !String.IsNullOrWhiteSpace(Instance.Name) ? Instance.Name : Rule.Read(Rule.Read(id, "title"), "interface", "title_key"));
            SelectResource();
            Images = sprites ?? new Sprites(mount, cancel, useStockGfx);
            Problems.AddRange(Images.Problems);
            CheckBindings();
        }
        internal Document Doc(string path) {
            if (String.IsNullOrWhiteSpace(path)) return null;
            path = Files.Relative(path);
            Document found; if (Docs.TryGetValue(path, out found)) return found;
            var entry = Mount.Get(path, true);
            if (entry == null) return null;
            var doc = new Document(path, TextFile.Decode(entry.Data).Text);
            Docs[path] = doc; Problems.AddRange(doc.Problems); return doc;
        }
        private void Index(Document doc, Dictionary<string, Source> index, Func<Node, bool> include, Node root, bool useRoot = true) {
            if (doc == null || root == null && !useRoot) return;
            foreach (var node in (root ?? doc.Root).Nodes.Where(n => n.Block && include(n))) {
                Source other;
                if (index.TryGetValue(node.Key, out other)) Problems.Add(new Problem(doc.File, node.Start, L.T("定义重复 ") + node.Key + L.T("，另一处在 ") + other.Path));
                else index.Add(node.Key, new Source(doc, node));
            }
        }
        internal Field Resolve(string id, List<string> visited, bool record = false) {
            if (!record && Effective.ContainsKey(id)) return Effective[id];
            Source source;
            if (!Types.TryGetValue(id, out source)) { Problems.Add(new Problem("", 0, L.T("找不到继承类型 ") + id)); return null; }
            if (visited.Contains(id) || visited.Count >= 32) { Problems.Add(source.At(L.T("继承循环或超过支持深度 ") + String.Join(" → ", visited.Concat(new[] { id })))); return null; }
            var field = new Field(source);
            string parent = source.Node.Get("copy_from",id!="default"&&Types.ContainsKey("default")?"default":"");
            if (record) Chain.Add(source);
            if (!String.IsNullOrWhiteSpace(parent)) {
                var inherited = Resolve(parent, visited.Concat(new[] { id }).ToList(), record);
                if (inherited == null) return null;
                field.Children.AddRange(inherited.Children.Where(f => f.Key != "abstract").Select(CloneField));
            }
            field.Overlay(source);
            Effective[id] = field;
            return field;
        }
        private static Field CloneField(Field item) {
            var field = new Field(item.From);
            field.Children.AddRange(item.Children.Select(CloneField));
            return field;
        }
        private void Locate() {
            string layout = Rule.Read("", "interface", "layout");
            Background = Rule.Read("", "interface", "background");
            if (!Layouts.TryGetValue(layout, out Layout)) {
                Problems.Add(Type.At(layout.Length == 0 ? L.T("尚未指定 interface.layout。") : L.T("找不到布局 ") + layout));
            } else {
                GuiPath = Layout.Node.Get("gui_file");
                var doc = Doc(GuiPath); var root = doc == null ? null : doc.Root.Child("guiTypes");
                var window = root == null ? null : root.Named(Layout.Node.Get("window"));
                if (window == null) Problems.Add(Layout.At(L.T("找不到原生窗口 ") + Layout.Node.Get("window") + "。"));
                else Window = new Source(doc, window);
            }
            string hud = Rule.Read("", "interface", "entry_button", "region");
            EntrySprite = Rule.Read("", "interface", "entry_button", "icon");
            if (hud.Length!=0 && !Huds.TryGetValue(hud, out Hud)) Problems.Add(Type.At(L.T("找不到入口区域 ") + hud + "。"));
            if (Hud != null) {
                HudGuiPath = Hud.Node.Get("gui_file"); GfxPath = Hud.Node.Get("gfx_file");
            } else GfxPath = Mount.Paths.FirstOrDefault(p => p.StartsWith("interface/") && p.EndsWith(".gfx")) ?? "";
        }
        internal static int DateKey(string date) {
            date = date ?? "";
            var parts = date.Split('.'); int year, month, day;
            if (parts.Length != 3 || !Int32.TryParse(parts[0], out year) || year < 1 || year > 9999 || !Int32.TryParse(parts[1], out month) || month < 1 || month > 12 ||
                !Int32.TryParse(parts[2], out day) || day < 1 || day > DateTime.DaysInMonth(year, month)) return -1;
            return year * 10000 + month * 100 + day;
        }
        private Snapshot ReadHistory(Source source) {
            var n = source.Node;
            var instance = new Snapshot { Id = n.Key, Type = n.Get("type"), Name = n.Get("name"), From = source };
            string start = n.Get("founded",n.Get("date")); var dates = n.Nodes.Where(item => item.Block && DateKey(item.Key) > 0).OrderBy(item => DateKey(item.Key)).ToArray();
            instance.Date = start.Length == 0 ? (dates.Length == 0 ? "1444.1.1" : dates[0].Key) : start;
            instance.Future = DateKey(instance.Date) > DateKey(Date);
            if (instance.Future) return instance;
            ApplyHistory(instance, n);
            foreach (var dated in dates.Where(d => DateKey(d.Key) <= DateKey(Date))) ApplyHistory(instance, dated);
            string rule = Rule.Read("explicit", "members", "source");
            instance.MemberRule = rule;
            if (rule == "leader_subjects") {
                string subject = Rule.Read("", "members", "subject_type");
                instance.MemberRule += " · " + subject;
                var found = Subjects(instance.Leader, subject);
                instance.Members.Clear(); instance.Members.AddRange(found);
                if (found.Count == 0 && subject == "daimyo_vassal" && instance.Leader == "ASK") {
                    instance.SampleMembers = true;
                    instance.Members.AddRange("AMA DTE HSK HTK IMG ODA OTM OUC SBA SMZ TKD TKG UES YMN RFR KTB AKT CBA ISK ITO KKC KNO OGS SHN ASA ANE HJO HSM YMG".Split(' '));
                } else if (found.Count == 0) instance.SampleMembers = true;
            } else if (rule=="capital_territory") {
                var selected=new List<string>();
                string flag=Rule.Read("","territory","history_key");
                var provinces=ProvinceHistory();
                foreach(var country in CountryPaths()) {
                    var facts=CountryFacts(country.Key,Date);
                    int capital=facts==null?0:Document.Int(facts.Get("capital"));
                    string province;
                    if(capital!=0&&(instance.Territory.Contains(capital)||flag!=""&&provinces.TryGetValue(capital,out province)&&ProvinceHasFlag(province,flag)))
                        selected.Add(country.Key);
                }
                instance.Members.Clear();instance.Members.AddRange(selected.OrderBy(x=>x,StringComparer.Ordinal));
            } else if (rule=="query"||rule=="mixed") {
                var query=Rule.Get("members","source_trigger");
                if(query!=null&&query.Children.All(f=>f.Key=="religion"||f.Key=="primary_culture"||f.Key=="tag"||f.Key=="always")) {
                    if(rule=="query")instance.Members.Clear();
                    foreach(var pair in CountryPaths()) {
                        var facts=CountryFacts(pair.Key,Date);
                        if(facts!=null&&query.Children.All(f=>f.Key=="tag"?pair.Key==f.Value:f.Key=="always"?f.Value=="yes":facts.Get(f.Key)==f.Value)
                            &&!instance.Members.Contains(pair.Key))instance.Members.Add(pair.Key);
                    }
                }
                instance.SampleMembers=true;
            } else if (rule!="explicit"&&rule!="history")instance.SampleMembers=true;
            var configured=Rule.Get("special_members");
            if(configured!=null)foreach(var role in configured.Children.Where(f=>f.Block&&f.Read("","history_key")!="")) {
                string key=role.Read("","history_key");
                List<string> holders; HashSet<string> disabled;
                if(!instance.Roles.TryGetValue(role.Key,out holders))instance.Roles[role.Key]=holders=new List<string>();
                instance.RoleExclusions.TryGetValue(role.Key,out disabled);
                var eligible=role.Read("yes","require_member")=="yes"?instance.Members.ToArray():CountryPaths().Select(p=>p.Key).ToArray();
                foreach(var country in eligible.Where(c=>!holders.Contains(c)&&(disabled==null||!disabled.Contains(c)))) {
                    var facts=CountryFacts(country,Date);
                    if(facts!=null&&facts.Get(key)=="yes")holders.Add(country);
                }
            }
            return instance;
        }
        private List<string> Subjects(string leader, string subject) {
            var result = new HashSet<string>(StringComparer.Ordinal);
            if (!Files.IsId(leader) || leader == "" || subject == "") return result.ToList();
            IEnumerable<string> stock = Directory.Exists(Mount.Roots.Game) ? Files.Walk(Mount.Roots.Game, "history/diplomacy", false).Where(Mount.StockVisible) : new string[0];
            int date = DateKey(Date);
            foreach (var file in stock.Concat(Mount.Paths.Where(p => p.StartsWith("history/diplomacy/"))).Where(p => p.EndsWith(".txt")).Distinct(Files.Windows)) {
                var entry = Mount.Get(file, true); if (entry == null) continue;
                string text = TextFile.Decode(entry.Data).Text;
                string key = subject == "daimyo_vassal" ? "daimyo" : subject;
                if (text.IndexOf(leader,StringComparison.Ordinal)<0 || subject!="daimyo_vassal"&&text.IndexOf(key,StringComparison.OrdinalIgnoreCase)<0) continue;
                var parsed = new Document(file, text);
                foreach (var relation in parsed.Root.Nodes.Where(n => n.Block && (n.Key == subject || n.Key == key || subject=="daimyo_vassal"&&n.Key=="vassal"))) {
                    string from = relation.Get("first"), to = relation.Get("second");
                    if (from != leader || to.Length != 3 || DateKey(relation.Get("start_date")) > date ||
                        DateKey(relation.Get("end_date")) >= 0 && date >= DateKey(relation.Get("end_date"))) continue;
                    if(subject=="daimyo_vassal"&&relation.Key=="vassal") {
                        var facts=CountryFacts(to,Date);
                        if(facts==null||!facts.Reforms.Contains("daimyo"))continue;
                    }
                    result.Add(to);
                }
            }
            return result.OrderBy(tag => tag, StringComparer.Ordinal).ToList();
        }
        private static void ApplyHistory(Snapshot instance, Node n) {
            var leader = n.Child("leader");
            if (leader != null) instance.Leader = leader.Value == "none" || leader.Value == "clear" ? "" : leader.Value;
            var members = n.Child("members");
            if (members != null) {
                instance.Members.Clear();
                foreach (var member in members.Nodes) SetMember(instance.Members, member);
            }
            var removed=n.Child("remove_members");if(removed!=null)foreach(var tag in removed.Nodes)instance.Members.Remove(tag.Key);
            var added=n.Child("add_members");if(added!=null)foreach(var tag in added.Nodes)SetMember(instance.Members,tag);
            var land=n.Child("territory");if(land!=null){instance.Territory.Clear();foreach(var province in land.Nodes)if(Document.Int(province.Key)>0)instance.Territory.Add(Document.Int(province.Key));}
            var lost=n.Child("remove_provinces");if(lost!=null)foreach(var province in lost.Nodes)instance.Territory.Remove(Document.Int(province.Key));
            var gained=n.Child("add_provinces");if(gained!=null)foreach(var province in gained.Nodes)if(Document.Int(province.Key)>0)instance.Territory.Add(Document.Int(province.Key));
            var balances=n.Child("resources");
            if(balances!=null)foreach(var balance in balances.Nodes) {
                decimal value;
                if(Decimal.TryParse(balance.Value,NumberStyles.Number,CultureInfo.InvariantCulture,out value))instance.Balances[balance.Key]=value;
            }
            var roles = n.Child("roles");
            if (roles != null) {
                if (roles.Value == "clear") instance.Roles.Clear();
                foreach (var role in roles.Nodes) {
                    List<string> list;
                    if (!instance.Roles.TryGetValue(role.Key, out list)) instance.Roles[role.Key] = list = new List<string>();
                    HashSet<string> excluded;
                    if(!instance.RoleExclusions.TryGetValue(role.Key,out excluded))
                        instance.RoleExclusions[role.Key]=excluded=new HashSet<string>(StringComparer.Ordinal);
                    if (role.Value == "clear" || role.Nodes.Any(node => node.Operator == null)) list.Clear();
                    foreach (var tag in role.Nodes) {
                        SetMember(list, tag);
                        if(tag.Value=="no")excluded.Add(tag.Key);else excluded.Remove(tag.Key);
                    }
                }
            }
        }
        private static void SetMember(List<string> list, Node member) {
            if (member.Key.Length != 3) return;
            if (member.Value == "no") list.Remove(member.Key);
            else if (!list.Contains(member.Key)) list.Add(member.Key);
        }
        private void SelectResource() {
            ResourceGroup = Rule.Get("resources");
            var active = ResourceGroup == null ? new Field[0] : ResourceGroup.Children.Where(f => f.Block).ToArray();
            Primary = active.FirstOrDefault(f => f.Read("instance", "owner") != "country") ?? active.FirstOrDefault() ?? Rule.Get("authority");
            if (Primary != null) {
                PrimaryId = Primary.Key;
                Decimal.TryParse(Primary.Read("0", "initial"), NumberStyles.Number, CultureInfo.InvariantCulture, out Amount);
                if(Instance!=null&&Instance.Balances.ContainsKey(PrimaryId))Amount=Instance.Balances[PrimaryId];
                Growth=Scene.Number(Primary.Get("monthly_growth"),"value");
                Precision = Math.Min(6, Math.Max(0, Document.Int(Primary.Read("2", "precision"), 2)));
            }
        }
        internal sealed class CountryInfo {
            internal readonly Dictionary<string,string> Fields=new Dictionary<string,string>(StringComparer.Ordinal);
            internal readonly HashSet<string> Reforms=new HashSet<string>(StringComparer.Ordinal);
            internal string Name="", Dynasty="", Throne="";
            internal string Get(string key) { string value;return Fields.TryGetValue(key,out value)?value:""; }
        }
        private IEnumerable<KeyValuePair<string,string>> CountryPaths() {
            if(countryFiles==null) {
                countryFiles=new Dictionary<string,string>(StringComparer.Ordinal);
                IEnumerable<string> stock=Directory.Exists(Mount.Roots.Game)?Files.Walk(Mount.Roots.Game,"history/countries",false).Where(Mount.StockVisible):new string[0];
                foreach(var path in stock.Concat(Mount.Paths.Where(p=>p.StartsWith("history/countries/"))).Where(p=>p.EndsWith(".txt")).Distinct(Files.Windows)) {
                    string name=System.IO.Path.GetFileName(path);
                    if(name.Length>7&&name[3]==' '&&Files.IsId(name.Substring(0,3)))countryFiles[name.Substring(0,3)]=path;
                }
            }
            return countryFiles;
        }
        internal CountryInfo CountryFacts(string country,string date) {
            CountryPaths();
            string path;
            if(country==null||!countryFiles.TryGetValue(country,out path))return null;
            Document doc;
            if(!countrySources.TryGetValue(country,out doc)) {
                var entry=Mount.Get(path,true);
                if(entry==null)return null;
                doc=new Document(path,TextFile.Decode(entry.Data).Text);countrySources[country]=doc;
            }
            var result=new CountryInfo();
            HistoryFact(result,doc.Root,"");
            foreach(var n in doc.Root.Nodes.Where(x=>x.Block&&DateKey(x.Key)>0&&DateKey(x.Key)<=DateKey(date)).OrderBy(x=>DateKey(x.Key)))HistoryFact(result,n,n.Key);
            return result;
        }
        private static void HistoryFact(CountryInfo facts,Node block,string date) {
            foreach(var n in block.Nodes.Where(n=>n.Operator!=null)) {
                if(n.Key=="add_government_reform"&&n.Value!=null)facts.Reforms.Add(n.Value);
                else if(n.Key=="remove_government_reform"&&n.Value!=null)facts.Reforms.Remove(n.Value);
                else if(n.Key=="monarch"&&n.Block) { facts.Name=n.Get("name");facts.Dynasty=n.Get("dynasty");facts.Throne=date; }
                else if(n.Value!=null)facts.Fields[n.Key]=n.Value;
            }
        }
        private Dictionary<int,string> ProvinceHistory() {
            var result=new Dictionary<int,string>();
            IEnumerable<string> stock=Directory.Exists(Mount.Roots.Game)?Files.Walk(Mount.Roots.Game,"history/provinces",false).Where(Mount.StockVisible):new string[0];
            foreach(var p in stock.Concat(Mount.Paths.Where(p=>p.StartsWith("history/provinces/"))).Where(p=>p.EndsWith(".txt")).Distinct(Files.Windows)) {
                var match=Regex.Match(System.IO.Path.GetFileName(p),@"^(\d+)[ -]");
                if(match.Success)result[Document.Int(match.Groups[1].Value)]=p;
            }
            return result;
        }
        private bool ProvinceHasFlag(string path,string key) {
            var entry=Mount.Get(path,true);
            if(entry==null)return false;
            string text=TextFile.Decode(entry.Data).Text;
            if(text.IndexOf(key,StringComparison.Ordinal)<0)return false;
            var document=new Document(path,text);
            string value=document.Root.Get(key);
            foreach(var date in document.Root.Nodes.Where(n=>DateKey(n.Key)>0&&DateKey(n.Key)<=DateKey(Date)).OrderBy(n=>DateKey(n.Key)))value=date.Get(key,value);
            return value=="yes";
        }
        internal string TabPath(int tab) {
            switch (tab) {
                case 0: return Type == null ? "" : Type.Path;
                case 1: return Layout == null ? "" : Layout.Path;
                case 2: return GuiPath;
                case 3: return GfxPath;
                case 4: return Instance == null ? Mount.Paths.FirstOrDefault(p => p.StartsWith(HistoryDir)) ?? "" : Instance.From.Path;
                case 5: var key = Loc.Find(Rule == null ? "" : Rule.Read("", "interface", "title_key"));
                    return key != null ? key.Path : Mount.Paths.FirstOrDefault(p => p.StartsWith("localisation/") && p.EndsWith("_" + Language + ".yml")) ?? "";
                default: return "";
            }
        }
        internal string BitmapFor(Node node) { return node.Get("spriteType", node.Get("quadTextureSprite", node.Get("sprite"))); }
        internal IEnumerable<Source> LayoutUsers(string layout) {
            return Types.Values.Where(s => {
                var resolved = Resolve(s.Node.Key, new List<string>());
                return resolved != null && resolved.Read("", "interface", "layout") == layout;
            });
        }
        private void CheckBindings() {
            if (Window != null) {
                ControlBindings.Check(this);
                var regions = Layout.Node.Child("regions");
                if (Rule.Read("appointed", "leader","succession","mode") == "elected" ) {
                    var special = regions == null ? null : regions.Child("special_members_region");
                    if (special == null || special.Child("support_shield") == null || special.Child("nomination_score") == null)
                        Problems.Add(Layout.At(L.T("选举布局未绑定小盾徽或评分。请将面板与本机运行时配套更新。"), false));
                }
                var essential=new[]{Layout.Node.Get("background_control"),Layout.Node.Get("close_control")}
                    .Select(n=>Window.Node.Named(n)).Where(n=>n!=null).Select(BitmapFor).Concat(new[]{Background}).Where(s=>s!="").ToArray();
                foreach (string sprite in Window.Node.Nodes.Select(BitmapFor).Concat(essential).Where(s => s.Length != 0).Distinct())
                    CheckSprite(sprite,Window,essential.Contains(sprite));
            }
            if (Hud == null) return;
            var doc = Doc(HudGuiPath); var gfx = Doc(GfxPath);
            var root = doc == null ? null : doc.Root.Child("guiTypes");
            if (root == null || gfx == null || EntrySprite == "") { Problems.Add(Hud.At(L.T("入口缺少 GUI、GFX 或图标绑定。"))); return; }
            int capacity = Document.Int(Hud.Node.Get("capacity"));
            if (capacity < 1 || capacity > 8) Problems.Add(Hud.At(L.T("入口容量范围为 1–8。")));
            for (int i = 0; i < Math.Min(8, capacity); ++i) {
                string name = Hud.Node.Get("container") + "_" + i.ToString("D2", CultureInfo.InvariantCulture);
                var slot = root.Named(name);
                if (slot == null || !slot.Nodes.Any(n => n.Key == "guiButtonType" && n.Get("name").StartsWith(Hud.Node.Get("button_template"), StringComparison.Ordinal) &&
                    n.Get("quadTextureSprite") == EntrySprite))
                    Problems.Add(new Problem(HudGuiPath, slot == null ? root.Start : slot.Start, name + L.T(" 缺少已绑定的入口图标 ") + EntrySprite));
            }
            var spriteRoot = gfx.Root.Child("spriteTypes");
            if (spriteRoot == null || spriteRoot.Named(EntrySprite) == null) Problems.Add(new Problem(GfxPath, 0, L.T("区域的 GFX 没有登记 ") + EntrySprite));
            CheckSprite(EntrySprite, Hud, true);
        }
        private void CheckSprite(string sprite, Source source, bool required) {
            if (Images == null) return;
            var candidates = Images.Find(sprite);
            if (candidates.Count == 0) Problems.Add(source.At(L.T("未解析到素材 ") + sprite, required && Directory.Exists(Mount.Roots.Game)));
            else if (candidates.Count > 1) Problems.Add(source.At(L.T("同名素材有 ") + candidates.Count + L.T(" 个定义 ") + sprite, required));
            else if (candidates[0].SourceRoot == "未找到") Problems.Add(source.At(L.T("未挂载贴图 ") + candidates[0].Texture, required && Directory.Exists(Mount.Roots.Game)));
        }
    }

    internal static class Fingerprints {
        internal static string Framework(string game) {
            if (String.IsNullOrEmpty(game) || !Directory.Exists(game)) return L.T("未定位");
            string toml = Files.At(game, "anvil/plugins/great-khan-native/plugin.toml");
            if (!File.Exists(toml)) return L.T("未找到 plugin.toml");
            var match = Regex.Match(TextFile.Decode(Files.Read(toml, 512000)).Text, "(?m)^\\s*version\\s*=\\s*\"([^\"]+)\"");
            return match.Success ? match.Groups[1].Value : "未知";
        }
        internal static string GameVersion(string game) {
            if (String.IsNullOrEmpty(game) || !File.Exists(Path.Combine(game, "eu4.exe"))) return L.T("未定位");
            string launcher = Path.Combine(game, "launcher-settings.json");
            if (File.Exists(launcher)) {
                string raw=TextFile.Decode(Files.Read(launcher,512000)).Text;
                var match = Regex.Match(raw, "\"rawVersion\"\\s*:\\s*\"([^\"]+)\"");
                if(!match.Success)match=Regex.Match(raw,"\"version\"\\s*:\\s*\"([^\"]+)\"");
                if (match.Success) return match.Groups[1].Value;
            }
            var version = System.Diagnostics.FileVersionInfo.GetVersionInfo(Path.Combine(game, "eu4.exe"));
            if (!String.IsNullOrWhiteSpace(version.ProductVersion)) return "PE " + version.ProductVersion;
            return L.T("见 eu4.exe 指纹");
        }
    }
}
