using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace PoliticalEditor {
    internal sealed class Plan {
        internal readonly Project Project;
        internal readonly Mount Mount;
        internal readonly Dictionary<string, Entry> Before, After;
        internal readonly string Title;
        internal Plan(Project project, Mount mount, string title) {
            Project = project; Mount = mount; Title = title; Before = project.Content; After = project.Draft();
        }
        internal Entry Get(string path) {
            Entry e; return After.TryGetValue(path, out e) ? e : Mount.Get(path, true);
        }
        internal Document Read(string path) {
            var e = Get(path);
            var doc = new Document(path, e == null ? "" : TextFile.Decode(e.Data).Text);
            if (doc.Problems.Count != 0) throw new InvalidDataException(path + L.T(" 行 ") + doc.Line(doc.Problems[0].Offset) + "  " + doc.Problems[0].Message);
            return doc;
        }
        internal void Text(string path, string text) { Project.PutText(After, path, text, Get(path)); }
        internal void Put(string path, byte[] data) { Project.Put(After, path, data, Get(path)); }
        internal void Set(string path, string rootId, string[] keys, string value) {
            var doc = Read(path); var node = doc.Root.Child(rootId);
            if (node == null || !node.Block) throw new InvalidDataException(L.T("无法找到 ") + rootId + "。");
            Text(path, doc.SetPath(node, keys, 0, value));
        }
        internal void Commit() {
            if (!ReferenceEquals(Project.Content, Before)) throw new InvalidOperationException(L.T("草稿已有更新。请重新预览这次修改。"));
            Project.Apply(After, Title);
        }
        internal IEnumerable<string> Changes {
            get { return After.Keys.Where(p => !Before.ContainsKey(p) || !Files.Same(Before[p].Data, After[p].Data)).OrderBy(p => p, Files.Windows); }
        }
        internal string Diff(string path) {
            Entry entry;
            var old = Before.TryGetValue(path, out entry) ? entry : Mount.Get(path, true);
            var current = After[path];
            return EditorDiff.Create(path, old == null ? null : old.Data, current.Data);
        }
    }

    internal static class EditorDiff {
        internal static string Create(string path, byte[] old, byte[] data) {
            string first = "--- " + (old == null ? L.T("新增") : L.T("原文件")) + "  " + path + L.T("\r\n+++ 拟应用  ") + path + "\r\n";
            if (path.EndsWith(".dds", StringComparison.OrdinalIgnoreCase) || path.EndsWith(".tga", StringComparison.OrdinalIgnoreCase) || path.EndsWith(".png", StringComparison.OrdinalIgnoreCase))
                return first + L.T("贴图字节数 ") + data.Length + "\r\nSHA-256 " + Files.Hash(data);
            var a = (old == null ? "" : Document.ViewText(TextFile.Decode(old).Text)).Split('\n');
            var b = Document.ViewText(TextFile.Decode(data).Text).Split('\n');
            var result = new StringBuilder(first);
            int i = 0, j = 0, last = -100;
            while (i < a.Length || j < b.Length) {
                if (i < a.Length && j < b.Length && a[i] == b[j]) { ++i; ++j; continue; }
                int commonI = a.Length, commonJ = b.Length, best = 300;
                for (int x = i; x < Math.Min(a.Length, i+120); ++x) {
                    for (int y = j; y < Math.Min(b.Length, j+120); ++y) {
                        int cost = x-i+y-j;
                        if (cost >= best || a[x] != b[y] || String.IsNullOrWhiteSpace(a[x]) || a[x].Trim() == "}") continue;
                        if ((x+1 < a.Length && y+1 < b.Length && a[x+1] == b[y+1]) || cost == 1) {
                            best = cost; commonI = x; commonJ = y;
                        }
                    }
                }
                if (i-last > 3) result.AppendLine(L.T("@@  原行 ") + (i+1) + L.T(" → 新行 ") + (j+1));
                while (i < commonI && result.Length < 290000) result.AppendLine("- " + a[i++]);
                while (j < commonJ && result.Length < 290000) result.AppendLine("+ " + b[j++]);
                if (result.Length >= 290000) { result.AppendLine(L.T("差异过长，查看完整文件。")); break; }
                last = i;
            }
            return result.ToString();
        }
    }

    internal sealed class VisualTarget {
        internal string Gui, Window, Layout, LayoutId;
    }

    internal static partial class Editing {
        internal static Project Default(RootMap roots, string id, string instance, string date, string country, bool shogun = false) {
            CheckNew(id, instance, date, country);
            var clean = roots.Copy(); clean.Mod = ""; clean.Dependencies = new string[0];
            var p = new Project(clean);
            p.Data.Title = shogun ? L.T("室町幕府练习项目") : L.T("政治体系起步项目");
            p.Data.Type = id; p.Data.Instance = instance; p.Data.Date = shogun && date=="1444.1.1"?"1444.11.11":date; p.Data.Country = country;
            var seed = new Dictionary<string, string> {
                { Model.LayoutDir+"pf075_layouts.txt", "starter.layout" }, { "interface/pf075_panels.gui", "starter.gui" },
                { "interface/pf075_icons.gfx", "starter.gfx" }, { "localisation/pf075_presets_l_english.yml", "starter.loc" },
                { "interface/ps_instance_selector.gui", "starter.selector" }, { "localisation/ps_interface_l_english.yml", "starter.runtime_loc" },
                { Model.TypeDir+"pf075_styles.txt", "starter.styles" }, { "reference/political_systems/default.txt", "starter.reference" }
            };
            foreach (var pair in seed) {
                var bytes = Files.Resource(pair.Value);
                if (bytes != null) Project.Put(p.Content, pair.Key, bytes);
            }
            var lang = new StringBuilder("l_english:\n");
            string loc = id + "_title", lead = id + "_leader", member = id + "_member", amount = id + "_resource";
            lang.AppendLine(" " + loc + ":0 \"" + (shogun ? "Shogun System" : "New Political System") + "\"");
            lang.AppendLine(" " + lead + ":0 \"" + (shogun ? "Shogun" : "Leader") + "\"");
            lang.AppendLine(" " + member + ":0 \"" + (shogun ? "Daimyo" : "Members") + "\"");
            lang.AppendLine(" " + amount + ":0 \"" + (shogun ? "Shogun Resource" : "Authority") + "\"");
            lang.AppendLine(" " + amount + "_tip:0 \"Resource balance and monthly growth\"");
            lang.AppendLine(" " + id + "_entry:0 \"Open " + (shogun ? "Shogun System" : "Political System") + "\"");
            lang.AppendLine(" editor_kanrei:0 \"Kanrei\"\n editor_samurai:0 \"Samurai-dokoro\"\n editor_office_tip:0 \"Appointed seat\"");
            var rule = new StringBuilder();
            rule.AppendLine("# 类型是一套规则。历史文件中的实例使用这套规则。");
            rule.AppendLine(id + " = {");
            if (shogun) rule.AppendLine("    copy_from = celestial_style");
            rule.AppendLine("    title = " + loc + " # 面板标题的本地化键");
            rule.AppendLine("    leader = { title = " + lead + " } # 领袖栏标题");
            rule.AppendLine("    members = { source = " + (shogun ? "leader_subjects subject_type = daimyo_vassal" : "explicit") + " title = " + member + " }");
            rule.AppendLine("    resources = {\n" + (shogun ? "        mandate = clear\n" : "") + "        " + amount + " = { initial = 55 precision = 2 name_key = " + amount + " tooltip = " + amount + "_tip }\n    }");
            if (shogun) {
                rule.AppendLine("    reforms = { charter = { cost = { resource = " + amount + " payer = instance amount = 20 } } }");
                rule.AppendLine("    special_members = {\n        kanrei = { title_key = editor_kanrei tooltip = editor_office_tip acquisition = appointed max_holders = 3 require_member = yes }");
                rule.AppendLine("        samurai_dokoro = { title_key = editor_samurai tooltip = editor_office_tip acquisition = appointed max_holders = 4 require_member = yes }\n    }");
            }
            rule.AppendLine("    interface = {");
            rule.AppendLine("        layout = celestial_layout background = GFX_eoc_bg_dom title_key = " + loc);
            rule.AppendLine("        entry_button = { region = pf075_style_entries icon = GFX_pf075_celestial_icon order = 30 tooltip = " + id + "_entry }\n    }\n}");
            Project.PutText(p.Content, Model.TypeDir + id + ".txt", rule.ToString(), null);
            string history = "# 该日期及以后的新开战役读取领袖，以及截至开局日的历史变更。\n" + instance + " = {\n    type = " + id + "\n    name = " + loc + "\n    " + date + " = {\n        leader = " + country + "\n" +
                (shogun ? "        roles = { kanrei = { HSK = yes SBA = yes HTK = yes } samurai_dokoro = { ISK = yes YMN = yes } }\n" : "        members = { " + country + " }\n") + "    }\n}\n";
            Project.PutText(p.Content, Model.HistoryDir + instance + ".txt", history, null);
            Project.PutText(p.Content, "localisation/" + id + "_l_english.yml", lang.ToString(), null);
            Project.PutText(p.Content, "descriptor.mod", "name = " + Document.Quote(p.Data.Title) + "\nsupported_version = \"1.37.*\"\n", null);
            p.Touch(); return p;
        }
        private static void CheckNew(string id, string instance, string date, string country) {
            if (!Files.IsId(id) || !Files.IsId(instance) || id.Length > 50 || instance.Length > 50)
                throw new InvalidDataException(L.T("类型和实例 ID 需以英文字母或下划线开头，最多 50 位。"));
            if (Model.DateKey(date) < 0 || country == null || country.Length != 3 || !country.All(c => c >= 'A' && c <= 'Z' || c >= '0' && c <= '9'))
                throw new InvalidDataException(L.T("请填写有效日期和三个字符的国家标签，例如 1444.1.1 和 ASK。"));
        }
        internal static Plan FromTemplate(Project p, Model model, string parent, string layout, string id, string instance, string date, string country, bool copy) {
            CheckNew(id, instance, date, country);
            if (model.Types.ContainsKey(id) || model.Mount.Paths.Any(file => file.StartsWith(Model.HistoryDir) &&
                file.EndsWith(".txt") && model.Doc(file).Root.Child(instance) != null))
                throw new InvalidDataException(L.T("类型或实例 ID 已被占用。"));
            if (!model.Types.ContainsKey(parent) || !model.Layouts.ContainsKey(layout)) throw new InvalidDataException(L.T("请选择可解析的类型和布局。"));
            Field rule = model.Resolve(parent, new List<string>());
            if (rule == null) throw new InvalidDataException(L.T("模板的继承链尚未解析成功。"));
            var plan = new Plan(p, model.Mount, copy ? L.T("复制模板") : L.T("继承模板"));
            string path = Model.TypeDir + id + ".txt";
            if (plan.Get(path) != null) throw new IOException(L.T("目标类型文件已存在 ") + path);
            string copyText = copy ? String.Join("\n", rule.Children.Where(f => f.Key != "abstract").Select(f => "    " + f.Render(1))) : "    copy_from = " + parent;
            var doc = new Document(path, id + " = {\n" + copyText + "\n}\n");
            plan.Text(path, doc.SetPath(doc.Root.Child(id), new[] { "interface", "layout" }, 0, layout));
            plan.Set(path, id, new[] { "abstract" }, "no");
            plan.Set(path, id, new[] { "title" }, id + "_title");
            plan.Set(path, id, new[] { "interface", "title_key" }, id + "_title");
            path = Model.HistoryDir + instance + ".txt";
            if (plan.Get(path) != null) throw new IOException(L.T("目标历史文件已存在 ") + path);
            plan.Text(path, instance + " = {\n    type = " + id + "\n    name = " + id + "_title\n    " + date + " = { leader = " + country + " }\n}\n");
            path = "localisation/" + id + "_" + model.Language + ".yml";
            if (plan.Get(path) != null) throw new IOException(L.T("目标本地化文件已存在 ") + path);
            plan.Text(path, model.Language + ":\n " + id + "_title:0 \"" + id + "\"\n");
            if (copy) {
                var view = new Model(new Mount(p.Data.Roots, plan.After, CancellationToken.None), id, instance, country, date, model.Language, CancellationToken.None, model.Images);
                EnsureVisual(plan, view, false, true);
            }
            return plan;
        }
        internal static VisualTarget EnsureVisual(Plan plan, Model m, bool shared, bool force = false) {
            if (m.Layout == null || m.Window == null || m.Type == null) throw new InvalidDataException(L.T("请先修复布局与原生窗口的绑定。"));
            var t = new VisualTarget { Gui = m.GuiPath, Window = m.Window.Node.Get("name"), Layout = m.Layout.Path, LayoutId = m.Layout.Node.Key };
            var users = m.LayoutUsers(t.LayoutId).ToArray();
            bool sharesWindow = m.Layouts.Values.Any(l => l.Node.Key != t.LayoutId && l.Node.Get("gui_file") == t.Gui && l.Node.Get("window") == t.Window);
            if (shared || !force && users.Length <= 1 && !sharesWindow) return t;
            string ownId = m.Type.Node.Key + "_editor";
            if (m.Layouts.ContainsKey(ownId)) throw new InvalidOperationException(L.T("独立布局 ID 已占用 ") + ownId);
            var d = plan.Read(t.Gui); var root = d.Root.Child("guiTypes"); var panel = root.Named(t.Window);
            string newWindow = "pf_" + ownId;
            if (root.Named(newWindow) != null) throw new InvalidOperationException(L.T("独立窗口 ID 已占用 ") + newWindow);
            string clone = RenameGui(d.At(panel), newWindow);
            string newLayout = ownId + " = " + m.Layout.Doc.Text.Substring(m.Layout.Node.ValueStart, m.Layout.Node.End - m.Layout.Node.ValueStart);
            var source = new Document(t.Layout, newLayout);
            newLayout = source.Set(source.Root.Child(ownId), "window", newWindow);
            var regions = m.Layout.Node.Child("regions");
            if (regions != null) foreach (var region in regions.Nodes.Where(r => r.Get("auto_place") == "yes" && r.Get("frame").Length > 0)) {
                var frame = root.Named(region.Get("frame"));
                if (frame == null) throw new InvalidDataException(L.T("原生自动框不存在。"));
                string name = "pf_" + ownId + "_" + region.Key.Replace("_region", "");
                if (name.Length > 79 || root.Named(name) != null) throw new InvalidDataException(L.T("自动框 ID 过长或已有定义。"));
                clone += "\n\n" + RenameGui(d.At(frame), name);
                source = new Document(t.Layout, newLayout);
                newLayout = source.SetPath(source.Root.Child(ownId), new[] { "regions", region.Key, "frame" }, 0, name);
            }
            plan.Text(t.Gui, d.Add(root, clone));
            d = plan.Read(t.Layout);
            plan.Text(t.Layout, d.Add(d.Root, newLayout));
            plan.Set(m.Type.Path, m.Type.Node.Key, new[] { "interface", "layout" }, ownId);
            t.Window = newWindow; t.LayoutId = ownId;
            return t;
        }
        private static string RenameGui(string source, string name) {
            var fragment = new Document("", source);
            return fragment.Set(fragment.Root.Nodes.First(), "name", Document.Quote(name));
        }
        internal static Node GuiControl(Document d, string window, int[] key) {
            var node = d.Root.Child("guiTypes").Named(window);
            if (key != null) foreach (var k in key) node = node == null || k < 0 || k >= node.Children.Count ? null : node.Children[k];
            return node;
        }
        internal static Plan Move(Project p, Model m, VisualItem item, int x, int y, bool shared, bool moveSeat=false) {
            if (item.Locked) throw new InvalidOperationException(L.T("此坐标由自动区域计算。请移动区域框或切成手动。"));
            var plan = new Plan(p, m.Mount, L.T("移动 ") + item.Name);
            VisualTarget target = EnsureVisual(plan, m, shared);
            var gui = plan.Read(target.Gui);
            var root = gui.Root.Child("guiTypes").Named(target.Window);
            if (item.RegionFrame != null) {
                var region = plan.Read(target.Layout).Root.Child(target.LayoutId).Child("regions").Child(item.RegionFrame);
                var frame = gui.Root.Child("guiTypes").Named(region.Get("frame"));
                if (frame == null) throw new InvalidDataException(L.T("自动框已改名，请重选。"));
                string slider = region.Get("scrollbar");
                var scrollNode = root.Named(slider);
                if (scrollNode != null) {
                    var current = Geometry.Raw(scrollNode);
                    plan.Text(target.Gui, gui.Position(scrollNode, current.X + x-item.Raw.X, current.Y + y-item.Raw.Y));
                    gui = plan.Read(target.Gui); frame = gui.Root.Child("guiTypes").Named(region.Get("frame"));
                }
                plan.Text(target.Gui, gui.Position(frame, x, y));
                return plan;
            }
            var node = GuiControl(gui, target.Window, item.Key);
            if (node == null || node.Get("name") != item.Name || !node.Block) throw new InvalidOperationException(L.T("坐标节点已变化。请重新选择控件。"));
            if (item.Key.Length == 1 && item.Name == LeaderSurface(m))
                throw new InvalidOperationException(L.T("悬停区跟随领袖姓名。请选中领袖姓名后移动。"));
            var binding=ControlBindings.ForItem(m,item);
            if(item.Key.Length==1&&MoveOfficeParts(plan,target,gui,root,node,binding,new Point(x,y),moveSeat))return plan;
            var companion = IconCompanion(gui, root, node);
            plan.Text(target.Gui, gui.Position(node, x, y));
            if (companion != null) {
                string tag = companion.Get("name");
                var raw = Geometry.Raw(companion);
                gui = plan.Read(target.Gui);
                var updated = gui.Root.Child("guiTypes").Named(target.Window).Named(tag);
                plan.Text(target.Gui, gui.Position(updated, raw.X + x-item.Raw.X, raw.Y + y-item.Raw.Y));
            }
            if (item.Key.Length == 1 && item.Name == LeaderName(m)) AlignLeaderSurface(plan, target);
            if(binding!=null&&binding.Path!="regions.primary_resource_region.tooltip_control"&&
                binding.Path!="regions.special_members_region.group_tooltip_control"&&binding.Path!="regions.actions_region.read_only_control")
                AlignReadOnlyControls(plan,target,binding.Path,false);
            return plan;
        }
        private const string HintSuffix = "_pf_hint";
        private const string HintMarker = "# pf_editor_paired_icon_hint";
        // The comment is ignored by EU4 and records which two controls the editor owns as one item.
        internal static bool IsIconHint(Document document, Node node) {
            return node != null && node.Key == "instantTextBoxType" && node.Get("name").EndsWith(HintSuffix,StringComparison.Ordinal)
                && document.At(node).Contains(HintMarker);
        }
        private static Node IconCompanion(Document gui, Node window, Node node) {
            if (node.Key=="iconType" && window.Nodes.Contains(node)) {
                var hint=window.Named(node.Get("name")+HintSuffix);
                return IsIconHint(gui,hint)?hint:null;
            }
            if (IsIconHint(gui,node)) {
                var name=node.Get("name");
                var icon=window.Named(name.Substring(0,name.Length-HintSuffix.Length));
                return icon!=null && icon.Key=="iconType"?icon:null;
            }
            return null;
        }
        private static bool RefersToControl(Node field,string name) {
            if (field.Block) return field.Key!="tooltips"&&field.Nodes.Any(n=>RefersToControl(n,name));
            if(field.Value==null||field.Value.Length==0)return false;
            if(field.Value==name)return true;
            return name.Length==field.Value.Length+3&&name.StartsWith(field.Value+"_",StringComparison.Ordinal)
                && Char.IsDigit(name[name.Length-2])&&Char.IsDigit(name[name.Length-1]);
        }
        private static string WithoutNode(Document doc, Node node) {
            int first=node.Start,last=node.End;
            while(first>0&&(doc.Text[first-1]==' '||doc.Text[first-1]=='\t'))--first;
            if(first==0||doc.Text[first-1]=='\n') {
                int end=last;
                while(end<doc.Text.Length&&(doc.Text[end]==' '||doc.Text[end]=='\t'||doc.Text[end]=='\r'))++end;
                if(end<doc.Text.Length&&doc.Text[end]=='\n')return doc.Replace(first,end+1,"");
            }
            return doc.Replace(node.Start,node.End,"");
        }
        internal static Plan DeleteControl(Project project,Model model,VisualItem item,bool shared) {
            if(item.Key==null||item.Key.Length==0||item.Locked||
                !new[]{"iconType","instantTextBoxType","guiButtonType"}.Contains(item.Node.Key))
                throw new InvalidOperationException(L.T("此组件由框架布局管理。请选择独立添加的图标、文字或提示区。"));
            var plan=new Plan(project,model.Mount,L.T("删除组件 ")+item.Name);
            var target=EnsureVisual(plan,model,shared);
            var gui=plan.Read(target.Gui);
            var window=gui.Root.Child("guiTypes").Named(target.Window);
            var node=GuiControl(gui,target.Window,item.Key);
            if(node==null||node.Get("name")!=item.Name||node.Key!=item.Node.Key)
                throw new InvalidOperationException(L.T("组件位置已变化。请重新选择。"));
            var companion=IconCompanion(gui,window,node);
            var removed=companion==null?new[]{node}:new[]{node,companion};
            var names=removed.Select(n=>n.Get("name")).ToArray();
            var layouts=shared?model.Layouts.Values.Where(l=>l.Node.Get("gui_file")==target.Gui&&l.Node.Get("window")==target.Window)
                .Select(l=>Tuple.Create(l.Path,l.Node.Key)).ToArray():new[]{Tuple.Create(target.Layout,target.LayoutId)};
            foreach(var entry in layouts) {
                var layout=plan.Read(entry.Item1);
                var definition=layout.Root.Child(entry.Item2);
                if(names.Any(name=>definition.Nodes.Any(n=>RefersToControl(n,name))))
                    throw new InvalidOperationException("「"+item.Name+L.T("」仍被布局 ")+entry.Item2+L.T(" 使用。请先把该功能改绑到另一个控件。"));
                if(item.Key.Length!=1)continue;
                foreach(var name in names) {
                    layout=plan.Read(entry.Item1);
                    var hints=layout.Root.Child(entry.Item2).Child("tooltips");
                    var hint=hints==null?null:hints.Child(name);
                    if(hint!=null)plan.Text(entry.Item1,WithoutNode(layout,hint));
                }
            }
            var owner=item.Key.Length==1?window:GuiControl(gui,target.Window,item.Key.Take(item.Key.Length-1).ToArray());
            if(owner.Nodes.Except(removed).Any(n=>names.Any(name=>RefersToControl(n,name)))||
                owner.Nodes.Any(n=>n.Key=="backGround"&&names.Contains(n.Value)))
                throw new InvalidOperationException(L.T("同一窗口仍有组件引用「")+item.Name+L.T("」。请先处理该引用。"));
            string text=gui.Text;
            foreach(var part in removed.OrderByDescending(n=>n.Start)) {
                var current=new Document(target.Gui,text);
                var currentNode=part==node?GuiControl(current,target.Window,item.Key):
                    current.Root.Child("guiTypes").Named(target.Window).Named(part.Get("name"));
                if(currentNode==null||currentNode.Get("name")!=part.Get("name"))
                    currentNode=current.Root.Child("guiTypes").Named(target.Window).Named(part.Get("name"));
                text=WithoutNode(current,currentNode);
            }
            plan.Text(target.Gui,text);
            return plan;
        }
        internal static Plan UnpackFrame(Project p, Model m, Scene scene, string regionKey, bool shared) {
            var plan = new Plan(p, m.Mount, L.T("把 ") + regionKey + L.T(" 转为手动位置"));
            var t = EnsureVisual(plan, m, shared);
            var layout = plan.Read(t.Layout);
            var region = layout.Root.Child(t.LayoutId).Child("regions").Child(regionKey);
            if (region.Get("auto_place") != "yes") throw new InvalidOperationException(L.T("该区域已经使用手动坐标。"));
            foreach (var item in scene.Items.Where(i => i.AutoRegion == regionKey && i.Key != null && i.Positioned)) {
                var gui = plan.Read(t.Gui); var raw = GuiControl(gui, t.Window, item.Key);
                var offset = item.DisplayOffset;
                plan.Text(t.Gui, gui.Position(raw, item.Raw.X+offset.X, item.Raw.Y+offset.Y));
            }
            plan.Text(t.Layout, layout.Set(region, "auto_place", "no")); return plan;
        }
        internal static void ChangeTexture(Plan plan, VisualTarget target, VisualItem item, string sprite, Size? imageSize=null) {
            var doc = plan.Read(target.Gui);
            var node = GuiControl(doc, target.Window, item.Key);
            if (node == null || node.Get("name") != item.Name || (node.Key != "iconType" && node.Key != "guiButtonType"))
                throw new InvalidDataException(L.T("当前选中项没有可替换的图片。"));
            var window=doc.Root.Child("guiTypes").Named(target.Window);
            var hint=IconCompanion(doc,window,node);
            plan.Text(target.Gui, doc.Set(node, node.Key == "iconType" ? "spriteType" : "quadTextureSprite", Document.Quote(sprite)));
            if(hint!=null&&hint.Key=="instantTextBoxType"&&node.Child("size")==null) {
                if(!imageSize.HasValue||imageSize.Value.Width<1||imageSize.Value.Height<1)
                    throw new InvalidDataException(L.T("请从素材库或电脑选择图片，以便同步修改悬停范围。"));
                string name=hint.Get("name");
                doc=plan.Read(target.Gui);
                plan.Text(target.Gui,doc.Set(doc.Root.Child("guiTypes").Named(target.Window).Named(name),"maxWidth",imageSize.Value.Width.ToString(CultureInfo.InvariantCulture)));
                doc=plan.Read(target.Gui);
                plan.Text(target.Gui,doc.Set(doc.Root.Child("guiTypes").Named(target.Window).Named(name),"maxHeight",imageSize.Value.Height.ToString(CultureInfo.InvariantCulture)));
            }
        }
        internal static string Register(Plan plan, Model model, string gfxPath, string spriteId, string texture, int frames, byte[] imported, Sprite existing) {
            if (!Files.IsId(spriteId) || !spriteId.StartsWith("GFX_", StringComparison.Ordinal)) throw new InvalidDataException(L.T("sprite ID 请以 GFX_ 开头。"));
            gfxPath = Files.Relative(gfxPath);
            if (!gfxPath.StartsWith("interface/") || !gfxPath.EndsWith(".gfx")) throw new InvalidDataException(L.T("登记文件必须为 interface 下的 .gfx。"));
            if (existing != null && imported == null && existing.Name == spriteId) {
                var candidates = model.Images.Find(spriteId);
                if (candidates.Count != 1) throw new InvalidDataException(L.T("同名素材定义不唯一。可用新 sprite ID 登记所选贴图。"));
                if (existing.Path == gfxPath) return spriteId;
            }
            if (model.Images.Find(spriteId).Any()) throw new InvalidDataException(L.T("已存在这个 sprite ID。请换名或选「复用」。"));
            texture = Files.Relative(imported == null && existing != null ? existing.Texture : texture);
            if (!texture.StartsWith("gfx/", StringComparison.Ordinal) || texture.Length > 125) throw new InvalidDataException(L.T("贴图须在 gfx/ 下并使用短相对路径。"));
            if (imported != null) {
                string ext = Path.GetExtension(texture);
                if (!ext.Equals(".dds", StringComparison.OrdinalIgnoreCase) && !ext.Equals(".tga", StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException(L.T("自有贴图请保留 DDS 或 TGA 扩展名。"));
                using (var bitmap = Raster.Decode(imported, ext)) if (frames < 1 || frames > 128 || bitmap.Width % frames != 0)
                    throw new InvalidDataException(L.T("帧数必须均分贴图宽度，范围 1–128。"));
                var previous = plan.Get(texture);
                if (previous != null && !Files.Same(previous.Data, imported)) throw new IOException(L.T("贴图路径被不同内容占用 ") + texture);
                if (previous == null) plan.Put(texture, imported);
            } else if (plan.Get(texture) == null) throw new FileNotFoundException(L.T("素材未挂载 ") + texture);
            var doc = plan.Read(gfxPath);
            if (doc.Text.Length == 0) doc = new Document(gfxPath, "spriteTypes = {\n}\n");
            var root = doc.Root.Child("spriteTypes");
            if (root == null || !root.Block) throw new InvalidDataException(L.T("登记目标缺少 spriteTypes。"));
            plan.Text(gfxPath, doc.Add(root, "spriteType = { name = " + Document.Quote(spriteId) + " texturefile = " + Document.Quote(texture) + " noOfFrames = " + frames + " }"));
            return spriteId;
        }
        internal static void BindEntry(Plan plan, Model m, string sprite) {
            if (m.Hud == null) throw new InvalidOperationException(L.T("当前类型没有可解析的 HUD 区域。"));
            string prefix = m.Hud.Node.Get("button_template");
            var doc = plan.Read(m.HudGuiPath); var root = doc.Root.Child("guiTypes");
            int slots = Document.Int(m.Hud.Node.Get("capacity")), length = 0;
            if (root == null || slots < 1 || slots > 8) throw new InvalidDataException(L.T("HUD 区域容量或 GUI 根无效。"));
            var first = root.Named(m.Hud.Node.Get("container") + "_00");
            if (first == null) throw new InvalidDataException(L.T("HUD 第一个位置缺失。"));
            var matched = first.Nodes.FirstOrDefault(n => IsHudButton(n, prefix) && n.Get("quadTextureSprite") == sprite);
            if (matched == null) {
                length = first.Nodes.Count(n => IsHudButton(n, prefix));
                if (length >= 32) throw new InvalidOperationException(L.T("HUD 区域支持最多 32 种素材。"));
                if (sprite.Length > 100) throw new InvalidDataException(L.T("入口素材名称过长。"));
                string name = prefix + "_editor_" + length;
                for (int i = 0; i < slots; ++i) {
                    doc = plan.Read(m.HudGuiPath);
                    var slot = doc.Root.Child("guiTypes").Named(m.Hud.Node.Get("container") + "_" + i.ToString("D2", CultureInfo.InvariantCulture));
                    if (slot == null || slot.Named(name) != null) throw new InvalidDataException(L.T("对应的 HUD 位置缺失或按钮名已用。"));
                    var exemplar = slot.Nodes.FirstOrDefault(n => IsHudButton(n, prefix));
                    if (exemplar == null) throw new InvalidDataException(L.T("HUD 位置没有可复用的原生模板。"));
                    var copy = new Document("", RenameGui(doc.At(exemplar), name));
                    string block = copy.Set(copy.Root.Nodes.First(), "quadTextureSprite", Document.Quote(sprite));
                    plan.Text(m.HudGuiPath, doc.Add(slot, block));
                }
            }
            plan.Set(m.Type.Path, m.Type.Node.Key, new[] { "interface", "entry_button", "icon" }, sprite);
        }
        private static bool IsHudButton(Node node, string prefix) {
            return node.Key == "guiButtonType" && (node.Get("name") == prefix ||
                node.Get("name").StartsWith(prefix + "_", StringComparison.Ordinal));
        }
        internal static string CreateCaption(Plan plan,Model model,string name,string message) {
            if(!Files.IsId(name)||String.IsNullOrWhiteSpace(message))throw new InvalidDataException(L.T("请填写组件名称和说明文字。"));
            if(!Files.IsId(model.Language))throw new InvalidDataException(L.T("请在主窗口选定本地化语言。"));
            string key="pf_editor_"+model.Type.Node.Key+"_"+name+"_tip";
            if(key.Length>120)throw new InvalidDataException(L.T("组件名称过长，请缩短后再试。"));
            string path="localisation/pf_editor_"+model.Type.Node.Key+"_"+model.Language+".yml";
            var current=plan.Get(path);
            if(current!=null) {
                var source=TextFile.Decode(current.Data).Text;
                if(source.Split('\n').Any(l=>l.TrimStart().StartsWith(key+":",StringComparison.Ordinal))) {
                    int suffix=2;
                    while(source.Split('\n').Any(l=>l.TrimStart().StartsWith(key+"_"+suffix+":",StringComparison.Ordinal)))++suffix;
                    key+="_"+suffix;
                }
            }
            string original=current==null?model.Language+":\n":TextFile.Decode(current.Data).Text;
            if(!original.TrimStart().StartsWith(model.Language+":",StringComparison.Ordinal))throw new InvalidDataException(L.T("说明文件的语言头不匹配 ")+path);
            string newline=original.Contains("\r\n")?"\r\n":"\n";
            plan.Text(path,original+(original.EndsWith("\n")?"":newline)+" "+key+":0 "+Document.Quote(message.Trim())+newline);
            return key;
        }
        internal static void AddControl(Plan plan, Model m, VisualTarget t, string kind, string name, int x, int y, string sprite, string tooltipKey, string parentName, bool replace, Size? iconSize=null) {
            if (!Files.IsId(name)) throw new InvalidDataException(L.T("控件名必须是合法 ID。"));
            var doc = plan.Read(t.Gui); var window = doc.Root.Child("guiTypes").Named(t.Window);
            var parent = String.IsNullOrWhiteSpace(parentName) || parentName == m.Window.Node.Get("name")
                ? window : FindContainer(window, parentName);
            if (parent == null) throw new InvalidDataException(L.T("找不到父窗口 ") + parentName);
            if (parent.Named(name) != null) throw new InvalidDataException(L.T("已有同名控件 ") + name);
            string xy = " position = { x = " + x + " y = " + y + " }", body;
            string baseline = " name = " + Document.Quote(name) + xy;
            if (kind == "装饰" || kind=="带提示的图标") {
                if (sprite == "") throw new InvalidDataException(L.T("请选择装饰素材。"));
                body = "iconType = {" + baseline + " spriteType = " + Document.Quote(sprite) + " alwaystransparent = yes }";
                if(kind=="带提示的图标") {
                    if(parent!=window) throw new InvalidDataException(L.T("带悬停提示的图标请放在当前体系的主窗口。"));
                    if(!Files.IsId(name+HintSuffix)||parent.Named(name+HintSuffix)!=null)throw new InvalidDataException(L.T("提示控件名称过长或已被使用。请缩短或修改组件名。"));
                    if(!iconSize.HasValue||iconSize.Value.Width<1||iconSize.Value.Height<1)throw new InvalidDataException(L.T("无法确定图标大小，请重新选择一张可预览的素材。"));
                    AddHint(plan,t,name+HintSuffix,tooltipKey);
                    body+="\ninstantTextBoxType = { "+HintMarker+"\n    name = "+Document.Quote(name+HintSuffix)+xy+
                        " font = \"vic_18\" format = left textureFile = \"\" maxWidth = "+iconSize.Value.Width+" maxHeight = "+iconSize.Value.Height+
                        " alwaystransparent = yes text = \"\"\n}";
                }
            } else {
                if (parent != window && kind != "文本") throw new InvalidDataException(L.T("此布局的运行时绑定只查找主窗的直接子控件。"));
                var layout = plan.Read(t.Layout);
                var layoutNode = layout.Root.Child(t.LayoutId);
                if (kind == "余额" || kind == "月变") {
                    string property = kind == "余额" ? "value_control" : "growth_control";
                    Node summary = layoutNode.Child("regions") == null ? null : layoutNode.Child("regions").Child("primary_resource_region");
                    if (summary == null || summary.Get(kind == "余额" ? "growth_control" : "value_control") == "") throw new InvalidDataException(L.T("请在布局里先建立完整的一对余额和月变控件。"));
                    if (!replace && summary.Get(property) != "") throw new InvalidOperationException(L.T("该数值位置已有绑定。先勾选确认替换。"));
                    plan.Text(t.Layout, layout.Set(summary, property, name));
                } else if (tooltipKey.Length > 0) {
                    if (parent != window) throw new InvalidDataException(L.T("附带布局提示的文字必须直接放在主窗内。"));
                    AddHint(plan,t,name,tooltipKey);
                }
                body = "instantTextBoxType = {" + baseline + " font = \"vic_18\" format = left textureFile = \"\" maxWidth = 120 maxHeight = 26 alwaystransparent = yes text = " +
                    Document.Quote(kind == "提示区" || kind == "余额" || kind == "月变" ? "" : tooltipKey) + " }";
            }
            plan.Text(t.Gui, doc.Add(parent, body));
            if(kind=="余额"||kind=="月变")
                AlignReadOnlyControls(plan,t,"regions.primary_resource_region."+(kind=="余额"?"value_control":"growth_control"),true);
        }
        private static void AddHint(Plan plan,VisualTarget target,string name,string key) {
            if(!Files.IsId(key))throw new InvalidDataException(L.T("请填写悬停说明。"));
            var layout=plan.Read(target.Layout);
            var node=layout.Root.Child(target.LayoutId);
            if(node.Get("hover_control")=="")throw new InvalidDataException(L.T("这个布局缺少接收悬停的控件。请先给布局绑定 hover_control。"));
            var hints=node.Child("tooltips");
            if(hints!=null&&hints.Children.Count>=16)throw new InvalidDataException(L.T("布局的 16 个提示位置已用满。"));
            plan.Text(target.Layout,layout.SetPath(node,new[]{"tooltips",name},0,key));
        }
        private static Node FindContainer(Node node, string name) {
            if (node == null) return null;
            if (node.Get("name") == name) return node;
            foreach (var child in node.Nodes.Where(n => n.Key == "windowType" || n.Key == "containerWindowType")) {
                var found = FindContainer(child, name); if (found != null) return found;
            }
            return null;
        }
    }
}
