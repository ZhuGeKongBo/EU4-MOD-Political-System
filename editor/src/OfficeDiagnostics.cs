using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;

namespace PoliticalEditor {
    internal static partial class Editing {
        internal const string OfficeHeadingBounds="office group heading must sit above its row and beside the vertical rail";
        internal const string OfficeShieldOverlap="office shield overlaps its group heading";

        private sealed class OfficeRowGeometry {
            internal int Number, MinimumY, MaximumY, RailX, PanelHeight;
            internal Node Label, Hover, Rail, Above, FirstShield;
            internal Rectangle Hit;
        }

        private static OfficeRowGeometry OfficeRow(Document gui,Node layout,int targetRow) {
            var region=ReadField(layout,"regions.special_members_region");
            if(region==null||region.Get("group_heading")=="")return null;
            var root=gui.Root.Child("guiTypes");
            var window=root==null?null:root.Named(layout.Get("window"));
            var scroll=window==null?null:window.Named(region.Get("scrollbar"));
            int rows=Document.Int(region.Get("rows")),columns=Document.Int(region.Get("columns"),1);
            if(scroll==null||rows<1||rows>8||columns<1||columns>8||targetRow<0||targetRow>=rows)return null;
            var rail=Geometry.Raw(scroll);
            var above=scroll;
            int previousBottom=rail.Y;
            var frame=region.Get("auto_place")=="yes"?root.Named(region.Get("frame")):null;
            var frameAt=Geometry.Raw(frame);
            var frameSize=Geometry.SizeOf(frame,Size.Empty);
            int gridWidth=rail.X-frameAt.X-4;
            for(int row=0;row<=targetRow;++row) {
                var label=window.Named(NumberedControl(region,"group_heading",row));
                var hover=window.Named(NumberedControl(region,"group_tooltip_control",row));
                Rectangle hit;
                if(label==null||!AreaFor(hover,false,out hit))return null;
                int panelHeight=Geometry.SizeOf(window,Size.Empty).Height;
                var geometry=new OfficeRowGeometry { Number=row+1,MinimumY=previousBottom,MaximumY=panelHeight-hit.Height,
                    RailX=rail.X,PanelHeight=panelHeight,Label=label,Hover=hover,Rail=scroll,Above=above,Hit=hit };
                previousBottom=hit.Bottom;above=hover;
                for(int col=0;col<columns;++col) {
                    var shield=window.Named(NumberedControl(region,"control",row*columns+col));
                    Rectangle box;
                    if(!AreaFor(shield,false,out box))continue;
                    if(frame!=null&&gridWidth>0) {
                        int top=row*frameSize.Height/rows,height=(row+1)*frameSize.Height/rows-top;
                        box.Y=frameAt.Y+top+(height-box.Height)/2;
                    }
                    if(box.Top-hit.Height<geometry.MaximumY) {
                        geometry.MaximumY=box.Top-hit.Height;geometry.FirstShield=shield;
                    }
                    if(box.Bottom>previousBottom){previousBottom=box.Bottom;above=shield;}
                }
                if(row==targetRow)return geometry;
            }
            return null;
        }

        private static string DescribeOfficeTitle(OfficeRowGeometry row) {
            var reasons=new List<string>();
            string hit=row.Hover.Get("name");
            if(row.Hit.Top<row.MinimumY) {
                string rule=row.Number==1?L.T("第 {0} 排标题悬停区 {1} 从 Y={2} 开始，滑条 {3} 从 Y={4} 开始。标题高出了 {5} 像素。"):
                    L.T("第 {0} 排标题悬停区 {1} 从 Y={2} 开始，上方的 {3} 到 Y={4} 才结束。两者重叠了 {5} 像素。");
                reasons.Add(String.Format(rule,row.Number,hit,row.Hit.Top,row.Above.Get("name"),row.MinimumY,row.MinimumY-row.Hit.Top));
            }
            if(row.Hit.Left<0)reasons.Add(String.Format(L.T("{0} 的左端 X={1} 超出面板左边界 X=0。"),hit,row.Hit.Left));
            if(row.Hit.Right>row.RailX)
                reasons.Add(String.Format(L.T("{0} 的右端 X={1}，滑条 {2} 的左端 X={3}。悬停区越过滑条 {4} 像素。"),
                    hit,row.Hit.Right,row.Rail.Get("name"),row.RailX,row.Hit.Right-row.RailX));
            if(row.Hit.Bottom>row.PanelHeight)
                reasons.Add(String.Format(L.T("{0} 的下端 Y={1} 超出面板下边界 Y={2}。"),hit,row.Hit.Bottom,row.PanelHeight));
            if(row.FirstShield!=null&&row.Hit.Top>row.MaximumY)
                reasons.Add(String.Format(L.T("{0} 的下端 Y={1}，同排盾徽 {2} 从 Y={3} 开始。标题和盾徽重叠。"),
                    hit,row.Hit.Bottom,row.FirstShield.Get("name"),row.MaximumY+row.Hit.Height));
            if(reasons.Count==0)return null;
            string guidance=row.MinimumY<=row.MaximumY&&row.Hit.Width<=row.RailX?
                String.Format(L.T("把 {0} 和 {1} 一起移到 X 0–{2}、Y {3}–{4}。可在「绑定与修复…」点「恢复控件…」。"),
                    row.Label.Get("name"),hit,row.RailX-row.Hit.Width,row.MinimumY,row.MaximumY):
                L.T("这排留给标题的空间不足。点「排布选举区…」为标题、盾徽和滑条一起腾出空间。");
            return String.Join("\r\n",reasons)+"\r\n"+guidance;
        }

        internal static string DescribePosition(Document gui,Node layout,string control,string reason,ControlBinding binding) {
            if(binding==null||layout==null)return null;
            int index=Array.IndexOf(binding.Names.ToArray(),control);
            if(reason==OfficePanelBounds) {
                var window=gui.Root.Child("guiTypes").Named(layout.Get("window"));
                Rectangle box;
                if(window==null||window.Named(control)==null||!OfficeBounds(window.Named(control),out box))return null;
                var panel=Geometry.SizeOf(window,Size.Empty);
                return String.Format(L.T("{0} 位于 ({1}, {2})，大小 {3} × {4}，面板大小 {5} × {6}。请选择该控件并移回面板内，或调整尺寸。"),
                    control,box.X,box.Y,box.Width,box.Height,panel.Width,panel.Height);
            }
            if(reason==OfficeHeadingBounds||reason==OfficeShieldOverlap) {
                var region=ReadField(layout,"regions.special_members_region");
                int columns=region==null?1:Math.Max(1,Document.Int(region.Get("columns"),1));
                var row=OfficeRow(gui,layout,reason==OfficeHeadingBounds?index:index/columns);
                return row==null?null:DescribeOfficeTitle(row);
            }
            if(reason==NominationProblem&&index>=0) {
                var rows=NominationRows(gui,layout);
                if(rows.Length==0||index>=rows.Length*rows[0].Length)return null;
                var seat=rows[index/rows[0].Length][index%rows[0].Length];
                if(!seat.Readable)return null;
                if(seat.HitBox.Left<seat.FlagBox.Right&&seat.HitBox.Top<seat.FlagBox.Bottom)
                    return String.Format(L.T("评分区域 {0} 从 ({1}, {2}) 开始，盾徽 {3} 的右端 X={4}、下端 Y={5}。评分应移到盾徽右侧或下方。"),
                        seat.Hit.Get("name"),seat.HitBox.Left,seat.HitBox.Top,seat.Shield.Get("name"),seat.FlagBox.Right,seat.FlagBox.Bottom)+"\r\n"+
                        L.T("用「定位并修复…」选中候选人评分，再点「恢复控件…」。");
            }
            if(reason=="text must fit inside its read-only tooltip surface"||reason==ScoreSurfaceProblem) {
                var root=gui.Root.Child("guiTypes");
                var window=root==null?null:root.Named(layout.Get("window"));
                if(window==null)return null;
                string hit="";
                if(binding.Path==ControlBindings.LeaderLabel)hit=layout.Get("leader_history_control");
                else if(binding.Path==NominationScore&&index>=0)
                    hit=NumberedControl(ReadField(layout,OfficeRegion),"nomination_tooltip_control",index);
                else if(binding.Path=="regions.special_members_region.group_heading"&&index>=0)
                    hit=NumberedControl(ReadField(layout,"regions.special_members_region"),"group_tooltip_control",index);
                else if(binding.Path.StartsWith("regions.primary_resource_region.",StringComparison.Ordinal))
                    hit=ReadField(layout,"regions.primary_resource_region").Get("tooltip_control");
                Rectangle text,area;
                if(hit!=""&&AreaFor(window.Named(control),true,out text)&&AreaFor(window.Named(hit),false,out area))
                    return String.Format(L.T("文字 {0} 的范围是 X {1}–{2}、Y {3}–{4}，悬停区 {5} 的范围是 X {6}–{7}、Y {8}–{9}。悬停区要覆盖整块文字。可用「恢复控件…」对齐。"),
                        control,text.Left,text.Right,text.Top,text.Bottom,hit,area.Left,area.Right,area.Top,area.Bottom);
            }
            return null;
        }

        internal static string OfficePositionScope(Document gui,Node layout,string reason) {
            if(reason!=OfficeHeadingBounds&&reason!=OfficeShieldOverlap&&reason!=NominationProblem)return null;
            var root=gui.Root.Child("guiTypes");
            var window=root==null?null:root.Named(layout.Get("window"));
            if(window==null)return null;
            var bindings=ControlBindings.Read(layout).Where(b=>b.Path.StartsWith("regions.special_members_region.",StringComparison.Ordinal)&&
                !b.Path.EndsWith(".caption",StringComparison.Ordinal)).ToArray();
            var parts=bindings.SelectMany(b=>b.Names).Select(n=>window.Named(n)??root.Named(n)).Where(n=>n!=null)
                .Select(n=>gui.At(n));
            return Files.Hash(Files.Utf8.GetBytes(String.Join("\n",parts)+"\n"+Geometry.SizeOf(window,Size.Empty)));
        }

        private static void RestoreOfficeTitlePositions(Plan plan,VisualTarget target) {
            var layout=plan.Read(target.Layout).Root.Child(target.LayoutId);
            var region=ReadField(layout,"regions.special_members_region");
            int rows=region==null?0:Document.Int(region.Get("rows"));
            for(int i=0;i<Math.Min(8,rows);++i) {
                var gui=plan.Read(target.Gui);
                if(region.Get("auto_place")!="yes") {
                    var window=gui.Root.Child("guiTypes").Named(target.Window);
                    var label=window.Named(NumberedControl(region,"group_heading",i));
                    var hit=window.Named(NumberedControl(region,"group_tooltip_control",i));
                    if(label==null||hit==null)continue;
                    var origin=Geometry.Raw(label);
                    var parts=new[]{label,hit};
                    var range=OfficeRange(window,parts,origin);
                    Point placed;
                    if(range==null)continue;
                    if(!range.TryNearest(origin,out placed))throw new InvalidDataException(range.Description);
                    if(origin!=placed) {
                        var delta=new Size(placed.X-origin.X,placed.Y-origin.Y);
                        plan.Text(target.Gui,SetNominationPositions(gui,parts.ToDictionary(node=>node,node=>Point.Add(Geometry.Raw(node),delta))));
                    }
                    continue;
                }
                var row=OfficeRow(gui,layout,i);
                if(row==null||DescribeOfficeTitle(row)==null)continue;
                if(row.MinimumY>row.MaximumY||row.Hit.Width>row.RailX)
                    throw new InvalidDataException(DescribeOfficeTitle(row));
                var position=new Point(Math.Max(0,Math.Min(row.Hit.X,row.RailX-row.Hit.Width)),
                    Math.Max(row.MinimumY,Math.Min(row.Hit.Y,row.MaximumY)));
                var moves=new Dictionary<Node,Point> { {row.Label,position},{row.Hover,position} };
                plan.Text(target.Gui,SetNominationPositions(gui,moves));
            }
        }
    }
}
