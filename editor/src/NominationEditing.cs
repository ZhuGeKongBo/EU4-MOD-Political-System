using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;

namespace PoliticalEditor {
    internal static partial class Editing {
        internal const string NominationScore="regions.special_members_region.nomination_score";
        internal const string NominationHit="regions.special_members_region.nomination_tooltip_control";
        internal const string NominationProblem="nomination tooltip must cover the number beside the shield inside the visible row";
        internal const string ScoreSurfaceProblem="nomination tooltip must cover the score text";
        internal const string OfficePanelBounds="manual office control must fit inside the panel";

        private sealed class NominationSeat {
            internal readonly Node Shield, Score, Hit, Support;
            internal readonly Rectangle FlagBox, ScoreBox, HitBox, SupportBox;
            internal readonly bool Readable, NeedsRepair;

            internal NominationSeat(Node shield,Node score,Node hit,Node support,Rectangle limit,bool automatic) {
                Shield=shield;Score=score;Hit=hit;Support=support;
                Readable=AreaFor(shield,false,out FlagBox)&AreaFor(score,true,out ScoreBox)&
                    AreaFor(hit,hit==score,out HitBox)&(support==null||AreaFor(support,false,out SupportBox));
                // The runtime uses the text rectangle itself if there is no separate hit control.
                NeedsRepair=Readable&&(!limit.Contains(HitBox)||!HitBox.Contains(ScoreBox)||
                    (Support!=null&&!limit.Contains(SupportBox))||
                    (automatic&&((HitBox.Left<FlagBox.Right&&HitBox.Top<FlagBox.Bottom)||
                        (Support!=null&&HitBox.IntersectsWith(SupportBox)))));
            }
        }

        private static string NumberedControl(Node region,string key,int slot) {
            string prefix=region.Get(key);
            return prefix==""?"":prefix+"_"+slot.ToString("D2",CultureInfo.InvariantCulture);
        }

        private static NominationSeat[][] NominationRows(Document gui,Node layout) {
            var region=ReadField(layout,"regions.special_members_region");
            if(region==null||region.Get("nomination_score")=="")return new NominationSeat[0][];
            var root=gui.Root.Child("guiTypes");
            var window=root==null?null:root.Named(layout.Get("window"));
            var rail=window==null?null:window.Named(region.Get("scrollbar"));
            if(rail==null)return new NominationSeat[0][];
            int rows=Math.Max(1,Document.Int(region.Get("rows"),1));
            int columns=Math.Max(1,Document.Int(region.Get("columns"),1));
            if(rows>8||columns>8||rows*columns>8)return new NominationSeat[0][];
            int railX=Geometry.Raw(rail).X;
            var frame=region.Get("auto_place")=="yes"?root.Named(region.Get("frame")):null;
            var frameBox=frame==null?Rectangle.Empty:new Rectangle(Geometry.Raw(frame),Geometry.SizeOf(frame,Size.Empty));
            int gridWidth=railX-frameBox.Left-4;
            var panelLimit=new Rectangle(Point.Empty,Geometry.SizeOf(window,Size.Empty));
            var result=new NominationSeat[rows][];
            for(int row=0;row<rows;++row) {
                result[row]=new NominationSeat[columns];
                for(int col=0;col<columns;++col) {
                    int slot=row*columns+col;
                    var shield=window.Named(NumberedControl(region,"control",slot));
                    var score=window.Named(NumberedControl(region,"nomination_score",slot));
                    var hit=region.Get("nomination_tooltip_control")==""?score:window.Named(NumberedControl(region,"nomination_tooltip_control",slot));
                    var support=region.Get("support_shield")==""?null:window.Named(NumberedControl(region,"support_shield",slot));
                    var limit=panelLimit;
                    Rectangle flag;
                    if(frame!=null&&gridWidth>0&&AreaFor(shield,false,out flag)) {
                        int left=col*gridWidth/columns,top=row*frameBox.Height/rows;
                        int width=(col+1)*gridWidth/columns-left,height=(row+1)*frameBox.Height/rows-top;
                        var roleLabel=window.Named(NumberedControl(region,"label",slot));
                        int groupWidth=flag.Width+(roleLabel==null?0:6+Geometry.SizeOf(roleLabel,Size.Empty).Width);
                        // Compare raw control coordinates with the cell translated back from its automatic offset.
                        limit=new Rectangle(flag.X-(width-groupWidth)/2,flag.Y-(height-flag.Height)/2,width,height);
                    }
                    result[row][col]=new NominationSeat(shield,score,hit,support,limit,frame!=null);
                }
            }
            return result;
        }

        private static string SetNominationPositions(Document gui,IDictionary<Node,Point> moves) {
            string text=gui.Text;
            foreach(var change in moves.Where(entry=>entry.Key!=null&&entry.Key.Child("position")!=null&&Geometry.Raw(entry.Key)!=entry.Value)
                .OrderByDescending(entry=>entry.Key.Start)) {
                var position=change.Key.Child("position");
                text=text.Remove(position.ValueStart,position.End-position.ValueStart)
                    .Insert(position.ValueStart,PairText(change.Value.X,change.Value.Y));
            }
            return text;
        }

        private static IEnumerable<NominationSeat[][]> NominationSources(Model model,Model previous) {
            if(previous!=null&&previous.Type!=null&&previous.Type.Node.Key==model.Type.Node.Key&&previous.Window!=null)
                yield return NominationRows(previous.Window.Doc,previous.Layout.Node);
            foreach(var parent in model.Chain.Skip(1)) {
                var rule=model.Resolve(parent.Node.Key,new List<string>());
                Source layout;
                if(rule==null||!model.Layouts.TryGetValue(rule.Read("","interface","layout"),out layout))continue;
                var gui=model.Doc(layout.Node.Get("gui_file"));
                if(gui!=null)yield return NominationRows(gui,layout.Node);
            }
        }

        internal static int[] RepairNominationRows(Plan plan,Model model,Model previous,VisualTarget target) {
            var layout=plan.Read(target.Layout).Root.Child(target.LayoutId);
            var gui=plan.Read(target.Gui);
            var rows=NominationRows(gui,layout);
            var affected=Enumerable.Range(0,rows.Length).Where(i=>rows[i].Any(seat=>seat.NeedsRepair)).ToArray();
            if(affected.Length==0)return affected;
            var sources=NominationSources(model,previous).ToArray();
            foreach(int row in affected) {
                var moves=new Dictionary<Node,Point>();
                foreach(var seat in rows[row].Where(s=>s.NeedsRepair&&s.Hit!=null&&s.Hit!=s.Score))
                    moves[seat.Hit]=seat.ScoreBox.Location;
                var currentText=SetNominationPositions(gui,moves);
                var candidate=new Document(target.Gui,currentText);
                if(NominationRows(candidate,layout)[row].Any(seat=>seat.NeedsRepair)) {
                    candidate=null;
                    foreach(var source in sources.Where(s=>row<s.Length&&s[row].Length==rows[row].Length&&!s[row].Any(seat=>seat.NeedsRepair))) {
                        moves.Clear();
                        for(int col=0;col<rows[row].Length;++col) {
                            var seat=rows[row][col];var baseline=source[row][col];
                            CopyNominationPosition(moves,seat.Shield,baseline.Shield);
                            CopyNominationPosition(moves,seat.Score,baseline.Score);
                            if(seat.Hit!=seat.Score)CopyNominationPosition(moves,seat.Hit,baseline.Hit);
                            CopyNominationPosition(moves,seat.Support,baseline.Support);
                        }
                        var trial=new Document(target.Gui,SetNominationPositions(gui,moves));
                        if(!NominationRows(trial,layout)[row].Any(seat=>seat.NeedsRepair)){candidate=trial;break;}
                    }
                }
                if(candidate==null)throw new InvalidDataException(String.Format(L.T("第 {0} 排选举盾徽旁没有合适的评分位置。请给数字留出空间，再点「预览改动」。"),row+1));
                gui=candidate;
                rows=NominationRows(gui,layout);
            }
            plan.Text(target.Gui,gui.Text);
            return affected.Select(row=>row+1).ToArray();
        }

        private static void CopyNominationPosition(IDictionary<Node,Point> moves,Node destination,Node source) {
            if(destination!=null&&source!=null)moves[destination]=Geometry.Raw(source);
        }

        internal static bool HasManualOffices(Model model) {
            if(model==null||model.Layout==null||model.Window==null)return false;
            var region=ReadField(model.Layout.Node,OfficeRegion);
            return region!=null&&region.Get("auto_place")!="yes";
        }

        internal static bool CanMoveOfficeSeat(Model model) {
            if(!HasManualOffices(model))return false;
            var region=ReadField(model.Layout.Node,OfficeRegion);
            return region.Get("label")!=""||region.Get("nomination_score")!=""||region.Get("support_shield")!="";
        }

        private static Node[] OfficeParts(Node window,Node layout,ControlBinding binding,string name,bool moveSeat) {
            var region=ReadField(layout,OfficeRegion);
            if(region==null||region.Get("auto_place")=="yes"||binding==null||
                !binding.Path.StartsWith(OfficeRegion+".",StringComparison.Ordinal))return new Node[0];
            int index=Array.IndexOf(binding.Names.ToArray(),name);
            if(index<0)return new Node[0];
            string field=binding.Path.Substring(OfficeRegion.Length+1);
            string[] linked;
            switch(field) {
                case "control":
                    linked=moveSeat?new[]{"label","nomination_score","nomination_tooltip_control","support_shield"}:new string[0];
                    break;
                case "nomination_score": linked=new[]{"nomination_tooltip_control"};break;
                case "nomination_tooltip_control": linked=new[]{"nomination_score"};break;
                case "group_heading": linked=new[]{"group_tooltip_control"};break;
                case "group_tooltip_control": linked=new[]{"group_heading"};break;
                case "support_shield":
                case "label":
                case "scrollbar": linked=new string[0];break;
                default: return new Node[0];
            }
            return new[]{name}.Concat(linked.Select(key=>NumberedControl(region,key,index)))
                .Where(key=>key!="").Select(window.Named).Where(node=>node!=null).Distinct().ToArray();
        }

        private static bool OfficeBounds(Node node,out Rectangle bounds) {
            if(node.Key!="scrollbarType")return AreaFor(node,node.Key=="instantTextBoxType",out bounds);
            bounds=new Rectangle(Geometry.Raw(node),Geometry.SizeOf(node,Size.Empty));
            return bounds.Width>0&&bounds.Height>0;
        }

        private static bool MoveOfficeParts(Plan plan,VisualTarget target,Document gui,Node window,Node control,
                                            ControlBinding binding,Point location,bool moveSeat) {
            var layout=plan.Read(target.Layout).Root.Child(target.LayoutId);
            var parts=OfficeParts(window,layout,binding,control.Get("name"),moveSeat);
            if(parts.Length==0)return false;
            var origin=Geometry.Raw(control);
            var range=OfficeRange(window,parts,origin);
            if(range!=null&&!range.Areas.Any(box=>box.Contains(location)))
                throw new InvalidOperationException(range.Description);
            var shift=new Size(location.X-origin.X,location.Y-origin.Y);
            var moves=parts.ToDictionary(part=>part,part=>Point.Add(Geometry.Raw(part),shift));
            plan.Text(target.Gui,SetNominationPositions(gui,moves));
            return true;
        }
    }
}
