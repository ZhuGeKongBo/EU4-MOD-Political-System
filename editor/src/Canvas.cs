using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace PoliticalEditor {
    internal sealed class PreviewCanvas : Control {
        internal Scene Scene;
        internal VisualItem Selected;
        internal PlacementRange AllowedPlacement;
        internal bool PanelVisible = true;
        internal string Subwindow = "";
        internal int SubwindowOffset;
        internal decimal Zoom { get; set; }
        internal event Action<VisualItem> SelectItem;
        internal event Action<VisualItem, Point> MoveItem;
        internal event Action<VisualItem> ActivateItem;
        internal event Action<string, int> ScrollList;
        internal event Action<int> PickInstance;
        internal event Action<string> Tell;
        private readonly Dictionary<string, Raster> textures = new Dictionary<string,Raster>(Files.Windows);
        private readonly Dictionary<string, Bitmap> shields = new Dictionary<string,Bitmap>(StringComparer.Ordinal);
        private readonly Dictionary<int,Font> fonts = new Dictionary<int,Font>();
        private readonly ToolTip tips = new ToolTip { ShowAlways=true, InitialDelay=225, ReshowDelay=40, AutoPopDelay=14000 };
        private Sprites imageIndex;
        private RectangleF lastWindow, entryBounds;
        private RectangleF chooserBounds, chooserRail;
        private Point press, previewOffset;
        private VisualItem dragged, hitAtPress;
        private ListViewState scrolling;
        private string lastTip = "";
        private float scale=1;
        internal PreviewCanvas() {
            Zoom=1m;
            SetStyle(ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.UserPaint|ControlStyles.ResizeRedraw|ControlStyles.Selectable,true);
            BackColor=Color.FromArgb(29,42,52); TabStop=true; AccessibleName=L.T("政治面板画布");
        }
        internal void Use(Scene scene, string selected) {
            var sprites=scene==null||scene.Model==null?null:scene.Model.Images;
            if (!ReferenceEquals(sprites,imageIndex)) { ClearTextures(); imageIndex=sprites; }
            Scene=scene; Selected=scene==null?null:scene.Items.FirstOrDefault(i=>i.Name==selected);
            AllowedPlacement=scene!=null&&!scene.Settings.Playing?Editing.ElectionPlacement(scene.Model,Selected):null;
            tips.Hide(this); lastTip=""; Invalidate();
        }
        private void ClearTextures() {
            foreach (var item in textures.Values) if (item!=null) item.Dispose();
            textures.Clear();
            foreach(var image in shields.Values)image.Dispose();
            shields.Clear();
        }
        protected override void Dispose(bool disposing) {
            if (disposing) { tips.Dispose(); ClearTextures(); foreach (var font in fonts.Values) font.Dispose(); }
            base.Dispose(disposing);
        }
        private Font Typeface(int size) {
            Font font;
            if (!fonts.TryGetValue(size,out font)) fonts[size]=font=new Font("Georgia",size,FontStyle.Regular,GraphicsUnit.Pixel);
            return font;
        }
        private Raster Texture(string path) {
            if (imageIndex==null || String.IsNullOrWhiteSpace(path)) return null;
            Raster texture;
            if (!textures.TryGetValue(path,out texture)) {
                try {
                    var data=imageIndex.Mount.Get(path,true);
                    texture=data==null?null:Raster.Decode(data.Data,Path.GetExtension(path));
                } catch (Exception e) {
                    if (!(e is IOException || e is InvalidDataException || e is ArgumentException || e is ExternalException || e is UnauthorizedAccessException)) throw;
                    texture=null;
                }
                if (textures.Count>160) ClearTextures();
                textures[path]=texture;
            }
            return texture;
        }
        private Sprite Info(string id) { return imageIndex==null?null:imageIndex.Find(id).FirstOrDefault(); }
        private bool Sprite(Graphics g,string id,Rectangle rect,int frame) {
            var item=Info(id); var texture=item==null?null:Texture(item.Texture);
            if (texture==null) return false;
            int width=Math.Max(1,texture.Width/Math.Max(1,item.Frames));
            int x=Math.Min(item.Frames-1,Math.Max(0,frame))*width;
            if (x+width>texture.Width) x=0;
            g.DrawImage(texture.Bitmap,rect,new Rectangle(x,0,width,texture.Height),GraphicsUnit.Pixel);
            return true;
        }
        protected override void OnPaint(PaintEventArgs e) {
            var g=e.Graphics; g.SmoothingMode=SmoothingMode.AntiAlias; g.InterpolationMode=InterpolationMode.HighQualityBicubic;
            using (var brush=new LinearGradientBrush(ClientRectangle.IsEmpty?new Rectangle(0,0,1,1):ClientRectangle,Color.FromArgb(48,63,71),Color.FromArgb(22,37,48),58f))
                g.FillRectangle(brush,ClientRectangle);
            if (Scene==null||Scene.Model.Window==null||Scene.Panel.Width<=0) {
                TextRenderer.DrawText(g,L.T("在右侧检查原生窗口绑定"),Font,ClientRectangle,Color.FromArgb(184,196,207),TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);
                return;
            }
            int padding=Scene.Settings.Playing?64:56;
            scale=Math.Max(.12f,Math.Min((Width-padding)/(float)Scene.Panel.Width,(Height-padding)/(float)Scene.Panel.Height)*(float)Zoom*(float)Scene.Settings.Scale);
            float w=Scene.Panel.Width*scale,h=Scene.Panel.Height*scale;
            // Preserve the native window anchor and expose screen bounds independently of the editor zoom.
            float dx=(Scene.OnScreen.X+Scene.Panel.Width/2f-Scene.Settings.Resolution.Width/(float)Scene.Settings.Scale/2f)*scale;
            float dy=(Scene.OnScreen.Y+Scene.Panel.Height/2f-Scene.Settings.Resolution.Height/(float)Scene.Settings.Scale/2f)*scale;
            lastWindow=new RectangleF((Width-w)*.5f+dx,(Height-h)*.5f+dy,w,h);
            entryBounds=new RectangleF(Width-58,Height-61,38,38);
            chooserBounds=chooserRail=RectangleF.Empty;
            if (PanelVisible) {
                var saved=g.Save();
                g.TranslateTransform(lastWindow.X,lastWindow.Y);
                g.ScaleTransform(scale,scale);
                var background=Scene.ByName(Scene.BackgroundControl);
                if(background==null||Info(background.Sprite)==null)
                    using(var backdrop=new SolidBrush(Color.FromArgb(197,13,19,25))) g.FillRectangle(backdrop,0,0,Scene.Panel.Width,Scene.Panel.Height);
                foreach (var item in Scene.Items) {
                    if (item.Hidden) continue;
                    var bounds=item.Bounds;
                    if (item==dragged||dragged!=null&&AllowedPlacement!=null&&AllowedPlacement.MovingControls.Contains(item.Name))
                        bounds.Offset(previewOffset);
                    var state=g.Save(); g.SetClip(item.Clip);
                    try {
                        if (item.RegionFrame!=null) {
                            if (Selected==item) using (var pen=new Pen(Color.FromArgb(115,220,223,245),1) { DashStyle=DashStyle.Dash }) g.DrawRectangle(pen,bounds);
                            continue;
                        }
                        PaintItem(g,item,bounds);
                        if (!Scene.Settings.Playing && item==Selected)
                            using (var pen=new Pen(Color.FromArgb(116,215,253),1.8f)) g.DrawRectangle(pen,bounds);
                    } finally { g.Restore(state); }
                }
                if(!Scene.Settings.Playing&&AllowedPlacement!=null) {
                    using(var outline=new Pen(Color.FromArgb(234,115,250,184),1.7f)) {
                        foreach(var area in AllowedPlacement.Areas) {
                            g.DrawRectangle(outline,area.X,area.Y,Math.Max(1,area.Width-1),Math.Max(1,area.Height-1));
                        }
                    }
                }
                g.Restore(saved);
                if (Subwindow.Length!=0) PaintChooser(g);
            }
            if (!Scene.Settings.Playing||Scene.Settings.Visible&&Scene.Settings.MapDiscovered) {
                if (Scene.Model.EntrySprite!="") {
                    using (var brush=new SolidBrush(Color.FromArgb(169,17,31,42))) g.FillEllipse(brush,entryBounds);
                    if (!Sprite(g,Scene.Model.EntrySprite,Rectangle.Round(entryBounds),0))
                        TextRenderer.DrawText(g,L.T("入口"),Font,Rectangle.Round(entryBounds),Color.White,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);
                    if (!Scene.Settings.Enabled) using (var dim=new SolidBrush(Color.FromArgb(170,40,44,48))) g.FillEllipse(dim,entryBounds);
                }
            }
            TextRenderer.DrawText(g,Scene.Settings.Playing ? L.T("离线试玩") : L.T("布局预览"),Font,new Point(15,13),Color.FromArgb(192,207,220));
        }
        private void PaintItem(Graphics g,VisualItem item,Rectangle bounds) {
            if (item.Node.Key=="scrollbarType") {
                var list=Scene.Lists.FirstOrDefault(i=>i.Slider==item.Name);
                if (list!=null) PaintScroll(g,bounds,list.Offset,list.Maximum);
                return;
            }
            bool masked=!String.IsNullOrWhiteSpace(item.Country)&&PaintMaskedShield(g,item,bounds);
            if (!masked && item.Sprite!="" && item.Sprite!=null && item.Sprite!="GFX_pf075_transparent_hit") {
                Sprite(g,item.Sprite,bounds,item.FrameIndex);
            }
            if (!masked&&!String.IsNullOrWhiteSpace(item.Country)) PaintFlag(g,item.Country,bounds);
            if (!String.IsNullOrEmpty(item.Text) && bounds.Width>=1 && bounds.Height>=1) {
                string mode=item.Node.Get("format");
                using (var format=new StringFormat { Alignment=mode=="left"?StringAlignment.Near:mode=="right"?StringAlignment.Far:StringAlignment.Center,
                    LineAlignment=StringAlignment.Center, Trimming=StringTrimming.EllipsisCharacter, FormatFlags=StringFormatFlags.NoWrap })
                using (var brush=new SolidBrush(item.Dimmed?Color.FromArgb(155,154,156):Color.FromArgb(251,245,228))) {
                    var shadow=bounds; shadow.Offset(1,1);
                    int fontSize=item.FontSize;
                    while(fontSize>7&&(g.MeasureString(item.Text,Typeface(fontSize),Int32.MaxValue,format).Width>bounds.Width-2||
                        Typeface(fontSize).GetHeight(g)>bounds.Height-1))--fontSize;
                    g.DrawString(item.Text,Typeface(fontSize),Brushes.Black,shadow,format);
                    g.DrawString(item.Text,Typeface(fontSize),brush,bounds,format);
                }
            }
        }
        private bool PaintMaskedShield(Graphics g,VisualItem item,Rectangle target) {
            var sprite=Info(item.Sprite);
            if(sprite==null||sprite.Mask==null)return false;
            string key=item.Sprite+"\n"+item.Country;
            Bitmap shield;
            if(!shields.TryGetValue(key,out shield)) {
                var frame=Texture(sprite.Texture);var mask=Texture(sprite.Mask);var flag=Texture("gfx/flags/"+item.Country+".tga");
                if(frame==null||mask==null||flag==null||frame.Width>256||frame.Height>256)return false;
                shield=new Bitmap(frame.Width,frame.Height,System.Drawing.Imaging.PixelFormat.Format32bppArgb);
                var maskImage=mask.Bitmap; var flagImage=flag.Bitmap;
                int left=mask.Width,right=-1,top=mask.Height,bottom=-1;
                for(int y=0;y<mask.Height;++y)for(int x=0;x<mask.Width;++x) {
                    var c=maskImage.GetPixel(x,y);
                    if(MaskAlpha(c,mask.HasAlpha)<=16)continue;
                    left=Math.Min(left,x);right=Math.Max(right,x);top=Math.Min(top,y);bottom=Math.Max(bottom,y);
                }
                if(right<left||bottom<top){shield.Dispose();return false;}
                int maskX=(shield.Width-mask.Width)/2,maskY=(shield.Height-mask.Height)/2+Math.Max(0,(shield.Height-mask.Height)/8);
                for(int y=Math.Max(0,maskY);y<Math.Min(shield.Height,maskY+mask.Height);++y)
                for(int x=Math.Max(0,maskX);x<Math.Min(shield.Width,maskX+mask.Width);++x) {
                    int mx=x-maskX,my=y-maskY;
                    int alpha=MaskAlpha(maskImage.GetPixel(mx,my),mask.HasAlpha);
                    if(alpha==0)continue;
                    int fx=Math.Max(0,Math.Min(flag.Width-1,(mx-left)*flag.Width/Math.Max(1,right-left+1)));
                    int fy=Math.Max(0,Math.Min(flag.Height-1,(my-top)*flag.Height/Math.Max(1,bottom-top+1)));
                    var face=flagImage.GetPixel(fx,fy);
                    shield.SetPixel(x,y,Color.FromArgb(alpha,face.R,face.G,face.B));
                }
                using(var composite=Graphics.FromImage(shield))composite.DrawImageUnscaled(frame.Bitmap,0,0);
                if(shields.Count>=200){foreach(var face in shields.Values)face.Dispose();shields.Clear();}
                shields[key]=shield;
            }
            g.DrawImage(shield,target);return true;
        }
        private static int MaskAlpha(Color c,bool alpha) {return alpha?c.A:(c.R+c.G+c.B)/3;}
        private void PaintFlag(Graphics g,string tag,Rectangle bounds) {
            var tex=Texture("gfx/flags/"+tag+".tga");
            var center=new Rectangle(bounds.X+bounds.Width/6,bounds.Y+bounds.Height/5,bounds.Width*2/3,bounds.Height*3/5);
            if (center.Width<3||center.Height<3) return;
            using (var shape=new GraphicsPath()) {
                shape.AddPolygon(new[] { new Point(center.Left,center.Top),new Point(center.Right,center.Top),
                    new Point(center.Right-1,center.Top+center.Height*7/10),new Point(center.Left+center.Width/2,center.Bottom),
                    new Point(center.Left+1,center.Top+center.Height*7/10) });
                var state=g.Save(); g.SetClip(shape,CombineMode.Intersect);
                if (tex!=null) g.DrawImage(tex.Bitmap,center);
                else {
                    using (var brush=new SolidBrush(Color.FromArgb(89,104,119))) g.FillRectangle(brush,center);
                    using (var sf=new StringFormat { Alignment=StringAlignment.Center,LineAlignment=StringAlignment.Center })
                        g.DrawString(tag,Typeface(Math.Max(8,Math.Min(13,center.Width/3))),Brushes.White,center,sf);
                }
                g.Restore(state);
            }
        }
        private void PaintScroll(Graphics g,Rectangle b,int offset,int maximum) {
            if (b.Width<1||b.Height<11) return;
            int cap=Math.Min(b.Width,b.Height/3), rail=Math.Max(3,b.Height-cap*2), x=b.Left+b.Width/2;
            int thumb=cap+(maximum==0?0:(rail-cap)*offset/maximum);
            using (var pen=new Pen(Color.FromArgb(139,112,64),2)) g.DrawLine(pen,x,b.Top+cap,x,b.Bottom-cap);
            if (!Sprite(g,"GFX_scroll_up",new Rectangle(b.X,b.Y,b.Width,cap),0)) {
                g.FillPolygon(Brushes.DarkGoldenrod,new[]{new Point(x,b.Top),new Point(b.Right,b.Top+cap),new Point(b.Left,b.Top+cap)});
            }
            if (!Sprite(g,"GFX_scroll_down",new Rectangle(b.X,b.Bottom-cap,b.Width,cap),0)) {
                g.FillPolygon(Brushes.DarkGoldenrod,new[]{new Point(x,b.Bottom),new Point(b.Right,b.Bottom-cap),new Point(b.Left,b.Bottom-cap)});
            }
            if (!Sprite(g,"GFX_scroll_drager",new Rectangle(b.X,b.Y+thumb,b.Width,cap),0))
                g.FillEllipse(Brushes.Goldenrod,b.X,b.Y+thumb,b.Width,cap);
        }
        private string[] Options() {
            if (Subwindow=="instances") return Scene.Model.Instances.Select(i=>Scene.Model.Loc.Caption(i.Name==""?i.Id:i.Name)+"  ·  "+i.Leader).ToArray();
            if (Subwindow=="vote") return Scene.Model.Instance==null?new string[0]:Scene.Model.Instance.Members.ToArray();
            return new string[0];
        }
        private void PaintChooser(Graphics g) {
            var texts=Options();
            float x=Math.Max(4,Math.Min(Width-218,lastWindow.Left-220));
            if (x+210>lastWindow.Left && lastWindow.Right+218<Width) x=lastWindow.Right+8;
            float y=Math.Max(45,lastWindow.Top+20);
            int visible=Math.Min(8,Math.Max(1,texts.Length));
            chooserBounds=new RectangleF(x,y,208,visible*26+70);
            chooserRail=new RectangleF(chooserBounds.Right-19,y+40,15,visible*26);
            using (var brush=new SolidBrush(Color.FromArgb(247,27,42,53))) g.FillRectangle(brush,chooserBounds);
            using (var pen=new Pen(Color.FromArgb(162,143,97))) g.DrawRectangle(pen,Rectangle.Round(chooserBounds));
            TextRenderer.DrawText(g,Subwindow=="instances"?L.T("选择实例"):L.T("候选人（样例）"),Font,Rectangle.Round(new RectangleF(x+9,y+9,186,25)),Color.White);
            for (int n=0;n<visible && SubwindowOffset+n<texts.Length;++n) {
                var rect=new Rectangle((int)x+9,(int)y+40+26*n,178,24);
                using (var brush=new SolidBrush(Color.FromArgb(46,66,80))) g.FillRectangle(brush,rect);
                TextRenderer.DrawText(g,texts[SubwindowOffset+n],Font,rect,Color.White,TextFormatFlags.EndEllipsis|TextFormatFlags.VerticalCenter);
            }
            if (texts.Length>visible) PaintScroll(g,Rectangle.Round(chooserRail),SubwindowOffset,texts.Length-visible);
            TextRenderer.DrawText(g,L.T("取消"),Font,new Point((int)x+10,(int)chooserBounds.Bottom-23),Color.FromArgb(197,216,233));
        }
        internal Point Local(Point point) { return new Point((int)Math.Round((point.X-lastWindow.X)/scale),(int)Math.Round((point.Y-lastWindow.Y)/scale)); }
        internal Point ClientAt(Point local) { return new Point((int)Math.Round(local.X*scale+lastWindow.X),(int)Math.Round(local.Y*scale+lastWindow.Y)); }
        internal VisualItem HitAt(Point local,bool editing) {
            if (Scene==null || !PanelVisible) return null;
            for (int i=Scene.Items.Count-1;i>=0;--i) {
                var item=Scene.Items[i];
                if (item.Hidden||!item.Bounds.Contains(local)||!item.Clip.Contains(local)) continue;
                if (item.RegionFrame!=null) {
                    if(!editing)continue;
                    var interior=item.Bounds; interior.Inflate(-7,-7);
                    if (interior.Contains(local)) continue;
                    return item;
                }
                if (editing ? item.Name!=Scene.HoverControl && item.Name!=Scene.BackgroundControl &&
                    !Editing.IsIconHint(Scene.Model.Window.Doc,item.Node) : item.Hit) return item;
            }
            return null;
        }
        internal void Escape() {
            if (Subwindow.Length>0) { Subwindow=""; SubwindowOffset=0; }
            else if (Scene!=null&&Scene.Settings.Playing) PanelVisible=false;
            Invalidate();
        }
        private void Tip(string text) {
            if (lastTip==text) return;
            lastTip=text; tips.SetToolTip(this,text);
        }
        protected override void OnMouseMove(MouseEventArgs e) {
            base.OnMouseMove(e);
            if (Scene==null||Scene.Model.Window==null) return;
            var p=Local(e.Location);
            if (scrolling!=null) { RailSeek(scrolling,p.Y,false); return; }
            if (dragged!=null) {
                var origin=Local(press);
                var attempted=new Point(dragged.Raw.X+p.X-origin.X,dragged.Raw.Y+p.Y-origin.Y);
                Point placed=attempted;
                if(AllowedPlacement!=null&&!AllowedPlacement.TryNearest(attempted,out placed))placed=dragged.Raw;
                previewOffset=new Point(placed.X-dragged.Raw.X,placed.Y-dragged.Raw.Y);
                Invalidate(); return;
            }
            if (entryBounds.Contains(e.Location)&&(Scene.Settings.Visible&&Scene.Settings.MapDiscovered||!Scene.Settings.Playing)) {
                string key=Scene.Model.Rule.Read(L.T("入口"),"interface","entry_button","tooltip");
                Tip(Scene.Model.Loc.Caption(key)+(Scene.Settings.Enabled?"":L.T("\n样例状态  ·  当前不可用"))); Cursor=Scene.Settings.Enabled?Cursors.Hand:Cursors.Default; return;
            }
            if (!chooserBounds.IsEmpty&&chooserBounds.Contains(e.Location)) { Tip(""); Cursor=Cursors.Hand; return; }
            var hit=HitAt(p,!Scene.Settings.Playing);
            string display=hit==null?"":hit.Text??"";
            string tip=hit==null?"":hit.Tooltip??"";
            if(display.Length>0&&!tip.Contains(display))tip=display+(tip.Length==0?"":"\n"+tip);
            Tip(hit!=null&&tip.Length==0&&!Scene.Settings.Playing?hit.Name:tip);
            Cursor=hit!=null&&(hit.Click||!Scene.Settings.Playing&&!hit.Locked)?Cursors.Hand:Cursors.Default;
        }
        protected override void OnMouseDown(MouseEventArgs e) {
            base.OnMouseDown(e);
            if (e.Button!=MouseButtons.Left||Scene==null||Scene.Model.Window==null) return;
            Focus(); press=e.Location;
            if (!chooserBounds.IsEmpty&&chooserBounds.Contains(e.Location) || entryBounds.Contains(e.Location)) return;
            var point=Local(e.Location);
            hitAtPress=HitAt(point,!Scene.Settings.Playing);
            if (hitAtPress==null) return;
            if (!Scene.Settings.Playing) {
                Selected=hitAtPress; if (SelectItem!=null) SelectItem(Selected);
                if (!Selected.Locked&&Selected.Positioned) { dragged=Selected; Capture=true; }
                Invalidate(); return;
            }
            if (hitAtPress.Action=="scroll") {
                var list=Scene.Lists.FirstOrDefault(x=>x.Key==hitAtPress.ListKey);
                if (list!=null) {
                    scrolling=list; Capture=true;
                    RailSeek(list,point.Y,true);
                }
            }
        }
        private void RailSeek(ListViewState list,int y,bool arrows) {
            int cap=Math.Min(list.Rail.Width,list.Rail.Height/3), value;
            if (arrows&&y<=list.Rail.Top+cap) value=list.Offset-1;
            else if (arrows&&y>=list.Rail.Bottom-cap) value=list.Offset+1;
            else {
                int top=list.Rail.Top+cap, span=Math.Max(1,list.Rail.Height-2*cap);
                value=(int)Math.Round((double)(y-top)/span*list.Maximum);
            }
            if (ScrollList!=null) ScrollList(list.Key,Math.Max(0,Math.Min(value,list.Maximum)));
        }
        protected override void OnMouseUp(MouseEventArgs e) {
            base.OnMouseUp(e);
            if (Scene==null||e.Button!=MouseButtons.Left) return;
            releasing=true;
            try { Capture=false; } finally{ releasing=false; }
            if (scrolling!=null) { scrolling=null; return; }
            if (dragged!=null) {
                var item=dragged; var start=Local(press);var finish=Local(e.Location);
                var move=new Point(finish.X-start.X,finish.Y-start.Y);
                dragged=null; previewOffset=Point.Empty; Invalidate();
                var binding=AllowedPlacement==null?null:ControlBindings.ForItem(Scene.Model,item);
                bool score=binding!=null&&(binding.Path==Editing.NominationScore||binding.Path==Editing.NominationHit);
                int threshold=score?0:2;
                if ((Math.Abs(e.X-press.X)>threshold||Math.Abs(e.Y-press.Y)>threshold)&&MoveItem!=null)
                    MoveItem(item,Point.Add(item.Raw,new Size(move)));
                return;
            }
            if (entryBounds.Contains(e.Location)&&entryBounds.Contains(press)&&Scene.Model.EntrySprite!="") {
                if (!Scene.Settings.Visible||!Scene.Settings.Enabled||!Scene.Settings.MapDiscovered) return;
                if (Scene.Model.Instances.Count>1) { if (Subwindow=="instances") Escape(); else { PanelVisible=true; Subwindow="instances"; SubwindowOffset=0; } }
                else { PanelVisible=!PanelVisible; Subwindow=""; }
                Invalidate(); return;
            }
            if (!chooserBounds.IsEmpty&&chooserBounds.Contains(e.Location)) {
                int offset=e.Y-(int)chooserBounds.Top-40;
                var texts=Options();
                if (chooserRail.Contains(e.Location)&&texts.Length>8) {
                    double position=(e.Y-chooserRail.Top)/chooserRail.Height;
                    SubwindowOffset=Math.Max(0,Math.Min(texts.Length-8,(int)Math.Round(position*(texts.Length-8)))); Invalidate(); return;
                }
                if (e.Y>=chooserBounds.Bottom-25) { Escape(); return; }
                int row=offset/26+SubwindowOffset;
                if (offset>=0&&row<texts.Length) {
                    var clicked=Subwindow; Escape();
                    if (clicked=="instances"&&PickInstance!=null) PickInstance(row);
                    if (clicked=="vote"&&Tell!=null) Tell(L.T("样例投票  ·  ")+texts[row]);
                }
                return;
            }
            if (!Scene.Settings.Playing) return;
            var hit=HitAt(Local(e.Location),false);
            if(hit==null||hit!=hitAtPress||!hit.Click)return;
            if(hit.Action=="close"){PanelVisible=false;Subwindow="";Invalidate();}
            else if(hit.Action=="instances"||hit.Action=="vote") {
                if(Subwindow==hit.Action)Escape(); else { Subwindow=hit.Action; SubwindowOffset=0; Invalidate(); }
            } else if(ActivateItem!=null) ActivateItem(hit);
        }
        protected override void OnMouseWheel(MouseEventArgs e) {
            if (Scene==null) return;
            if (chooserBounds.Contains(e.Location)) {
                SubwindowOffset=Math.Max(0,Math.Min(Options().Length-8,SubwindowOffset-Math.Sign(e.Delta)));Invalidate();return;
            }
            var point=Local(e.Location);
            var list=Scene.Lists.LastOrDefault(l=>l.Bounds.Contains(point)&&l.Maximum>0);
            if (list!=null&&PanelVisible&&ScrollList!=null) ScrollList(list.Key,Math.Max(0,Math.Min(list.Maximum,list.Offset-Math.Sign(e.Delta))));
            else base.OnMouseWheel(e);
        }
        protected override void OnMouseLeave(EventArgs e) { Tip(""); base.OnMouseLeave(e); }
        protected override void OnMouseCaptureChanged(EventArgs e) {
            base.OnMouseCaptureChanged(e);
            if(!Capture && !releasing && (dragged!=null||scrolling!=null)) {
                dragged=null;scrolling=null;previewOffset=Point.Empty;Invalidate();
            }
        }
        private bool releasing;
    }
}
