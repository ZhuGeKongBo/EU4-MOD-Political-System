using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.Linq;

namespace PoliticalEditor {
    internal static class Geometry {
        internal static Point Raw(Node node) { return Pair(node == null ? null : node.Child("position")); }
        internal static Point Pair(Node node) {
            return node == null ? Point.Empty : new Point(Limit(Document.Int(node.Get("x"))), Limit(Document.Int(node.Get("y"))));
        }
        internal static int Limit(int number) { return Math.Max(-16384, Math.Min(16384, number)); }
        internal static Size SizeOf(Node node, Size fallback) {
            if (node == null) return fallback;
            var size = Pair(node.Child("size"));
            int w = size.X == 0 ? Document.Int(node.Get("maxWidth"), fallback.Width) : size.X;
            int h = size.Y == 0 ? Document.Int(node.Get("maxHeight"), fallback.Height) : size.Y;
            return new Size(Math.Max(1, Math.Min(8192, w)), Math.Max(1, Math.Min(8192, h)));
        }
        internal static Point At(Node node, Size parent) {
            var result = Raw(node);
            string orientation = node.Get("Orientation").ToUpperInvariant();
            if (orientation == "CENTER" || orientation == "CENTRE") return Point.Add(result, new Size(parent.Width / 2, parent.Height / 2));
            if (orientation.EndsWith("_RIGHT", StringComparison.Ordinal)) result.X += parent.Width;
            if (orientation.StartsWith("LOWER_", StringComparison.Ordinal)) result.Y += parent.Height;
            if (orientation == "CENTER_UP" || orientation == "CENTER_DOWN") result.X += parent.Width / 2;
            if (orientation == "CENTER_DOWN") result.Y += parent.Height;
            return result;
        }
    }

    internal sealed class VisualItem {
        internal Node Node;
        internal int[] Key;
        internal Point Raw, DisplayOffset;
        internal Rectangle Bounds, Clip;
        internal string Name, Sprite, Text, Tooltip, Country, Role, Action, ListKey, AutoRegion, RegionFrame;
        internal bool Hidden, Hit, Click, Locked, Positioned, Transparent, Dimmed;
        internal int FontSize = 16, FrameIndex;
        public override string ToString() { return Name + (Locked ? L.T("   · 自动") : ""); }
    }

    internal sealed class ListViewState {
        internal string Key, Slider;
        internal int Columns, Rows, Total, Offset;
        internal Rectangle Bounds, Rail;
        internal int Maximum { get { return Math.Max(0, (Total + Columns - 1) / Columns - Rows); } }
    }

    internal sealed class PreviewSettings {
        internal Size Resolution = new Size(1920, 1080);
        internal decimal Scale = 1m;
        internal bool Playing, Visible = true, Enabled = true, MapDiscovered = true;
        internal string Screen = L.T("实验预览");
        internal readonly Dictionary<string, int> Offsets = new Dictionary<string, int>(StringComparer.Ordinal);
        internal readonly HashSet<string> Passed = new HashSet<string>(StringComparer.Ordinal);
    }

    internal sealed class Scene {
        internal readonly Model Model;
        internal readonly PreviewSettings Settings;
        internal readonly List<VisualItem> Items = new List<VisualItem>();
        internal readonly List<ListViewState> Lists = new List<ListViewState>();
        internal readonly List<ActionView> Actions = new List<ActionView>();
        internal Size Panel;
        internal Point OnScreen, EntryPosition;
        internal Rectangle WorldPanel;
        internal string BackgroundControl, HoverControl;
        private class OfficeSeat { internal string Role, Tag, Heading, Tip; internal bool First; }
        private readonly List<OfficeSeat> offices = new List<OfficeSeat>();
        private readonly Dictionary<string, VisualItem> byName = new Dictionary<string, VisualItem>(StringComparer.Ordinal);
        internal sealed class ActionView {
            internal string Module, Id, Name, Tip, Requirement;
            internal bool Enabled, Passed;
            internal Field Definition;
        }
        internal Scene(Model model, PreviewSettings settings) {
            Model = model; Settings = settings;
            if (model == null || model.Window == null) return;
            var window = model.Window.Node;
            Panel = Geometry.SizeOf(window, new Size(635, 720));
            var viewport = new Size((int)(settings.Resolution.Width / settings.Scale), (int)(settings.Resolution.Height / settings.Scale));
            OnScreen = Geometry.At(window, viewport);
            WorldPanel = new Rectangle(Point.Empty, Panel);
            var bounds = new Rectangle(Point.Empty, Panel);
            string close = model.Layout.Node.Get("close_control"), title = model.Layout.Node.Get("title_control");
            BackgroundControl = model.Layout.Node.Get("background_control"); HoverControl = model.Layout.Node.Get("hover_control");
            Build(window, null, Point.Empty, Panel, bounds);
            var entryDoc = model.Hud == null ? null : model.Doc(model.HudGuiPath);
            var hud = entryDoc == null || entryDoc.Root.Child("guiTypes") == null ? null : entryDoc.Root.Child("guiTypes").Named(model.Hud.Node.Get("container") + "_00");
            if (hud != null) EntryPosition = Geometry.At(hud, viewport);
            var element = ByName(BackgroundControl);
            if (element != null) {
                if (model.Background.Length != 0) element.Sprite = model.Background;
                element.Click = element.Hit = false;
            }
            Bind(close, null, L.T("关闭政治面板"), "close");
            Bind(title, model.Title, model.Title, null);
            Hide(model.Layout.Node.Get("instance_control"), model.Instances.Count < 2, L.T("选择当前类型的历史实例"), "instances");
            Hide(model.Layout.Node.Get("vote_control"), model.Rule.Read("", "leader","succession","mode") != "elected", L.T("选择候选人。预览用样例数据。"), "vote");
            if (ByName(HoverControl) != null) { ByName(HoverControl).Hit = true; ByName(HoverControl).Click = false; ByName(HoverControl).Transparent = true; }
            foreach (var item in Items.Where(i => i.Name.StartsWith("selector_"))) item.Hidden = true;
            var regions = model.Layout.Node.Child("regions");
            if (regions == null) return;
            var memberRegion = regions.Child("members_region");
            var leaderRegion = regions.Child("leader_region");
            var roleRegion = regions.Child("special_members_region");
            var resourceRegion = regions.Child("primary_resource_region");
            var registers = regions.Child("system_resources_region");
            var actions = regions.Child("actions_region");
            var members = model.Instance == null ? new List<string>() : model.Instance.Members;
            string leader = model.Instance == null || model.Instance.Leader == "" ? "REB" : model.Instance.Leader;
            if (leaderRegion != null) {
                string caption = model.Loc.Caption(model.Rule.Read("Leader", "leader","title"));
                Bind(leaderRegion.Get("caption"), caption, caption, null);
                var ruler=leader=="REB"?null:model.CountryFacts(leader,model.Date);
                string rulerName=ruler==null||ruler.Name==""?model.Loc.Caption(leader):(ruler.Name+" "+ruler.Dynasty).Trim();
                string tip = leader == "REB" ? L.T("领袖空缺") : rulerName + "\n" + model.Loc.Caption(leader) + "（" + leader + "）";
                if (ruler!=null&&ruler.Throne!="")tip+=L.T("\n统治者起始日 ")+ruler.Throne;
                if (leader != "REB") tip += L.T("\n历任领袖\n") + String.Join("\n", model.Instance.From.Node.Nodes.Where(n => n.Block && n.Child("leader") != null && Model.DateKey(n.Key)<=Model.DateKey(model.Date)).Select(n=>{
                    string tag=n.Get("leader");var former=model.CountryFacts(tag,n.Key);
                    return n.Key+"  "+model.Loc.Caption(tag)+(former==null||former.Name==""?"":"  "+former.Name+" "+former.Dynasty);
                }));
                Bind(leaderRegion.Get("label"), leader == "REB" ? "" : rulerName, tip, null);
                Bind(model.Layout.Node.Get("leader_history_control"), null, tip, null);
                Flag(leaderRegion.Get("control"), leader, tip, false);
                Auto(leaderRegion);
            }
            if (memberRegion != null) {
                string label = model.Loc.Caption(model.Rule.Read("Members","members","title"));
                string countTip = label + L.T("\n共 ") + members.Count + L.T(" 个显示对象\n来源 ") + (model.Instance == null ? L.T("缺少实例") : model.Instance.MemberRule);
                if (model.Instance != null && model.Instance.SampleMembers) countTip += L.T("\n以离线成员样例为准。具体关系由游戏确定。");
                Bind(memberRegion.Get("caption"), label + " (" + members.Count + ")", countTip, null);
                var list = ConfigureList(memberRegion, members.Count);
                for (int n = 0; n < list.Rows*list.Columns; ++n) {
                    int index = list.Offset*list.Columns+n;
                    var flag = Flag(Slot(memberRegion.Get("control"), n), index < members.Count ? members[index] : "",
                        index < members.Count ? model.Loc.Caption(members[index]) + "（" + members[index] + "）\n" + label : "", true);
                    if (flag != null) flag.ListKey = list.Key;
                }
                Auto(memberRegion);
            }
            if (roleRegion != null) ShowRoles(roleRegion);
            if (resourceRegion != null) ShowPrimary(resourceRegion);
            if (registers != null) ShowRegisters(registers);
            if (actions != null) ShowActions(actions);
            var tips = model.Layout.Node.Child("tooltips");
            if (tips != null) foreach (var tip in tips.Nodes) {
                var node = ByName(tip.Key);
                if (node != null && String.IsNullOrWhiteSpace(node.Tooltip)) Bind(node.Name, null, ResolveTip(tip.Value, 0, 0, ""), null);
            }
        }
        private void Build(Node parent, int[] parentKey, Point at, Size size, Rectangle clip) {
            for (int i = 0; i < parent.Children.Count; ++i) {
                var node = parent.Children[i];
                string name = node.Get("name");
                if (!node.Block || name.Length == 0) continue;
                var key = (parentKey ?? new int[0]).Concat(new[] { i }).ToArray();
                var origin = Point.Add(at, new Size(Geometry.At(node, size)));
                string sprite = Model.BitmapFor(node);
                bool text = node.Key == "instantTextBoxType" || node.Key == "editBoxType";
                bool frame = node.Key == "windowType" || node.Key == "containerWindowType";
                bool known = frame || text || node.Key == "guiButtonType" || node.Key == "iconType" || node.Key == "scrollbarType";
                var fallback=text?new Size(120,24):Model.Images==null?new Size(36,36):Model.Images.SizeOf(sprite,new Size(36,36));
                Size dimensions = frame ? Geometry.SizeOf(node, size) : Geometry.SizeOf(node, fallback);
                if (name == BackgroundControl) dimensions = Panel;
                var v = new VisualItem { Node = node, Key = key, Name = name, Raw = Geometry.Raw(node), Sprite = sprite,
                    Bounds = new Rectangle(origin, dimensions), Clip = clip, Text = Model.Loc.Caption(node.Get("text", node.Get("buttonText"))),
                    Tooltip = Model.Loc.Caption(node.Get("tooltipText")), Hit = node.Key == "guiButtonType" || node.Key == "scrollbarType",
                    Hidden = node.Get("visible") == "no" || node.Get("show") == "no" || !known,
                    Transparent = node.Get("alwaystransparent") == "yes" || node.Key == "iconType" || frame,
                    FontSize = FontFrom(node.Get("font",node.Get("buttonFont"))), Positioned = true };
                if (node.Get("enabled") == "no" || node.Get("disabled") == "yes") v.Dimmed = true;
                int frameIndex = Document.Int(node.Get("frame"), 1);
                v.FrameIndex = Math.Max(0, frameIndex - 1);
                Items.Add(v);
                if (!byName.ContainsKey(name)) byName.Add(name, v);
                if (frame && !v.Hidden) Build(node,key,origin,dimensions,Rectangle.Intersect(v.Bounds,clip));
            }
        }
        private static int FontFrom(string key) {
            var parts = key.Split('_');
            return Math.Max(12, Math.Min(31, parts.Select(s => Document.Int(s)).FirstOrDefault(v => v > 0)));
        }
        internal VisualItem ByName(string name) { VisualItem item; return name != null && byName.TryGetValue(name, out item) ? item : null; }
        private void Bind(string name, string label, string tooltip, string action) {
            var item = ByName(name); if (item == null) return;
            if (label != null) item.Text = label;
            if (tooltip != null) item.Tooltip = tooltip;
            item.Action = action; item.Click = !item.Dimmed && action != null;
            item.Hit = item.Click || !String.IsNullOrWhiteSpace(item.Tooltip) || item.Hit;
        }
        private void Hide(string name, bool hidden, string tip = "", string action = null) {
            var item = ByName(name);
            if (item != null) {
                item.Hidden = hidden; Bind(name, null, tip, action);
                if (hidden) item.Click = item.Hit = false;
            }
        }
        private VisualItem Flag(string name, string tag, string tip, bool hideEmpty) {
            var item = ByName(name); if (item == null) return null;
            item.Country = tag; item.Hidden = hideEmpty && String.IsNullOrEmpty(tag);
            Bind(name, null, item.Hidden ? "" : tip, !item.Hidden && tag.Length > 0 && tag != "REB" ? "diplomacy" : null);
            if (item.Hidden) item.Hit = false;
            return item;
        }
        internal static string Slot(string name, int n) { return name.Length == 0 ? "" : name + "_" + n.ToString("D2", CultureInfo.InvariantCulture); }
        private ListViewState ConfigureList(Node region, int total) {
            string slider = region.Get("scrollbar");
            var rail = ByName(slider);
            var list = new ListViewState { Key = region.Key, Slider = slider, Columns = Math.Max(1, Math.Min(16, Document.Int(region.Get("columns"), 1))),
                Rows = Math.Max(1, Math.Min(32, Document.Int(region.Get("rows"), 1))), Total = total,
                Rail = rail == null ? Rectangle.Empty : rail.Bounds };
            int offset; Settings.Offsets.TryGetValue(list.Key, out offset);
            list.Offset = Math.Max(0, Math.Min(list.Maximum, offset));
            list.Bounds = list.Rail;
            if (rail != null) {
                rail.ListKey = list.Key;
                Hide(rail.Name, list.Maximum == 0, L.T("滚动  ·  ") + total + L.T(" 项\n鼠标滚轮、滑块和箭头共用当前位置"), "scroll");
            }
            string frame = region.Get("frame");
            var auto = Model.Window.Doc.Root.Child("guiTypes").Named(frame);
            if (auto != null && region.Get("auto_place") == "yes") list.Bounds = new Rectangle(Geometry.Raw(auto), Geometry.SizeOf(auto, new Size(2,2)));
            else for (int n = 0; n < list.Rows*list.Columns; ++n) {
                var row = ByName(Slot(region.Get("control"), n));
                if (row != null) list.Bounds = list.Bounds.IsEmpty ? row.Bounds : Rectangle.Union(list.Bounds,row.Bounds);
                if(region.Key!="special_members_region")continue;
                foreach(string field in new[]{"label","nomination_score","nomination_tooltip_control","support_shield"}) {
                    var part=ByName(Slot(region.Get(field),n));
                    if(part!=null)list.Bounds=Rectangle.Union(list.Bounds,part.Bounds);
                }
                if(n%list.Columns!=0)continue;
                var heading=ByName(Slot(region.Get("group_tooltip_control"),n/list.Columns));
                if(heading!=null)list.Bounds=Rectangle.Union(list.Bounds,heading.Bounds);
            }
            Lists.Add(list); return list;
        }
        private void Auto(Node region) {
            if (region.Get("auto_place") != "yes") return;
            var frame = Model.Window.Doc.Root.Child("guiTypes").Named(region.Get("frame"));
            if (frame == null) return;
            var box = new Rectangle(Geometry.Raw(frame), Geometry.SizeOf(frame, new Size(1,1)));
            Items.Add(new VisualItem { Node = frame, Raw = box.Location, Bounds = box, Clip = WorldPanel, Name = frame.Get("name"),
                RegionFrame = region.Key, Tooltip = L.T("自动排布框  ·  ") + region.Key + L.T("\n移动该框及配套滑条。"), Hidden = Settings.Playing, Positioned = true });
            if (region.Key == "leader_region") { Place(region, -1, box); return; }
            var scroll = ByName(region.Get("scrollbar"));
            if (scroll == null || scroll.Bounds.Left < box.Left + 5) return;
            int rows = Math.Max(1, Math.Min(32, Document.Int(region.Get("rows")))), columns = Math.Max(1, Math.Min(16, Document.Int(region.Get("columns"))));
            int width = scroll.Bounds.Left - box.Left - 4;
            for (int i = 0; i < rows*columns; ++i) {
                int col = i%columns, row = i/columns, x = col*width/columns, y = row*box.Height/rows;
                Place(region,i,new Rectangle(box.X+x,box.Y+y,(col+1)*width/columns-x,(row+1)*box.Height/rows-y));
            }
        }
        private void Place(Node region, int index, Rectangle cell) {
            var item = ByName(index < 0 ? region.Get("control") : Slot(region.Get("control"), index));
            var label = index < 0 ? null : ByName(Slot(region.Get("label"),index));
            if (item == null) return;
            int width = item.Bounds.Width+(label == null ? 0 : label.Bounds.Width+6);
            var offset = new Point(cell.X+(cell.Width-width)/2-item.Bounds.X, cell.Y+(cell.Height-item.Bounds.Height)/2-item.Bounds.Y);
            ApplyAuto(item, region.Key, offset);
            if (label != null) ApplyAuto(label,region.Key,new Point(item.Bounds.Right+6-label.Bounds.X,cell.Y+(cell.Height-label.Bounds.Height)/2-label.Bounds.Y));
            if (index >= 0) foreach (var key in new[] { "nomination_score","nomination_tooltip_control","support_shield" })
                ApplyAuto(ByName(Slot(region.Get(key),index)),region.Key,offset);
        }
        private static void ApplyAuto(VisualItem item, string region, Point offset) {
            if (item == null) return;
            var rect = item.Bounds; rect.Offset(offset); item.Bounds = rect;
            item.DisplayOffset = offset; item.AutoRegion = region; item.Locked = true;
        }
        private void ShowRoles(Node region) {
            var roles = Model.Rule.Get("special_members");
            int columns = Math.Max(1, Math.Min(16, Document.Int(region.Get("columns"),1)));
            string voters = Model.Rule.Read("","leader","succession","electors_role");
            if (roles != null) foreach (var role in roles.Children.Where(r => r.Block)) {
                var tags = new List<string>();
                if (Model.Instance != null) Model.Instance.Roles.TryGetValue(role.Key,out tags);
                int size = Math.Min(32, Math.Max(1, Document.Int(role.Read("1","max_holders"),1)));
                int padded = (size+columns-1)/columns*columns;
                string heading = Model.Loc.Caption(role.Read(role.Key, "title_key"));
                string tip = Model.Loc.Caption(role.Read("", "tooltip"));
                for (int i = 0; i < padded; ++i) {
                    string tag = i >= size ? "" : tags != null && i < tags.Count ? tags[i] : "REB";
                    offices.Add(new OfficeSeat { Role = role.Key, Tag = tag, Heading = heading, Tip = tip, First = i == 0 });
                }
            }
            var list = ConfigureList(region,offices.Count);
            Bind(region.Get("caption"), offices.Count == 0 ? "" : Model.Loc.Caption("PS_ROLE_HEADING"), L.T("职位分组按类型文件中的顺序。\n未任命席位保留 REB 盾徽。"), null);
            for (int n = 0; n < list.Rows*list.Columns; ++n) {
                int index = list.Offset*list.Columns+n, r = n/list.Columns;
                var seat = index < offices.Count ? offices[index] : null;
                string suffix = seat == null ? "" : seat.Tag == "REB" ? L.T("\n席位空缺") : "\n" + seat.Tag;
                var flag = Flag(Slot(region.Get("control"),n),seat == null ? "" : seat.Tag,
                    seat == null ? "" : seat.Heading + (seat.Tag=="REB"?suffix:"\n"+Model.Loc.Caption(seat.Tag)+"（"+seat.Tag+"）") + "\n" + seat.Tip,true);
                if (flag != null) { flag.ListKey = list.Key; if (seat != null) flag.Role = seat.Role; }
                var label = Slot(region.Get("label"),n);
                Hide(label,seat == null); Bind(label,seat == null ? "" : seat.Heading,seat == null ? "" : seat.Tip,null);
                string num = Slot(region.Get("nomination_score"),n), hit = Slot(region.Get("nomination_tooltip_control"),n), support = Slot(region.Get("support_shield"),n);
                bool elector = seat != null && seat.Role == voters && seat.Tag != "" && seat.Tag != "REB";
                string supportTag = !elector || Model.Instance == null ? "" : Model.Instance.Leader;
                string details = !elector ? "" : L.T("选举人 ") + seat.Tag + L.T(" 对 ") + Model.Country + L.T(" 的提名评分\n范围 ") +
                    Model.Rule.Read("-2000","leader","succession","nomination","min") + "…"+Model.Rule.Read("2000","leader","succession","nomination","max") +
                    L.T("\n原版意见、触发条件和真实支持国须在 EU4 实测。");
                Hide(num,!elector); Bind(num,elector ? "—" : "",hit=="" ? details : "",null);
                Hide(hit,!elector); Bind(hit,null,details,null);
                Flag(support,supportTag, L.T("演示支持目标  ·  ") + supportTag + L.T("\n实际投票由 EU4 决定。"),true);
                if (n%list.Columns != 0) continue;
                bool show = seat != null && (seat.First || n == 0);
                string title = Slot(region.Get("group_heading"),r), groupHit = Slot(region.Get("group_tooltip_control"),r);
                Hide(title,!show); Bind(title,show ? seat.Heading : "",show ? seat.Tip : "",null);
                Hide(groupHit,!show); Bind(groupHit,null,show ? seat.Tip : "",null);
            }
            Auto(region);
        }
        internal static decimal Number(Field field, string key, decimal fallback = 0m) {
            decimal d;
            return field != null && Decimal.TryParse(field.Read("",key),NumberStyles.Number,CultureInfo.InvariantCulture,out d) ? Math.Max(-100000000m,Math.Min(100000000m,d)) : fallback;
        }
        private void ShowPrimary(Node region) {
            var primary = Model.Primary;
            if (primary == null) return;
            decimal amount = Model.Amount, growth = Number(primary.Get("monthly_growth"),"value");
            var added = new List<string>();
            var monthly = primary.Get("monthly_growth");
            decimal sum = growth, modifiers = 0;
            bool unknown = false;
            if (monthly != null) foreach (var term in monthly.Children.Where(t => t.Block)) {
                var trigger=term.Get("trigger");
                if (term.Key=="growth_modifier"||trigger!=null&&trigger.Children.Any(t=>t.Key!="always"&&t.Key!="tag"&&t.Key!="political_has_reform")) {
                    unknown=true;added.Add(term.Key+L.T("  ·  依赖游戏条件或原版修正"));continue;
                }
                if(trigger!=null&&!trigger.Children.All(t=>t.Key=="always"?t.Value=="yes":t.Key=="tag"?t.Value==Model.Country:
                    Settings.Passed.Contains("reforms."+t.Read("","id")))) {
                    added.Add(term.Key+L.T("  ·  样例条件未满足"));
                    continue;
                }
                decimal add = Number(term,"add"), modifier = Number(term,"modifier");
                sum += add; modifiers += modifier;
                added.Add(Model.Loc.Caption(term.Read(term.Key,"tooltip")) + "  "+ (term.Get("modifier") == null ? add.ToString("+0.##;-0.##;0",CultureInfo.InvariantCulture) : modifier.ToString("+0.##%;-0.##%;0%",CultureInfo.InvariantCulture)));
            }
            sum *= Math.Max(0,1+modifiers);
            string owner = primary.Read("instance","owner") == "country" ? L.T("各国专有  ·  ") + Model.Country : L.T("联盟共用  ·  ") + (Model.Instance == null ? L.T("未建立") : Model.Instance.Id);
            string low = primary.Read("0","min"), high = primary.Read("100","max");
            bool historical=Model.Instance!=null&&Model.Instance.Balances.ContainsKey(primary.Key);
            string source = owner + L.T("\n资源 ") + primary.Key + (historical?L.T("\n历史覆盖余额 "):L.T("\n开局初始值 ")) + Format(amount) + L.T("\n限制 ") + low + "…"+ high + L.T("\n来源 ") + (historical?Model.Instance.From.Path:primary.From.Path);
            var capped=Math.Min(Number(primary,"max",100m),Math.Max(Number(primary,"min"),amount+sum))-amount;
            string change = owner + L.T("\n月变基准 ") + Format(growth) + (added.Count == 0 ? "" : "\n" + String.Join("\n",added)) +
                L.T("\n合计倍率 ")+(1+modifiers).ToString("P0",CultureInfo.InvariantCulture) + L.T("\n封顶前 ") + Format(sum) +
                (capped==sum?"":L.T("\n上下限调整 ") + Format(capped-sum)) + L.T("\n样例合计 ") + Format(capped) +
                (unknown ? L.T("\n部分来源须在游戏读取。") : "");
            string title = Model.Loc.Caption(primary.Read(primary.Key,"name_key"));
            string key = primary.Read("","tooltip");
            Bind(region.Get("value_control"),Format(amount), tipText(title,key,amount,source,primary),null);
            Bind(region.Get("growth_control"),capped >= 0 ? "+" + Format(capped) : Format(capped),tipText(title,key,capped,change,primary),null);
            Bind(region.Get("tooltip_control"),null,source,null);
        }
        private string Format(decimal value) { return value.ToString("F"+Model.Precision,CultureInfo.InvariantCulture); }
        private string tipText(string title,string key,decimal amount,string detail,Field resource) {
            var loc=ResolveTip(key,amount,Number(resource,"max",100m),detail,resource);
            return title+(loc.Length==0?"":"\n"+loc)+(loc.Contains(detail)?"":"\n"+detail);
        }
        private string ResolveTip(string key, decimal value, decimal max, string detail, Field resource=null) {
            if (String.IsNullOrWhiteSpace(key)) return "";
            var dict = new Dictionary<string,object>(StringComparer.OrdinalIgnoreCase);
            var definitions=Model.Mount.Paths.Where(p => p.StartsWith("common/political_systems/localisation/") && p.EndsWith(".txt")).Select(p=>Model.Doc(p)).ToArray();
            var spec=definitions.Select(d=>d.Root.Child(key)).FirstOrDefault(p=>p!=null);
            var parameters=spec==null?null:spec.Child("parameters");
            string template=spec==null?key:spec.Get("template",key);
            if(parameters!=null)foreach (var parameter in parameters.Nodes) {
                    string id=parameter.Get("id"), source=parameter.Get("source",parameter.Value??"");
                    var forResource=id==""?resource:Model.ResourceGroup==null?null:Model.ResourceGroup.Get(id);
                    switch (source.ToLowerInvariant()) {
                        case "system": dict[parameter.Key] = Model.Title; break;
                        case "instance": dict[parameter.Key] = Model.Instance == null ? "" : Model.Instance.Id; break;
                        case "type": dict[parameter.Key] = Model.Type.Node.Key; break;
                        case "val":
                        case "value": dict[parameter.Key] = value; break;
                        case "maximum": if(forResource!=null)dict[parameter.Key] =Number(forResource,"max",max); break;
                        case "minimum": if(forResource!=null)dict[parameter.Key] =Number(forResource,"min"); break;
                        case "initial": if(forResource!=null)dict[parameter.Key] =Number(forResource,"initial"); break;
                        case "balance": if(forResource!=null)dict[parameter.Key]=Balance(forResource); break;
                        case "why":
                        case "breakdown": dict[parameter.Key] = detail; break;
                        case "country": dict[parameter.Key] = Model.Loc.Caption(Model.Country); break;
                        case "leader": if(Model.Instance!=null)dict[parameter.Key]=Model.Loc.Caption(Model.Instance.Leader); break;
                        case "localisation": if(id!="")dict[parameter.Key]=Model.Loc.Caption(id); break;
                        case "member_count": if(Model.Instance!=null)dict[parameter.Key]=Model.Instance.Members.Count; break;
                        case "role_count":
                            List<string> holders;
                            if(Model.Instance!=null&&Model.Instance.Roles.TryGetValue(id,out holders))dict[parameter.Key]=holders.Count;
                            break;
                        case "resource": case "name": if(forResource!=null)dict[parameter.Key]=Model.Loc.Caption(forResource.Read(forResource.Key,"name_key")); break;
                    }
            }
            else {dict["VAL"]=value;dict["VALUE"]=value;dict["SYSTEM"]=Model.Title;dict["BREAKDOWN"]=detail;dict["WHY"]=detail;}
            return Model.Loc.Format(template,dict);
        }
        private decimal Balance(Field resource) {
            decimal value;
            return Model.Instance!=null&&Model.Instance.Balances.TryGetValue(resource.Key,out value)?value:Number(resource,"initial");
        }
        private void ShowRegisters(Node region) {
            var group = Model.ResourceGroup;
            var registers = group == null ? new Field[0] : group.Children.Where(f => f.Block && f != Model.Primary).ToArray();
            var list = ConfigureList(region,registers.Length);
            for (int i = 0; i < list.Rows; ++i) {
                int index = list.Offset+i; var r = index < registers.Length ? registers[index] : null;
                string name = Slot(region.Get("control"),i), value = Slot(region.Get("value_control"),i), hit = Slot(region.Get("tooltip_control"),i);
                Hide(name,r==null); Hide(value,r==null); Hide(hit,r==null);
                if (r == null) continue;
                var amount = Balance(r);
                string label = Model.Loc.Caption(r.Read(r.Key,"name_key"));
                string detail = (r.Read("instance","owner") == "country" ? L.T("各国专有  ·  ")+Model.Country : L.T("联盟共用")) +
                    L.T("\n初始余额 ") + amount + L.T("\n来源 ") + r.From.Path;
                string tip = tipText(label,r.Read("","tooltip"),amount,detail,r);
                Bind(name,label,tip,null); Bind(value,amount.ToString("F"+Math.Max(0,Math.Min(6,Document.Int(r.Read("2","precision")))),CultureInfo.InvariantCulture),tip,null); Bind(hit,null,tip,null);
            }
        }
        private void ShowActions(Node region) {
            foreach (var module in new[] { "reforms","decrees","decisions","incidents","diplomatic_actions" }) {
                var definitions = Model.Rule.Get(module);
                if (definitions == null) continue;
                foreach (var a in definitions.Children.Where(f => f.Block)) {
                    string key = module+"."+a.Key;
                    var needed = a.Get("requires");
                    string denied = needed == null ? "" : String.Join(", ", needed.Children.Select(x=>x.Key).Where(s=>!Settings.Passed.Contains("reforms."+s)));
                    bool leader = Model.Instance != null && Model.Instance.Leader == Model.Country;
                    bool passed = Settings.Passed.Contains(key);
                    string why = denied.Length > 0 ? L.T("前置行动 ") + denied : !leader && a.Read("leader","actor_rule") == "leader" ? L.T("仅领袖可执行") : "";
                    string cost = a.Read("","cost","amount");
                    decimal d;
                    if (why == "" && Decimal.TryParse(cost,NumberStyles.Number,CultureInfo.InvariantCulture,out d) && Model.Amount < d) why = L.T("样例资源不足");
                    bool enabled = !passed && why.Length == 0;
                    string caption = Model.Loc.Caption(a.Read(a.Key,"name"));
                    string tip = caption + "\n" + ResolveTip(a.Read("","tooltip"),0,0,"") + "\n" + (passed ? L.T("已完成（模拟）") : enabled ? L.T("可执行（模拟）") : why);
                    if (cost.Length > 0) tip += L.T("\n费用 ") + cost + " " + a.Read("","cost","resource");
                    if (a.Get("trigger") != null || a.Get("allow") != null) tip += L.T("\n游戏条件尚待实测。");
                    Actions.Add(new ActionView { Module = module, Id = a.Key, Name = caption, Enabled = enabled, Passed = passed, Tip = tip, Requirement = why, Definition = a });
                }
            }
            var shown=Actions.Where(a=>a.Definition.Read("","ui","region")==region.Key).ToArray();
            var list = ConfigureList(region,shown.Length);
            for (int i = 0; i < list.Rows; ++i) {
                int index = list.Offset+i; var action = index < shown.Length ? shown[index] : null;
                string button = Slot(region.Get("control"),i), label = Slot(region.Get("label"),i);
                string ro = Slot(region.Get("read_only_control"),i), seal = Slot(region.Get("seal_control"),i), check = Slot(region.Get("check_control"),i);
                Hide(button,action == null || !action.Enabled && ByName(ro)!=null);
                Hide(label,action == null); Hide(ro,action == null || action.Enabled); Hide(seal,action == null || !action.Enabled); Hide(check,action == null || !action.Passed);
                if (action == null) continue;
                string invoke=action.Enabled?"action:"+action.Module+"."+action.Id:null;
                Bind(label,action.Name,action.Tip,invoke); Bind(button,null,action.Tip,invoke);
                Bind(ro,null,action.Tip,null); Bind(seal,null,action.Tip,null); Bind(check,null,action.Tip,null);
                if (ByName(label) != null) ByName(label).Dimmed = !action.Enabled;
            }
        }
    }
}
