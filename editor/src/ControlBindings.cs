using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace PoliticalEditor {
    internal sealed class ControlBinding {
        internal readonly string Path, Name, Title, Kind, Removal;
        internal readonly Node Location;
        internal readonly int Count;
        internal readonly bool IsGroup, Frame, Hint;
        internal ControlBinding(string path, string name, string title, string kind, Node location, int count,
            string removal, bool frame=false, bool hint=false) {
            Path=path; Name=name; Title=title; Kind=kind; Location=location;
            Count=Math.Max(1,count); IsGroup=count>0; Removal=removal; Frame=frame; Hint=hint;
        }
        internal string ControlName(string name,int index) { return IsGroup?name+"_"+index.ToString("D2",CultureInfo.InvariantCulture):name; }
        internal IEnumerable<string> Names { get { return Name==""?Enumerable.Empty<string>():Enumerable.Range(0,Count).Select(i=>ControlName(Name,i)); } }
        internal bool Accepts(Node node) {
            return node!=null&&(node.Key.Equals(Kind,StringComparison.OrdinalIgnoreCase)||
                Path=="background_control"&&node.Key.Equals("guiButtonType",StringComparison.OrdinalIgnoreCase));
        }
        internal Node Container(Model model) { return Frame?model.Window.Doc.Root.Child("guiTypes"):model.Window.Node; }
        internal IEnumerable<string> Missing(Model model) {
            var container=Container(model);
            return Names.Where(n=>!Accepts(container.Named(n)));
        }
    }

    internal static class ControlBindings {
        internal const string LeaderLabel="regions.leader_region.label", LeaderHistory="leader_history_control";
        internal static IEnumerable<ControlBinding> Read(Model model) {
            return model==null||model.Layout==null||model.Window==null?Enumerable.Empty<ControlBinding>():Read(model.Layout.Node);
        }
        internal static IEnumerable<ControlBinding> Read(Node layout) {
            var bindings=new List<ControlBinding>();
            Action<Node,string,string,string,string,int,string> add=(source,prefix,key,title,kind,count,removal)=>{
                var n=source.Child(key);
                if(n!=null&&!n.Block&&n.Value!="")
                    bindings.Add(new ControlBinding(prefix+key,n.Value,title,kind,n,count,removal,key=="frame"));
            };
            const string button="guiButtonType",text="instantTextBoxType",scroll="scrollbarType",window="windowType";
            add(layout,"","background_control",L.T("面板背景"),"iconType",0,"");
            add(layout,"","hover_control",L.T("面板悬停区"),button,0,"");
            add(layout,"","close_control",L.T("关闭按钮"),button,0,"");
            add(layout,"","title_control",L.T("面板标题"),text,0,"field");
            add(layout,"","instance_control",L.T("实例切换按钮"),button,0,"field");
            add(layout,"","vote_control",L.T("投票按钮"),button,0,"field");
            add(layout,"",LeaderHistory,L.T("领袖姓名悬停区"),button,0,"leader");
            var regions=layout.Child("regions");
            if(regions!=null)foreach(var region in regions.Nodes.Where(n=>n.Block)) {
                string path="regions."+region.Key+".";
                int rows=Math.Max(1, Math.Min(32,Document.Int(region.Get("rows"),1)));
                int columns=Math.Max(1, Math.Min(32,Document.Int(region.Get("columns"),1)));
                int seats=Math.Min(32,rows*columns);
                switch(region.Key) {
                    case "leader_region":
                        add(region,path,"control",L.T("领袖盾徽"),button,0,"");
                        add(region,path,"caption",L.T("领袖区域标题"),text,0,"field");
                        add(region,path,"label",L.T("领袖姓名"),text,0,"leader");
                        if(region.Child("label")==null)
                            bindings.Add(new ControlBinding(LeaderLabel,"",L.T("领袖姓名"),text,region,0,"leader"));
                        break;
                    case "members_region":
                        add(region,path,"control",L.T("成员盾徽"),button,seats,"");
                        add(region,path,"caption",L.T("成员区域标题"),text,0,"");
                        add(region,path,"scrollbar",L.T("成员列表滑条"),scroll,0,"");
                        break;
                    case "special_members_region":
                        add(region,path,"control",L.T("职位盾徽"),button,seats,"");
                        add(region,path,"caption",L.T("职位区域标题"),text,0,"");
                        add(region,path,"scrollbar",L.T("职位列表滑条"),scroll,0,"");
                        add(region,path,"label",L.T("职位标签"),text,seats,"");
                        add(region,path,"group_heading",L.T("职位分组标题"),text,rows,"");
                        add(region,path,"group_tooltip_control",L.T("职位分组悬停区"),button,rows,"");
                        add(region,path,"nomination_score",L.T("候选人评分"),text,seats,"nomination");
                        add(region,path,"nomination_tooltip_control",L.T("候选人评分悬停区"),button,seats,"nomination");
                        add(region,path,"support_shield",L.T("支持目标盾徽"),button,seats,"field");
                        break;
                    case "primary_resource_region":
                        add(region,path,"value_control",L.T("资源余额"),text,0,"primary");
                        add(region,path,"growth_control",L.T("资源月变化"),text,0,"primary");
                        add(region,path,"tooltip_control",L.T("资源摘要悬停区"),button,0,"primary");
                        break;
                    case "system_resources_region":
                        add(region,path,"control",L.T("资源列表文字"),text,region.Get("kind")=="slot"?0:rows,"");
                        add(region,path,"scrollbar",L.T("资源列表滑条"),scroll,0,"");
                        add(region,path,"value_control",L.T("资源列表数值"),text,rows,"");
                        add(region,path,"tooltip_control",L.T("资源列表悬停区"),button,rows,"");
                        break;
                    case "actions_region":
                        add(region,path,"control",L.T("行动按钮"),button,rows,"");
                        add(region,path,"label",L.T("行动名称"),text,rows,"");
                        add(region,path,"scrollbar",L.T("行动列表滑条"),scroll,0,"");
                        add(region,path,"read_only_control",L.T("行动的只读按钮"),button,rows,"");
                        add(region,path,"seal_control",L.T("行动的可执行印章"),button,rows,"");
                        add(region,path,"check_control",L.T("行动的完成勾号"),button,rows,"");
                        add(region,path,"status_control",L.T("行动状态文字"),text,rows,"field");
                        break;
                }
                if(region.Get("auto_place")=="yes")add(region,path,"frame",L.T("自动排列区域框")+" · "+region.Key,window,0,"manual");
            }
            var hints=layout.Child("tooltips");
            if(hints!=null)foreach(var field in hints.Nodes.Where(n=>n.Value!=null))
                bindings.Add(new ControlBinding("tooltips."+field.Key,field.Key,L.T("文字提示")+" · "+field.Key,text,field,0,"field",hint:true));
            return bindings;
        }
        internal static ControlBinding ForItem(Model model,VisualItem item) {
            if(item==null)return null;
            return Read(model).Where(b=>b.Names.Contains(item.Name)&&
                    (b.Frame?item.RegionFrame!=null:item.Key!=null&&item.Key.Length==1))
                .OrderBy(b=>b.Hint).FirstOrDefault();
        }
        internal static void Check(Model model) {
            Editing.CheckLeaderName(model);
            foreach(var binding in Read(model)) {
                if(binding.Path==LeaderLabel||binding.Path==LeaderHistory||binding.Hint&&binding.Name==Editing.LeaderName(model))continue;
                var missing=binding.Missing(model).FirstOrDefault();
                if(missing!=null)
                    model.Problems.Add(new Problem(model.Layout.Path,binding.Location.Start,binding.Title+" · "+
                        L.T("找不到控件或类型不匹配 ")+missing+L.T("。双击可打开绑定修复。"),true,binding.Path));
            }
            Editing.CheckReadOnlySurfaces(model);
        }
    }

    internal static partial class Editing {
        private static Tuple<string,string>[] BindingScopes(Model model,VisualTarget target,bool shared) {
            return shared?PanelLayouts(model).Select(l=>Tuple.Create(l.Path,l.Node.Key)).ToArray():
                new[]{Tuple.Create(target.Layout,target.LayoutId)};
        }
        private static Node ReadField(Node root,string path) {
            foreach(var name in path.Split('.')) { if(root==null)return null; root=root.Child(name); }
            return root;
        }
        private static void RemoveField(Plan plan,string file,string layoutId,string path) {
            var doc=plan.Read(file);
            var field=ReadField(doc.Root.Child(layoutId),path);
            if(field!=null)plan.Text(file,WithoutNode(doc,field));
        }
        private static Node BindingContainer(Document doc,VisualTarget target,bool frame=false) {
            var root=doc.Root.Child("guiTypes");
            return root==null?null:frame?root:root.Named(target.Window);
        }
        private static void RemoveUnusedControls(Plan plan,VisualTarget target,Tuple<string,string>[] layouts,IEnumerable<string> names) {
            foreach(var name in names.Distinct()) {
                var gui=plan.Read(target.Gui);
                var window=BindingContainer(gui,target);
                var item=window.Named(name);
                if(item==null||window.Nodes.Any(n=>n!=item&&RefersToControl(n,name)))continue;
                if(layouts.Any(l=>ControlBindings.Read(plan.Read(l.Item1).Root.Child(l.Item2)).Any(b=>b.Names.Contains(name))))continue;
                plan.Text(target.Gui,WithoutNode(gui,item));
            }
        }
        internal static Plan RemoveBinding(Project project,Model model,Model previous,Scene scene,ControlBinding binding,bool shared) {
            if(binding.Removal=="leader")return EditLeaderName(project,model,previous,LeaderNameOperation.Hide,"",null,shared);
            if(binding.Removal=="")throw new InvalidOperationException(L.T("这个区域需要该绑定。请选择更换控件，或恢复缺失的控件。"));
            if(binding.Removal=="manual") {
                string region=binding.Path.Split('.')[1];
                if(scene==null||!ReferenceEquals(scene.Model,model)||binding.Missing(model).Any())
                    throw new InvalidOperationException(L.T("先恢复自动排列区域框，再将位置转成手动。"));
                var manual=UnpackFrame(project,model,scene,region,shared);
                var definition=manual.Read(model.Type.Path).Root.Child(model.Type.Node.Key);
                var presentation=definition.Child("interface");
                var current=presentation==null?null:presentation.Get("layout");
                string id=String.IsNullOrEmpty(current)?model.Layout.Node.Key:current;
                string name=ReadField(manual.Read(model.Layout.Path).Root.Child(id),binding.Path).Value;
                RemoveField(manual,model.Layout.Path,id,binding.Path);
                var targetGui=manual.Read(model.GuiPath);
                var root=targetGui.Root.Child("guiTypes");
                var frame=root.Named(name);
                if(frame!=null&&!model.Layouts.Values.Any(l=>l.Node.Key!=id&&
                    ControlBindings.Read(l.Node).Any(b=>b.Frame&&b.Name==name)))manual.Text(model.GuiPath,WithoutNode(targetGui,frame));
                return manual;
            }
            var plan=new Plan(project,model.Mount,L.T("移除显示")+" · "+binding.Title);
            var target=EnsureVisual(plan,model,shared);
            var layouts=BindingScopes(model,target,shared);
            var retired=new List<string>();
            foreach(var entry in layouts) {
                var layout=plan.Read(entry.Item1).Root.Child(entry.Item2);
                var active=ControlBindings.Read(layout).FirstOrDefault(b=>b.Path==binding.Path&&b.Name==binding.Name);
                if(active==null)continue;
                var group=active.Removal=="primary"||active.Removal=="nomination"
                    ?ControlBindings.Read(layout).Where(b=>b.Removal==active.Removal).ToArray():new[]{active};
                foreach(var part in group) {
                    retired.AddRange(part.Names);
                    if(active.Removal!="primary")RemoveField(plan,entry.Item1,entry.Item2,part.Path);
                }
                if(active.Removal=="primary")RemoveField(plan,entry.Item1,entry.Item2,"regions.primary_resource_region");
                var used=ControlBindings.Read(plan.Read(entry.Item1).Root.Child(entry.Item2)).Where(b=>!b.Hint).SelectMany(b=>b.Names).ToArray();
                foreach(var name in group.SelectMany(b=>b.Names).Where(n=>!used.Contains(n)).Distinct())
                    RemoveField(plan,entry.Item1,entry.Item2,"tooltips."+name);
            }
            RemoveUnusedControls(plan,target,layouts,retired);
            return plan;
        }
        internal static IEnumerable<string> ReplacementControls(Model model,ControlBinding binding,bool shared) {
            if(binding.Removal=="leader")return LeaderNameTargets(model,shared);
            if(binding.Path=="background_control"||binding.Frame)return Enumerable.Empty<string>();
            var container=binding.Container(model);
            var taken=(shared?PanelLayouts(model):new List<Source>{model.Layout})
                      .SelectMany(l=>ControlBindings.Read(l.Node)).SelectMany(b=>b.Names).ToArray();
            return container.Nodes.Where(binding.Accepts).Select(n=>n.Get("name"))
                .Where(n=>Files.IsId(n)&&(!binding.IsGroup||n.EndsWith("_00",StringComparison.Ordinal)))
                .Select(n=>binding.IsGroup?n.Substring(0,n.Length-3):n).Distinct()
                .Where(n=>Enumerable.Range(0,binding.Count).All(i=>{
                    string name=binding.ControlName(n,i);
                    return !taken.Contains(name)&&binding.Accepts(container.Named(name))&&!IsIconHint(model.Window.Doc,container.Named(name));
                }));
        }
        internal static string ReplacementName(Model model,ControlBinding binding) {
            var stem=binding.Name.Length>60?binding.Name.Substring(0,60):binding.Name;
            string name=stem+"_rebound";
            var container=binding.Container(model);
            for(int i=2;Enumerable.Range(0,binding.Count).Any(n=>container.Named(binding.ControlName(name,n))!=null);++i)name=stem+"_rebound_"+i;
            return name;
        }
        internal static Plan RebindControl(Project project,Model model,Model previous,ControlBinding binding,string value,bool clone,bool shared) {
            if(binding.Removal=="leader")return EditLeaderName(project,model,previous,LeaderNameOperation.Rebind,clone?"":value,null,shared);
            if(!Files.IsId(value)||value==binding.Name||binding.ControlName(value,binding.Count-1).Length>79)
                throw new InvalidDataException(L.T("请输入新的控件名称。只能用英文字母、数字和下划线，并以字母或下划线开头。"));
            if(!clone&&!ReplacementControls(model,binding,shared).Contains(value))
                throw new InvalidDataException(L.T("所选控件已被占用或与此绑定不兼容。"));
            if(clone&&binding.Missing(model).Any())
                throw new InvalidOperationException(L.T("原控件有缺失。请先恢复它，或选择已有的兼容控件。"));
            var plan=new Plan(project,model.Mount,L.T("更换绑定")+" · "+binding.Title);
            var target=EnsureVisual(plan,model,shared);
            var scopes=BindingScopes(model,target,shared);
            foreach(var entry in scopes) {
                var layout=plan.Read(entry.Item1).Root.Child(entry.Item2);
                var active=ControlBindings.Read(layout).FirstOrDefault(b=>b.Path==binding.Path&&b.Name==binding.Name);
                if(active==null)continue;
                if(!active.Hint)plan.Set(entry.Item1,entry.Item2,binding.Path.Split('.'),value);
                for(int i=0;i<active.Count;++i) {
                    var oldName=active.ControlName(active.Name,i);
                    var newName=active.ControlName(value,i);
                    var doc=plan.Read(entry.Item1);
                    var root=doc.Root.Child(entry.Item2);
                    var hints=root.Child("tooltips");
                    var hint=hints==null?null:hints.Child(oldName);
                    if(hint==null)continue;
                    bool used=ControlBindings.Read(root).Any(b=>!b.Hint&&b.Names.Contains(oldName));
                    var already=hints.Child(newName);
                    if(used&&already==null)plan.Text(entry.Item1,doc.Set(hints,newName,hint.Value));
                    else if(!used)plan.Text(entry.Item1,already==null?doc.Replace(hint.Start,hint.KeyEnd,newName):WithoutNode(doc,hint));
                }
            }
            if(clone) {
                for(int i=0;i<binding.Count;++i) {
                    string oldName=binding.ControlName(binding.Name,i),newName=binding.ControlName(value,i);
                    var gui=plan.Read(target.Gui);
                    var container=BindingContainer(gui,target,binding.Frame);
                    if(container.Named(newName)!=null)throw new InvalidDataException(L.T("这个名称已被占用 ")+newName);
                    var original=container.Named(oldName);
                    if(scopes.Any(l=>ControlBindings.Read(plan.Read(l.Item1).Root.Child(l.Item2)).Any(b=>b.Names.Contains(oldName)))||
                        !binding.Frame&&container.Nodes.Any(n=>n!=original&&n.Key!="backGround"&&RefersToControl(n,oldName)))
                        throw new InvalidOperationException(L.T("另一个绑定也在使用原控件。请选择另一处可用控件。"));
                    plan.Text(target.Gui,gui.Set(original,"name",Document.Quote(newName)));
                    if(binding.Path=="background_control"&&container.Get("backGround")==oldName) {
                        gui=plan.Read(target.Gui);
                        plan.Text(target.Gui,gui.Set(BindingContainer(gui,target),"backGround",Document.Quote(newName)));
                    }
                }
            } else RemoveUnusedControls(plan,target,scopes,binding.Names);
            AlignReadOnlyControls(plan,target,binding.Path,true);
            return plan;
        }
        private static IEnumerable<Tuple<Document,Node,ControlBinding>> RestoreSources(Project project,Model model,Model previous,ControlBinding binding) {
            Func<Document,string,Node> pick=(doc,name)=> {
                var root=doc==null?null:doc.Root.Child("guiTypes");
                return root==null?null:binding.Frame?root:root.Named(name);
            };
            string window=model.Window.Node.Get("name");
            if(previous!=null&&previous.Window!=null&&previous.GuiPath==model.GuiPath&&previous.Window.Node.Get("name")==window) {
                var doc=previous.Window.Doc;
                yield return Tuple.Create(doc,pick(doc,window),binding);
            }
            Entry entry;
            if(project.Content.TryGetValue(model.GuiPath,out entry)&&entry.Original!=null) {
                var doc=new Document(model.GuiPath,TextFile.Decode(entry.Original).Text);
                yield return Tuple.Create(doc,pick(doc,window),binding);
            }
            foreach(var source in model.Chain.Skip(1)) {
                var rule=model.Resolve(source.Node.Key,new List<string>());
                if(rule==null)continue;
                Source layout;
                if(!model.Layouts.TryGetValue(rule.Read("","interface","layout"),out layout))continue;
                var bound=ControlBindings.Read(layout.Node).FirstOrDefault(b=>b.Path==binding.Path&&b.Kind==binding.Kind&&b.IsGroup==binding.IsGroup&&b.Name!="");
                if(bound==null)continue;
                var doc=model.Doc(layout.Node.Get("gui_file"));
                yield return Tuple.Create(doc,pick(doc,layout.Node.Get("window")),bound);
            }
        }
        private static string RestoreBlock(Document target,Node container,Document source,Node sourceContainer,Node original,string name,bool background) {
            string block=original.Get("name")==name?source.At(original):RenameGui(source.At(original),name);
            var anchor=sourceContainer.Nodes.SkipWhile(n=>n!=original).Skip(1).Select(n=>n.Get("name"))
                .Where(n=>n!="").Select(container.Named).FirstOrDefault(n=>n!=null);
            if(anchor==null&&background)anchor=container.Nodes.FirstOrDefault(n=>n.Get("name")!="");
            if(anchor==null)return target.Add(container,block);
            string indent=target.Indent(anchor.Start);
            return target.Replace(anchor.Start,anchor.Start,Document.ViewText(block).Replace("\n",target.Newline)+target.Newline+indent);
        }
        internal static Plan RestoreControl(Project project,Model model,Model previous,ControlBinding binding,bool shared) {
            if(binding.Removal=="leader")return EditLeaderName(project,model,previous,LeaderNameOperation.Restore,"",null,shared);
            var plan=new Plan(project,model.Mount,L.T("恢复控件")+" · "+binding.Title);
            var target=EnsureVisual(plan,model,shared);
            var sources=RestoreSources(project,model,previous,binding).Where(s=>s.Item2!=null).ToArray();
            for(int i=0;i<binding.Count;i++) {
                string name=binding.ControlName(binding.Name,i);
                var gui=plan.Read(target.Gui);
                var container=BindingContainer(gui,target,binding.Frame);
                var existing=container.Named(name);
                if(binding.Accepts(existing))continue;
                if(existing!=null)
                    throw new InvalidDataException(L.T("同名控件已存在但类型不同。请选择更换绑定，或先重命名这个控件。"));
                var found=sources.Select(s=>Tuple.Create(s.Item1,s.Item2,
                    i<s.Item3.Count?s.Item2.Named(s.Item3.ControlName(s.Item3.Name,i)):null)).FirstOrDefault(s=>binding.Accepts(s.Item3));
                if(found==null)
                    throw new InvalidDataException(L.T("找不到可恢复的原控件 ")+name+L.T("。请换绑到一个兼容控件，或打开含有原控件的工程副本。"));
                plan.Text(target.Gui,RestoreBlock(gui,container,found.Item1,found.Item2,found.Item3,name,binding.Path=="background_control"));
            }
            if(binding.Path==NominationScore||binding.Path==NominationHit) {
                RepairNominationRows(plan,model,previous,target);
                return plan;
            }
            AlignReadOnlyControls(plan,target,binding.Path,true);
            if(binding.Path=="regions.special_members_region.group_heading"||binding.Path=="regions.special_members_region.group_tooltip_control")
                RestoreOfficeTitlePositions(plan,target);
            return plan;
        }
        internal static bool CanAlign(string path) {
            return path.StartsWith("regions.primary_resource_region.",StringComparison.Ordinal)||
                path=="regions.special_members_region.group_heading"||path=="regions.special_members_region.group_tooltip_control"||
                path=="regions.actions_region.control"||path=="regions.actions_region.read_only_control";
        }
        internal static void AlignReadOnlyControls(Plan plan,VisualTarget target,string path,bool reconcileProperties) {
            if(!CanAlign(path))return;
            var layout=plan.Read(target.Layout).Root.Child(target.LayoutId);
            var window=BindingContainer(plan.Read(target.Gui),target);
            if(path.StartsWith("regions.primary_resource_region.",StringComparison.Ordinal)) {
                var region=ReadField(layout,"regions.primary_resource_region");
                string hit=region.Get("tooltip_control");
                var texts=new[]{region.Get("value_control"),region.Get("growth_control")};
                var boxes=new List<System.Drawing.Rectangle>();
                foreach(var name in texts) {
                    System.Drawing.Rectangle area;
                    if(!AreaFor(window.Named(name),true,out area))continue;
                    if(reconcileProperties)SetGuiField(plan,target,name,"alwaystransparent",hit==""?"no":"yes");
                    boxes.Add(area);
                }
                if(hit!=""&&boxes.Count==texts.Length&&window.Named(hit)!=null) {
                    var combined=System.Drawing.Rectangle.Union(boxes[0],boxes[1]);
                    WriteReadOnlySurface(plan,target,hit,combined);
                }
                return;
            }
            bool offices=path.StartsWith("regions.special_members_region.",StringComparison.Ordinal);
            string prefix=offices?"regions.special_members_region.":"regions.actions_region.";
            var all=ControlBindings.Read(layout).ToArray();
            var text=all.FirstOrDefault(b=>b.Path==prefix+(offices?"group_heading":"control"));
            var hitGroup=all.FirstOrDefault(b=>b.Path==prefix+(offices?"group_tooltip_control":"read_only_control"));
            if(text==null||hitGroup==null)return;
            for(int i=0;i<Math.Min(text.Count,hitGroup.Count);++i) {
                string caption=text.ControlName(text.Name,i),hit=hitGroup.ControlName(hitGroup.Name,i);
                if(window.Named(hit)==null||window.Named(caption)==null)continue;
                if(offices) {
                    System.Drawing.Rectangle area;
                    if(!AreaFor(window.Named(caption),true,out area))continue;
                    if(reconcileProperties)SetGuiField(plan,target,caption,"alwaystransparent","yes");
                    WriteReadOnlySurface(plan,target,hit,area);
                } else {
                    var p=Geometry.Raw(window.Named(caption));
                    SetGuiField(plan,target,hit,"position",PairText(p.X,p.Y));
                }
            }
        }
        private static void WriteReadOnlySurface(Plan plan,VisualTarget target,string control,System.Drawing.Rectangle area) {
            SetGuiField(plan,target,control,"position",PairText(area.X,area.Y));
            SetGuiField(plan,target,control,"size",PairText(area.Width,area.Height));
            SetGuiField(plan,target,control,"no_clicksound","yes");
            SetGuiField(plan,target,control,"alwaystransparent","no");
        }
        internal static void CheckReadOnlySurfaces(Model model) {
            var bindings=ControlBindings.Read(model).ToArray();
            var window=model.Window.Node;
            foreach(var slot in bindings.Where(b=>b.Path=="regions.primary_resource_region.value_control"||
                       b.Path=="regions.primary_resource_region.growth_control"||b.Path=="regions.special_members_region.group_heading")) {
                string companion=slot.IsGroup?"regions.special_members_region.group_tooltip_control":"regions.primary_resource_region.tooltip_control";
                var hit=bindings.FirstOrDefault(b=>b.Path==companion);
                for(int i=0;i<slot.Count;++i) {
                    var name=slot.ControlName(slot.Name,i);
                    var caption=window.Named(name);
                    var surface=hit==null?null:window.Named(hit.ControlName(hit.Name,i));
                    if(caption==null||hit!=null&&surface==null)continue;
                    System.Drawing.Rectangle text,area;
                    bool separate=hit!=null;
                    if(!AreaFor(caption,true,out text)||(caption.Get("alwaystransparent")=="yes")!=separate||
                        separate&&(!AreaFor(surface,false,out area)||!area.Contains(text)||
                        surface.Get("no_clicksound")!="yes"||surface.Get("alwaystransparent")=="yes")) {
                        model.Problems.Add(new Problem(model.Layout.Path,slot.Location.Start,name+" · "+
                            L.T("文字需要与只读悬停区对齐。打开「绑定与修复…」，选「恢复控件…」自动对齐。"),true,slot.Path));
                        break;
                    }
                }
            }
        }
    }
}
