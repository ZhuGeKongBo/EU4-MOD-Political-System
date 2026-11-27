using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;

namespace PoliticalEditor {
    internal sealed class PlacementRange {
        internal readonly Rectangle[] Areas;
        internal readonly string[] MovingControls;
        internal PlacementRange(Rectangle area,IEnumerable<string> movingControls) {
            Areas=area.Width>0&&area.Height>0?new[]{area}:new Rectangle[0];
            MovingControls=movingControls.ToArray();
        }

        internal bool TryNearest(Point attempted,out Point position) {
            position=attempted;
            if(Areas.Length==0)return false;
            var area=Areas[0];
            position=new Point(Math.Max(area.Left,Math.Min(area.Right-1,attempted.X)),
                Math.Max(area.Top,Math.Min(area.Bottom-1,attempted.Y)));
            return true;
        }

        internal string Summary {
            get {
                if(Areas.Length==0)return L.T("控件超出面板尺寸");
                var border=Areas[0];
                return String.Format("X {0}–{1}  /  Y {2}–{3}",
                    border.Left,border.Right-1,border.Top,border.Bottom-1);
            }
        }

        internal string Description { get { return Areas.Length==0?L.T("移动范围放不进面板。请缩小控件或加大面板。"):
            L.T("手动排布可叠放和跨排移动。绿线内是选中控件左上角的可放位置。")+" "+Summary; } }
    }

    internal sealed class ElectionArea {
        internal readonly Rectangle Current;
        internal readonly Size Minimum, Panel;
        internal ElectionArea(Rectangle current,Size minimum,Size panel) { Current=current;Minimum=minimum;Panel=panel; }
    }

    internal static partial class Editing {
        private const string OfficeRegion="regions.special_members_region";

        private static PlacementRange OfficeRange(Node window,Node[] parts,Point anchor) {
            if(parts.Length==0)return null;
            var panel=Geometry.SizeOf(window,Size.Empty);
            var boxes=new List<Rectangle>();
            foreach(var part in parts) {
                Rectangle box;
                if(!OfficeBounds(part,out box))return null;
                boxes.Add(box);
            }
            var union=boxes.Aggregate(Rectangle.Union);
            var area=Rectangle.FromLTRB(anchor.X-union.Left,anchor.Y-union.Top,
                panel.Width+1+anchor.X-union.Right,panel.Height+1+anchor.Y-union.Bottom);
            return new PlacementRange(area,parts.Select(node=>node.Get("name")));
        }

        internal static PlacementRange ElectionPlacement(Model model,VisualItem item,bool moveSeat=false) {
            if(item==null||item.Locked||model==null||model.Window==null)return null;
            var window=model.Window.Node;
            return OfficeRange(window,OfficeParts(window,model.Layout.Node,ControlBindings.ForItem(model,item),item.Name,moveSeat),item.Raw);
        }

        private sealed class ElectionGrid {
            internal readonly NominationSeat[][] Rows;
            internal readonly Node Region,Window,Rail;
            internal readonly int[] ColumnWidth,RowHeight,HeadingHeight;
            internal readonly ElectionArea Area;
            internal ElectionGrid(Document gui,Node layout) {
                Region=ReadField(layout,OfficeRegion);
                Window=gui.Root.Child("guiTypes").Named(layout.Get("window"));
                Rail=Window.Named(Region.Get("scrollbar"));
                Rows=NominationRows(gui,layout);
                if(Rows.Length==0||Rows.SelectMany(row=>row).Any(seat=>!seat.Readable))
                    throw new InvalidDataException(L.T("有选举控件缺少位置或尺寸。请先在「绑定与修复…」恢复缺少的控件。"));
                ColumnWidth=new int[Rows[0].Length];RowHeight=new int[Rows.Length];HeadingHeight=new int[Rows.Length];
                for(int r=0;r<Rows.Length;r++) {
                    Rectangle title;
                    if(AreaFor(Window.Named(NumberedControl(Region,"group_heading",r)),true,out title))HeadingHeight[r]=title.Height;
                    for(int c=0;c<ColumnWidth.Length;c++) {
                        var seat=Rows[r][c];
                        var label=LabelAt(r,c);
                        Point score=ScoreOffset(seat);
                        ColumnWidth[c]=Math.Max(ColumnWidth[c],Math.Max(seat.FlagBox.Width,score.X+seat.ScoreBox.Width)+
                            (label==null?0:6+Geometry.SizeOf(label,Size.Empty).Width));
                        int body=Math.Max(seat.FlagBox.Height,score.Y+seat.ScoreBox.Height);
                        if(seat.Support!=null)body=Math.Max(body,BadgeOffset(seat).Y+seat.SupportBox.Height);
                        if(label!=null)body=Math.Max(body,Geometry.SizeOf(label,Size.Empty).Height);
                        RowHeight[r]=Math.Max(RowHeight[r],HeadingHeight[r]+body);
                    }
                }
                var railAt=Geometry.Raw(Rail);
                var railSize=Geometry.SizeOf(Rail,Size.Empty);
                int left=Rows.SelectMany(row=>row).Min(seat=>seat.FlagBox.Left);
                Area=new ElectionArea(new Rectangle(left,railAt.Y,railAt.X+railSize.Width-left,railSize.Height),
                    new Size(ColumnWidth.Sum()+railSize.Width+4,RowHeight.Sum()),Geometry.SizeOf(Window,Size.Empty));
            }
            internal Node LabelAt(int row,int col) { return Window.Named(NumberedControl(Region,"label",row*ColumnWidth.Length+col)); }
            internal static Point BadgeOffset(NominationSeat seat) {
                return new Point(Math.Min(5,Math.Max(0,seat.FlagBox.Width-seat.SupportBox.Width)),Math.Max(0,seat.FlagBox.Height-16));
            }
            internal static Point ScoreOffset(NominationSeat seat) {
                // Relative position of hre_elector.relation on the vanilla 92px shield.
                return new Point((seat.FlagBox.Width*50+46)/92,(seat.FlagBox.Height*75+46)/92);
            }
        }

        internal static bool HasElectionArea(Model model) {
            if(model==null||model.Layout==null||model.Window==null)return false;
            var offices=ReadField(model.Layout.Node,OfficeRegion);
            return offices!=null&&offices.Get("auto_place")!="yes"&&offices.Get("nomination_score")!="";
        }

        internal static ElectionArea ReadElectionArea(Model model) {
            var area=new ElectionGrid(model.Window.Doc,model.Layout.Node).Area;
            if(area.Minimum.Width>area.Panel.Width||area.Minimum.Height>area.Panel.Height)
                throw new InvalidDataException(L.T("选举控件的最小宽高超过主面板。请在 GUI 中增大面板，或减少每排的列数。"));
            return area;
        }

        internal static Plan CompactElectionScores(Project project,Model model,bool shared) {
            var plan=new Plan(project,model.Mount,L.T("评分贴右下…"));
            var target=EnsureVisual(plan,model,shared);
            var gui=plan.Read(target.Gui);
            var layout=plan.Read(target.Layout).Root.Child(target.LayoutId);
            var grid=new ElectionGrid(gui,layout);
            var moves=new Dictionary<Node,Point>();
            foreach(var seat in grid.Rows.SelectMany(row=>row)) {
                var p=Point.Add(seat.FlagBox.Location,new Size(ElectionGrid.ScoreOffset(seat)));
                var delta=new Size(p.X-seat.ScoreBox.X,p.Y-seat.ScoreBox.Y);
                moves[seat.Hit]=Point.Add(seat.HitBox.Location,delta);
                moves[seat.Score]=p;
            }
            plan.Text(target.Gui,SetNominationPositions(gui,moves));
            foreach(var seat in grid.Rows.SelectMany(row=>row))
                SetGuiField(plan,target,seat.Score.Get("name"),"format","left");
            return plan;
        }

        internal static Plan ArrangeElection(Project project,Model model,Rectangle bounds,bool shared) {
            var plan=new Plan(project,model.Mount,L.T("排布选举区"));
            var target=EnsureVisual(plan,model,shared);
            var gui=plan.Read(target.Gui);
            var grid=new ElectionGrid(gui,plan.Read(target.Layout).Root.Child(target.LayoutId));
            var area=grid.Area;
            if(bounds.Width<area.Minimum.Width||bounds.Height<area.Minimum.Height||!new Rectangle(Point.Empty,area.Panel).Contains(bounds))
                throw new InvalidDataException(L.T("范围超出面板，或不足以容纳原来的盾徽和评分。请核对最小宽高后重试。"));
            var moves=new Dictionary<Node,Point>();
            int railWidth=Geometry.SizeOf(grid.Rail,Size.Empty).Width;
            int usableWidth=bounds.Width-railWidth-4;
            int extraX=bounds.Width-area.Minimum.Width,extraY=bounds.Height-area.Minimum.Height;
            int columns=grid.ColumnWidth.Length,rows=grid.Rows.Length,y=bounds.Top;
            for(int r=0;r<rows;r++) {
                int rowTop=y+extraY*r/rows;
                int spare=extraY*(r+1)/rows-extraY*r/rows;
                var heading=grid.Window.Named(NumberedControl(grid.Region,"group_heading",r));
                var headerHit=grid.Window.Named(NumberedControl(grid.Region,"group_tooltip_control",r));
                if(heading!=null)moves[heading]=new Point(bounds.Left,rowTop);
                if(headerHit!=null)moves[headerHit]=new Point(bounds.Left,rowTop);
                int x=bounds.Left;
                for(int c=0;c<columns;c++) {
                    var seat=grid.Rows[r][c];
                    var flagAt=new Point(x+extraX*(c+1)/(columns+1),rowTop+grid.HeadingHeight[r]+spare/2);
                    var scoreAt=Point.Add(flagAt,new Size(ElectionGrid.ScoreOffset(seat)));
                    moves[seat.Shield]=flagAt;moves[seat.Score]=scoreAt;moves[seat.Hit]=scoreAt;
                    if(seat.Support!=null)moves[seat.Support]=Point.Add(flagAt,new Size(ElectionGrid.BadgeOffset(seat)));
                    var label=grid.LabelAt(r,c);
                    if(label!=null)moves[label]=new Point(Math.Max(scoreAt.X+seat.ScoreBox.Width,flagAt.X+seat.FlagBox.Width)+6,flagAt.Y);
                    x+=grid.ColumnWidth[c];
                }
                y+=grid.RowHeight[r];
            }
            moves[grid.Rail]=new Point(bounds.Right-railWidth,bounds.Top);
            plan.Text(target.Gui,SetNominationPositions(gui,moves));
            SetGuiField(plan,target,grid.Rail.Get("name"),"size",PairText(railWidth,bounds.Height));
            for(int r=0;r<rows;r++) {
                string title=NumberedControl(grid.Region,"group_heading",r),hit=NumberedControl(grid.Region,"group_tooltip_control",r);
                if(grid.Window.Named(title)!=null)SetGuiField(plan,target,title,"maxWidth",usableWidth.ToString(CultureInfo.InvariantCulture));
                if(grid.Window.Named(hit)!=null)SetGuiField(plan,target,hit,"size",PairText(usableWidth,grid.HeadingHeight[r]));
                foreach(var seat in grid.Rows[r]) {
                    SetGuiField(plan,target,seat.Score.Get("name"),"format","left");
                    if(seat.Hit!=seat.Score)
                        SetGuiField(plan,target,seat.Hit.Get("name"),"size",PairText(seat.ScoreBox.Width,seat.ScoreBox.Height));
                }
            }
            return plan;
        }
    }
}
