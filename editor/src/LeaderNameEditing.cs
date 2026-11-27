using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;

namespace PoliticalEditor {
    internal enum LeaderNameOperation { Hide, Rebind, Restore }

    internal static partial class Editing {
        private static Node LeaderRegion(Node layout) {
            var regions=layout==null?null:layout.Child("regions");
            return regions==null?null:regions.Child("leader_region");
        }
        internal static string LeaderName(Model model) {
            var region=LeaderRegion(model==null||model.Layout==null?null:model.Layout.Node);
            return region==null?"":region.Get("label");
        }
        internal static string LeaderSurface(Model model) {
            return model==null||model.Layout==null?"":model.Layout.Node.Get("leader_history_control");
        }
        internal static bool HasLeaderArea(Model model) {
            return model!=null&&model.Window!=null&&model.Layout!=null&&LeaderRegion(model.Layout.Node)!=null;
        }
        internal static bool IsLeaderNameControl(Model model,VisualItem item) {
            return model!=null&&item!=null&&item.Key!=null&&item.Key.Length==1&&
                (item.Name==LeaderName(model)||item.Name==LeaderSurface(model));
        }
        private static bool AreaFor(Node control,bool text,out Rectangle area) {
            area=Rectangle.Empty;
            if(control==null||!control.Key.Equals(text?"instantTextBoxType":"guiButtonType",StringComparison.OrdinalIgnoreCase)||
                control.Child("parent")!=null||!control.Get("orientation","UPPER_LEFT").Equals("UPPER_LEFT",StringComparison.OrdinalIgnoreCase))return false;
            var position=control.Child("position");
            var size=text?control:control.Child("size");
            int x,y,w,h;
            if(position==null||size==null||
                !Int32.TryParse(position.Get("x"),NumberStyles.Integer,CultureInfo.InvariantCulture,out x)||
                !Int32.TryParse(position.Get("y"),NumberStyles.Integer,CultureInfo.InvariantCulture,out y)||
                !Int32.TryParse(size.Get(text?"maxWidth":"x"),NumberStyles.Integer,CultureInfo.InvariantCulture,out w)||
                !Int32.TryParse(size.Get(text?"maxHeight":"y"),NumberStyles.Integer,CultureInfo.InvariantCulture,out h)||
                Math.Abs((long)x)>16384||Math.Abs((long)y)>16384||w<1||h<1||w>8192||h>8192)return false;
            area=new Rectangle(x,y,w,h);
            return true;
        }
        internal static void CheckLeaderName(Model model) {
            var layout=model.Layout.Node;
            var region=LeaderRegion(layout);
            if(region==null)return;
            var field=region.Child("label");
            var hover=layout.Child("leader_history_control");
            var name=region.Get("label");
            var tool=layout.Get("leader_history_control");
            var window=model.Window.Node;
            var label=window.Named(name);
            var hit=window.Named(tool);
            Rectangle text, surface;
            if(name!=""&&(label==null||label.Key!="instantTextBoxType"||
                tool!=""&&(!AreaFor(label,true,out text)||label.Get("alwaystransparent")!="yes"))) {
                model.Problems.Add(new Problem(model.Layout.Path,field.Start,
                    L.T("领袖姓名控件缺失或属性不匹配：")+name+L.T("。在「绑定与修复…」中恢复、改绑或移除。"),true,"regions.leader_region.label"));
                return;
            }
            if(tool!=""&&(name==""||!AreaFor(label,true,out text)||!AreaFor(hit,false,out surface)||
                hit.Get("no_clicksound")!="yes"||hit.Get("alwaystransparent")=="yes"||!surface.Contains(text))) {
                model.Problems.Add(new Problem(model.Layout.Path,hover.Start,
                    L.T("领袖姓名的悬停区缺失或位置错开。用「绑定与修复…」恢复并对齐。"),true,"regions.leader_region.label"));
            }
        }
        private static List<Source> PanelLayouts(Model model) {
            string window=model.Window.Node.Get("name");
            return model.Layouts.Values.Where(l=>Files.Windows.Equals(l.Node.Get("gui_file"),model.GuiPath)&&
                l.Node.Get("window")==window).ToList();
        }
        internal static IEnumerable<string> LeaderNameTargets(Model model,bool shared) {
            var related=shared?PanelLayouts(model):new List<Source>{model.Layout};
            foreach(var node in model.Window.Node.Nodes) {
                var name=node.Get("name");
                Rectangle bounds;
                if(!Files.IsId(name)||name==LeaderName(model)||!AreaFor(node,true,out bounds)||
                    IsIconHint(model.Window.Doc,node))continue;
                if(!related.Any(l=>l.Node.Nodes.Any(f=>RefersToControl(f,name))))yield return name;
            }
        }
        internal static Rectangle LeaderNamePosition(Model model,Model previous) {
            Rectangle area;
            string name=LeaderName(model);
            if(AreaFor(model.Window.Node.Named(name),true,out area))return area;
            if(previous!=null&&previous.Type!=null&&previous.Type.Node.Key==model.Type.Node.Key&&
                previous.Window!=null&&previous.Window.Node.Get("name")==model.Window.Node.Get("name")&&
                AreaFor(previous.Window.Node.Named(name),true,out area))return area;
            if(AreaFor(model.Window.Node.Named(LeaderSurface(model)),false,out area))return area;
            var window=model.Window.Node;
            var size=Geometry.SizeOf(window,new Size(500,500));
            var shield=window.Named(LeaderRegion(model.Layout.Node).Get("control"));
            var shieldPos=Geometry.Raw(shield);
            var shieldSize=Geometry.SizeOf(shield,new Size(80,80));
            int width=Math.Min(size.Width,207),height=Math.Min(size.Height,25);
            return new Rectangle(Math.Max(0,Math.Min(shieldPos.X+(shieldSize.Width-width)/2,size.Width-width)),
                Math.Max(0,Math.Min(shieldPos.Y+shieldSize.Height+4,size.Height-height)),width,height);
        }
        private static string PairText(int x,int y) {
            return "{ x = "+x.ToString(CultureInfo.InvariantCulture)+" y = "+y.ToString(CultureInfo.InvariantCulture)+" }";
        }
        private static void SetGuiField(Plan plan,VisualTarget target,string name,string field,string value) {
            var doc=plan.Read(target.Gui);
            var control=doc.Root.Child("guiTypes").Named(target.Window).Named(name);
            plan.Text(target.Gui,doc.Set(control,field,value));
        }
        private static void AlignLeaderSurface(Plan plan,VisualTarget target) {
            var layout=plan.Read(target.Layout).Root.Child(target.LayoutId);
            var surface=layout.Get("leader_history_control");
            if(surface=="")return;
            var gui=plan.Read(target.Gui).Root.Child("guiTypes").Named(target.Window);
            Rectangle area,hit;
            if(!AreaFor(gui.Named(LeaderRegion(layout).Get("label")),true,out area)||
                !AreaFor(gui.Named(surface),false,out hit))
                throw new InvalidDataException(L.T("请先用「绑定与修复…」修复悬停区。"));
            SetGuiField(plan,target,surface,"position",PairText(area.X,area.Y));
            SetGuiField(plan,target,surface,"size",PairText(area.Width,area.Height));
        }
        private static void UpdateLeaderLayout(Plan plan,string path,string layoutId,string old,string name,string hover) {
            var doc=plan.Read(path);
            var layout=doc.Root.Child(layoutId);
            var region=LeaderRegion(layout);
            var label=region.Child("label");
            if(name!=""||label!=null) {
                plan.Text(path,name!=""?doc.Set(region,"label",name):WithoutNode(doc,label));
                doc=plan.Read(path);layout=doc.Root.Child(layoutId);
            }
            var hit=layout.Child("leader_history_control");
            if(hover!=""||hit!=null) {
                plan.Text(path,hover!=""?doc.Set(layout,"leader_history_control",hover):WithoutNode(doc,hit));
                doc=plan.Read(path);layout=doc.Root.Child(layoutId);
            }
            var hints=layout.Child("tooltips");
            var hint=hints==null||old==""||old==name?null:hints.Child(old);
            if(hint==null)return;
            bool referenced=ControlBindings.Read(layout).Any(b=>!b.Hint&&b.Names.Contains(old));
            if(name!=""&&hints.Child(name)==null)
                plan.Text(path,referenced?doc.Set(hints,name,hint.Value):doc.Replace(hint.Start,hint.KeyEnd,name));
            else if(!referenced)plan.Text(path,WithoutNode(doc,hint));
        }
        private static string AllocateGuiName(Node window,string stem) {
            var name=stem;
            for(int i=2;window.Named(name)!=null;++i)name=stem+"_"+i;
            return name;
        }
        internal static Plan EditLeaderName(Project project,Model model,Model previous,LeaderNameOperation operation,string selectedName,
            Point? placement,bool shared) {
            if(!HasLeaderArea(model))throw new InvalidOperationException(L.T("当前布局没有领袖区域。"));
            string oldName=LeaderName(model), oldSurface=LeaderSurface(model);
            var plan=new Plan(project,model.Mount,operation==LeaderNameOperation.Hide?L.T("移除领袖姓名"):
                operation==LeaderNameOperation.Rebind?L.T("改绑领袖姓名"):L.T("恢复领袖姓名"));
            var target=EnsureVisual(plan,model,shared);
            var doc=plan.Read(target.Gui);
            var window=doc.Root.Child("guiTypes").Named(target.Window);
            var layouts=shared?PanelLayouts(model).Select(l=>Tuple.Create(l.Path,l.Node.Key)).ToArray():
                new[]{Tuple.Create(target.Layout,target.LayoutId)};
            // In a shared window, change layouts that use this same name field and its hover surface.
            var affected=layouts.Where(l=>{
                var node=plan.Read(l.Item1).Root.Child(l.Item2);
                var leader=LeaderRegion(node);
                return leader!=null&&leader.Get("label")==oldName&&node.Get("leader_history_control")==oldSurface;
            }).ToArray();
            string name="", surface="";
            if(operation!=LeaderNameOperation.Hide) {
                var area=LeaderNamePosition(model,previous);
                if(placement.HasValue)area.Location=placement.Value;
                if(operation==LeaderNameOperation.Rebind&&selectedName!="") {
                    if(!LeaderNameTargets(model,shared).Contains(selectedName))
                        throw new InvalidDataException(L.T("所选文字已被占用或无法绑定。请选另一段文字，或新建位置。"));
                    name=selectedName;
                } else {
                    name=operation==LeaderNameOperation.Restore&&Files.IsId(oldName)?oldName:
                        AllocateGuiName(window,operation==LeaderNameOperation.Restore?"leader_name":"leader_name_rebound");
                }
                var existing=window.Named(name);
                Rectangle existingArea;
                if(!AreaFor(existing,true,out existingArea)) {
                    if(existing!=null&&existing.Key!="instantTextBoxType")
                        throw new InvalidDataException(L.T("这个名称已被其它类型的控件使用。请选择改绑。"));
                    Node lastLabel=null;
                    Document lastDoc=null;
                    Rectangle lastArea;
                    if(operation==LeaderNameOperation.Restore&&previous!=null&&previous.Window!=null&&previous.Type!=null&&
                        previous.Type.Node.Key==model.Type.Node.Key&&previous.Window.Node.Get("name")==window.Get("name")) {
                        lastDoc=previous.Window.Doc;
                        lastLabel=previous.Window.Node.Named(name);
                        if(!AreaFor(lastLabel,true,out lastArea))lastLabel=null;
                    }
                    string block=lastLabel!=null?lastDoc.At(lastLabel):
                        "instantTextBoxType = {\n    name = "+Document.Quote(name)+"\n    position = "+PairText(area.X,area.Y)+
                        "\n    font = \"vic_22\"\n    format = centre\n    textureFile = \"\"\n    borderSize = { x = 0 y = 0 }\n    text = \"\""+
                        "\n    maxWidth = "+area.Width+"\n    maxHeight = "+area.Height+"\n    alwaystransparent = yes\n}";
                    string gui=existing==null?doc.Add(window,block):doc.Replace(existing.Start,existing.End,block);
                    plan.Text(target.Gui,gui);
                }
                SetGuiField(plan,target,name,"alwaystransparent","yes");
                doc=plan.Read(target.Gui);window=doc.Root.Child("guiTypes").Named(target.Window);
                // Avoid resizing a surface another function or another surviving layout uses.
                var hit=window.Named(oldSurface);
                var otherUse=oldSurface!=""&&layouts.Any(l=>{
                    var layout=plan.Read(l.Item1).Root.Child(l.Item2);
                    bool changing=affected.Contains(l);
                    return layout.Nodes.Any(f=>(!changing||f.Key!="leader_history_control")&&RefersToControl(f,oldSurface));
                });
                surface=Files.IsId(oldSurface)&&!otherUse&&(hit==null||hit.Key=="guiButtonType")?oldSurface:AllocateGuiName(window,"leader_history_hover");
                var existingHit=window.Named(surface);
                Rectangle hitArea;
                if(!AreaFor(existingHit,false,out hitArea)) {
                    string sprite="GFX_pf075_transparent_hit";
                    if(model.Images==null||!model.Images.Find(sprite).Any(s=>s.SourceRoot!="未找到"))
                        throw new InvalidDataException(L.T("当前布局缺少透明提示素材 GFX_pf075_transparent_hit。请先恢复框架原有素材登记。"));
                    if(!AreaFor(window.Named(name),true,out area))
                        throw new InvalidDataException(L.T("所选姓名控件缺少有效位置或尺寸。"));
                    string block="guiButtonType = {\n    name = "+Document.Quote(surface)+"\n    position = "+PairText(area.X,area.Y)+
                        "\n    size = "+PairText(area.Width,area.Height)+"\n    quadTextureSprite = "+Document.Quote(sprite)+
                        "\n    buttonFont = \"vic_18\"\n    tooltip = \"\"\n    tooltipText = \"\"\n    no_clicksound = yes\n}";
                    plan.Text(target.Gui,existingHit==null?doc.Add(window,block):doc.Replace(existingHit.Start,existingHit.End,block));
                }
                SetGuiField(plan,target,surface,"no_clicksound","yes");
                SetGuiField(plan,target,surface,"alwaystransparent","no");
            }
            foreach(var layout in affected)UpdateLeaderLayout(plan,layout.Item1,layout.Item2,oldName,name,surface);
            if(operation!=LeaderNameOperation.Hide)AlignLeaderSurface(plan,target);
            foreach(var unused in new[]{oldName,oldSurface}.Distinct().Where(n=>n!=""&&n!=name&&n!=surface)) {
                doc=plan.Read(target.Gui);window=doc.Root.Child("guiTypes").Named(target.Window);
                var control=window.Named(unused);
                if(control==null||window.Nodes.Any(n=>n!=control&&RefersToControl(n,unused)))continue;
                bool used=layouts.Any(l=>{
                    var node=plan.Read(l.Item1).Root.Child(l.Item2);
                    var hints=node.Child("tooltips");
                    return node.Nodes.Any(r=>RefersToControl(r,unused))||hints!=null&&hints.Child(unused)!=null;
                });
                if(!used)plan.Text(target.Gui,WithoutNode(doc,control));
            }
            return plan;
        }
        internal static IEnumerable<string> LayoutReferences(Model model,string name) {
            return DescribeLayoutReferences(model.Layout.Node,name,"");
        }
        private static IEnumerable<string> DescribeLayoutReferences(Node root,string name,string prefix) {
            foreach(var field in root.Nodes) {
                if(!RefersToControl(field,name))continue;
                var path=prefix+field.Key;
                if(field.Block) {
                    foreach(var part in DescribeLayoutReferences(field,name,path+"."))yield return part;
                } else yield return path+" = "+field.Value;
            }
        }
    }
}
