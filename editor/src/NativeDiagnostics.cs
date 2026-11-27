using System;
using System.Linq;

namespace PoliticalEditor {
    internal static class NativeDiagnostics {
        internal static Problem Warning(Model model,string path,int row) {
            Document doc=model.Doc(path);
            int offset=LineOffset(doc,row);
            string line=String.Format(L.T("{0} 校验器第 {1} 行"),NativeValidation.Version,row);
            string message=L.Current==L.English
                ? "EU4 reads this script in-game. Check the game's error log and test its conditions or effects in a campaign."
                : "此处使用 EU4 原版脚本。进游戏后查看错误日志，并在战役中验证条件或效果。";
            return new Problem(path,offset,line+"  "+message,false);
        }
        private static int LineOffset(Document doc,int row) {
            int offset=0;
            if(doc!=null)for(int i=1;i<row&&offset<doc.Text.Length;++i) {
                int next=doc.Text.IndexOf('\n',offset);
                offset=next<0?doc.Text.Length:next+1;
            }
            return offset;
        }
        internal static Problem Read(Model model,string path,int row,string raw) {
            Document doc;
            model.Docs.TryGetValue(path,out doc);
            if(doc==null&&path!="")doc=model.Doc(path);
            int offset=LineOffset(doc,row);
            int colon=raw.IndexOf(": ",StringComparison.Ordinal);
            string reason=colon<0?raw:raw.Substring(0,colon);
            string context=colon<0?"":raw.Substring(colon+2);
            Source layout=null;
            ControlBinding binding=null;
            string control="";
            if(doc!=null&&path.EndsWith(".gui",StringComparison.OrdinalIgnoreCase)) {
                var root=doc.Root.Child("guiTypes");
                int end=doc.Text.IndexOf('\n',offset);
                if(end<0)end=doc.Text.Length;
                var window=root==null?null:root.Nodes.FirstOrDefault(n=>n.Key=="windowType"&&n.Start<=end&&n.End>=offset);
                if(window!=null) {
                    layout=model.Layouts.Values.Where(s=>Files.Windows.Equals(s.Node.Get("gui_file"),path)&&s.Node.Get("window")==window.Get("name"))
                        .OrderByDescending(s=>model.Layout!=null&&model.Layout.Node.Key==s.Node.Key).FirstOrDefault();
                    const string missing="missing or incompatible GUI control ";
                    if(reason.StartsWith(missing,StringComparison.Ordinal))control=reason.Substring(missing.Length);
                    else {
                        var child=window.Nodes.FirstOrDefault(n=>n.Block&&n.Start<=end&&n.End>=offset&&n.Get("name")!="");
                        if(child!=null)control=child.Get("name");
                    }
                }
                if(layout!=null&&control!="")
                    binding=ControlBindings.Read(layout.Node).Where(b=>b.Names.Contains(control)).OrderBy(b=>b.Hint).FirstOrDefault();
            }
            if(reason=="missing or invalid interface.layout"&&doc!=null) {
                int end=doc.Text.IndexOf('\n',offset);
                if(end<0)end=doc.Text.Length;
                var type=model.Types.Values.FirstOrDefault(s=>Files.Windows.Equals(s.Path,path)&&s.Node.Start<=end&&s.Node.End>=offset);
                if(type!=null) {
                    var rule=model.Resolve(type.Node.Key,new System.Collections.Generic.List<string>());
                    if(rule!=null)model.Layouts.TryGetValue(rule.Read("","interface","layout"),out layout);
                }
            }
            string position=layout==null||doc==null?null:Editing.DescribePosition(doc,layout.Node,control,reason,binding);
            string scope=position==null?null:Editing.OfficePositionScope(doc,layout.Node,reason);
            string guidance=position??Explain(reason,L.Current==L.English,context,layout,binding);
            string reference=control==""?"":" · "+control;
            if(layout!=null)reference+=" · "+layout.Node.Key;
            string text=String.Format(L.T("{0} 校验器第 {1} 行"),NativeValidation.Version,row)+reference+"  "+guidance;
            return new Problem(path,offset,text,true,binding==null?null:binding.Path,layout==null?null:layout.Node.Key,raw,control,position,scope);
        }
        private static string Explain(string reason,bool english,string context,Source layout,ControlBinding binding) {
            switch(reason) {
                case "text must fit inside its read-only tooltip surface":
                case Editing.ScoreSurfaceProblem:
                    if(binding!=null&&Editing.CanAlign(binding.Path)||binding!=null&&binding.Path==ControlBindings.LeaderLabel)
                        return L.T("文字移出了对应悬停区。打开「绑定与修复…」，选中这项文字，再点「恢复控件…」让悬停区自动对齐。");
                    return L.T("文字移出了对应悬停区。用右侧代码定位文字和配套悬停区，让悬停区的 position 与 size 覆盖整块文字，再重新预检。");
                case "missing or invalid interface.layout":
                    return layout==null?L.T("体系引用的布局找不到。打开「类型」页，检查 interface.layout 是否填写了「布局」页中已有的布局 ID。"):
                        L.T("体系引用的布局校验失败。先处理同一布局的 GUI 或布局错误，再重新预检。");
                case "layout is missing a control required by an enabled module":
                    return L.T("启用的体系功能缺少所需布局。检查布局里的领袖、成员、资源、关闭按钮和行动区域，并恢复仍在使用的区域。");
                case "HUD region or button sprite could not be bound":
                    return L.T("无法绑定小地图旁的入口。检查类型的 entry_button.region 和 icon，再用「添加组件 → 入口按钮」选用该区域可用的素材。");
                case "unknown field":
                    return L.T("不认识字段 ")+context+L.T("。检查当前区块允许的字段和拼写。");
                case "duplicate field":
                    return L.T("同一块里重复填写了字段 ")+context+L.T("。把两处设置合并到一处。");
                case "expected a block":
                    return L.T("这里需要花括号包起来的区块。检查当前字段的 { } 是否齐全。")+" "+context;
                case "expected an interface file path and the correct extension":
                    return L.T("需填写 interface 目录下、扩展名正确的文件。检查该字段所需的 .gui 或 .gfx 路径。")+" "+context;
                case "hover_control needs a separate native button that receives pointer input":
                case "tooltips requires a separate hover_control":
                    return L.T("面板的整体悬停区需要独立的原生按钮。用「绑定与修复…」检查面板悬停区，将 hover_control 指向能接收鼠标的 guiButtonType。");
                case "action states need read-only, seal and check controls together":
                    return L.T("行动状态需要同时绑定只读按钮、印章和完成勾号。恢复 actions_region 中缺少的相关字段。");
                case "auto-placement needs a vertical scrollbar on the right inside the frame":
                    return L.T("列表滑条应位于自动排列框的右侧，并完整留在框内。移动区域框或滑条后重新预检。");
                case "shield frame must fit inside the panel":
                    return L.T("盾徽排列框超出面板。将框移回面板内，或缩小它的 size。");
                case "shield and its label must fit in one frame cell":
                    return L.T("盾徽和文字放不进同一格。放大排列框、减少行列，或缩小对应组件。");
                case Editing.NominationProblem:
                    return L.T("这一排的选举评分与盾徽位置冲突，或评分超出了列表。用「定位并修复…」选中候选人评分，再点「恢复控件…」。");
                case Editing.OfficePanelBounds:
                    return L.T("手动职位控件超出了面板。把它移回画布绿线内，或调整控件与面板的尺寸。");
                case "support shield must fit inside its visible list area":
                case "list extends beyond the vertical rail":
                case "resource row must fit beside its vertical scrollbar":
                    return L.T("列表组件超出了可见区域。把对应编号控件移回行内，并放在竖向滑条左侧。");
                case Editing.OfficeHeadingBounds:
                case Editing.OfficeShieldOverlap:
                    return L.T("职位标题与盾徽位置冲突。把分组标题和它的悬停区放到该行盾徽上方、滑条左侧。");
                case "office shield extends below the panel":
                    return L.T("职位盾徽超出了面板下边界。请把它移回面板内。");
            }
            if(reason.StartsWith("missing or incompatible GUI control ",StringComparison.Ordinal))
                return L.T("控件缺失，或类型、尺寸、位置不符合布局绑定。打开「绑定与修复…」，检查对应项并恢复或改绑。")+" "+
                    reason.Substring("missing or incompatible GUI control ".Length);
            if(reason.StartsWith("expected a GUI name for ",StringComparison.Ordinal))
                return L.T("这里需要填写实际存在的 GUI 控件名。检查布局字段 ")+reason.Substring("expected a GUI name for ".Length);
            if(english)return (context==""?reason:reason+": "+context)+". Check this field in the code pane, or open the binding inspector and run preflight again.";
            return L.T("框架无法读取此处配置。跳到标出的行，检查该区块的字段与控件引用；可展开原始校验信息核对具体条件。")+
                (context==""?"":" "+context);
        }
    }
}
