using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PoliticalEditor {
    internal sealed class MaterialsDialog : Form {
        private readonly Model model;
        private readonly TextBox search=Ui.Box("",236);
        private readonly ComboBox category=Ui.Combo(116);
        private readonly CheckBox mounted=new CheckBox{ Text=L.T("仅可用"), AutoSize=true, Checked=true, Margin=new Padding(5,7,5,0) };
        private readonly ListBox pieces=new ListBox { Dock=DockStyle.Fill, IntegralHeight=false, HorizontalScrollbar=true, Font=Ui.Small };
        private readonly TextBox detail=Ui.View("");
        private readonly PictureBox imageBox=new PictureBox { Dock=DockStyle.Fill,BackColor=Color.FromArgb(46,61,73),SizeMode=PictureBoxSizeMode.Zoom, Margin=new Padding(0,0,0,4) };
        internal Sprite Chosen;
        internal MaterialsDialog(Model current) {
            model=current;
            Ui.Dialog(this,L.T("选择已登记的素材"),new Size(970,603));
            var outer=Ui.Stack(47,0,46);outer.Padding=new Padding(14,11,14,10);Controls.Add(outer);
            var header=Ui.Flow();header.Dock=DockStyle.Fill;
            header.Controls.Add(Ui.Label(L.T("来源")));header.Controls.Add(category);header.Controls.Add(Ui.Label(L.T("查找")));header.Controls.Add(search);header.Controls.Add(mounted);
            Ui.LoadChoices(category,new[] {"全部", "游戏原版", "源 Mod", "依赖", "项目草稿"},"全部");outer.Controls.Add(header,0,0);
            var split=new SplitContainer{Size=new Size(920,480),SplitterDistance=433,Dock=DockStyle.Fill,SplitterWidth=6,BackColor=Ui.Light};
            Ui.Fill(split.Panel1,pieces);var right=Ui.Stack(232,0);right.Controls.Add(imageBox,0,0);right.Controls.Add(detail,0,1);
            Ui.Fill(split.Panel2,right);outer.Controls.Add(split,0,1);
            var bottom=Ui.Flow();bottom.Dock=DockStyle.Fill;bottom.Padding=new Padding(0,8,0,0);
            bottom.Controls.Add(Ui.Button(L.T("使用所选素材"),(s,e)=>Done(),true,133));bottom.Controls.Add(Ui.Button(L.T("取消"),(s,e)=>Close()));outer.Controls.Add(bottom,0,2);
            search.TextChanged+=(s,e)=>Filter();category.SelectedIndexChanged+=(s,e)=>Filter();mounted.CheckedChanged+=(s,e)=>Filter();
            pieces.SelectedIndexChanged+=(s,e)=>Look();pieces.DoubleClick+=(s,e)=>Done();Filter();
        }
        private void Filter() {
            string q=search.Text.Trim(),which=Ui.Selected(category);
            var values=model.Images.All.Where(sprite=> {
                if(mounted.Checked&&sprite.SourceRoot=="未找到")return false;
                if(q.Length>0&&sprite.Name.IndexOf(q,StringComparison.OrdinalIgnoreCase)<0&&sprite.Texture.IndexOf(q,StringComparison.OrdinalIgnoreCase)<0)return false;
                string def=model.Mount.Source(sprite.Path);
                switch(which){case "游戏原版":return def==model.Mount.Roots.Game;case "源 Mod":return def==model.Mount.Roots.Mod;
                    case "依赖":return (model.Mount.Roots.Dependencies??new string[0]).Contains(def,Files.Windows);case "项目草稿":return def=="项目草稿";default:return true;}
            }).OrderBy(sprite=>sprite.Name,StringComparer.OrdinalIgnoreCase).Take(600).ToArray();
            pieces.BeginUpdate();pieces.Items.Clear();pieces.Items.AddRange(values);pieces.EndUpdate();
            detail.Text=values.Length==600?L.T("显示前 600 条结果。可用 sprite 名或贴图路径缩小范围。"):L.T("找到 ")+values.Length+L.T(" 项。");
        }
        private void Look() {
            var sprite=pieces.SelectedItem as Sprite;
            if(imageBox.Image!=null){imageBox.Image.Dispose();imageBox.Image=null;}
            if(sprite==null)return;
            string desc=sprite.Name+L.T("\r\n登记文件  ")+sprite.Path+L.T("\r\n贴图  ")+sprite.Texture+L.T("\r\n贴图来源  ")+L.T(sprite.SourceRoot)+L.T("\r\n帧数  ")+sprite.Frames;
            try {
                using(var tex=model.Images.Load(sprite)) {
                    imageBox.Image=new Bitmap(tex.Bitmap);
                    desc+="\r\n"+tex.Width+" × "+tex.Height+"  ·  "+(tex.HasAlpha?L.T("存在透明像素"):L.T("无透明像素"));
                }
            }catch(Exception ex){desc+=L.T("\r\n无法解码  ")+ex.Message;}
            var all=model.Images.Find(sprite.Name);
            if(all.Count>1) desc+=L.T("\r\n存在多个同名定义，不能默认为唯一胜出。建议用新 ID 登记所选贴图。\r\n")+String.Join("\r\n",all.Select(s=>s.Path));
            detail.Text=desc;
        }
        private void Done() { if (pieces.SelectedItem is Sprite) { Chosen=(Sprite)pieces.SelectedItem;DialogResult=DialogResult.OK;Close(); } }
        protected override void Dispose(bool disposing){if(disposing&&imageBox.Image!=null){imageBox.Image.Dispose();imageBox.Image=null;}base.Dispose(disposing);}
    }

    internal sealed class ComponentDialog : Form {
        private readonly Project project;
        private readonly Model model;
        private readonly Scene scene;
        private readonly VisualItem selected;
        private readonly bool shared;
        private readonly ComboBox kind=Ui.Combo(183), parent=Ui.Combo(280), action=Ui.Combo(256);
        private readonly TextBox name=Ui.Box("pf_editor_icon",235), x=Ui.Box("112",64), y=Ui.Box("95",64), tooltip=Ui.Box("",465);
        private readonly TextBox sprite=Ui.Box("",240), gfx=Ui.Box("",298), texture=Ui.Box("",323), frames=Ui.Box("1",56);
        private readonly CheckBox replace=new CheckBox{ Text=L.T("确认替换已占用的绑定"),AutoSize=true,Margin=new Padding(6,7,6,0) };
        private readonly CheckBox restyle=new CheckBox{ Text=L.T("同时为整组行动按钮换图"),AutoSize=true,Margin=new Padding(6,7,6,0) };
        private readonly TextBox info=Ui.Help("");
        private readonly CheckBox advanced=new CheckBox{Text=L.T("手动填写素材参数"),AutoSize=true,Margin=new Padding(7,7,5,0)};
        private FlowLayoutPanel actionRow,wordsRow;
        private TextBox placementHelp;
        private Button back,next,apply;
        private readonly PictureBox thumbnail=new PictureBox { Dock=DockStyle.Fill,BackColor=Color.FromArgb(47,62,74),SizeMode=PictureBoxSizeMode.Zoom, Margin=Padding.Empty };
        private readonly TabControl steps=new TabControl {Dock=DockStyle.Fill,Padding=new Point(14,5),ShowToolTips=true,Multiline=true};
        private Sprite existing;
        private byte[] imported;
        private string originalExt="";
        internal ComponentDialog(Project p,Model m,Scene s,VisualItem item,bool editShared) {
            project=p;model=m;scene=s;selected=item;shared=editShared;
            Ui.Dialog(this,L.T("组件与绑定"),new Size(970,617));
            var main=Ui.Stack(45,0,51);main.Padding=new Padding(15,12,15,10);Controls.Add(main);
            main.Controls.Add(Ui.Label(L.T("为 ")+m.Type.Node.Key+L.T(" 添加组件"),Ui.Heading),0,0);
            steps.TabPages.Add(new TabPage(L.T("1. 选择作用")) {BackColor=Ui.Paper,ToolTipText=L.T("选择图标、提示或行动要做什么")});
            steps.TabPages.Add(new TabPage(L.T("2. 选择图片")) {BackColor=Ui.Paper,ToolTipText=L.T("从素材库选图，或导入本地 DDS/TGA")});
            steps.TabPages.Add(new TabPage(L.T("3. 放到面板")) {BackColor=Ui.Paper,ToolTipText=L.T("在预览图上点要放的位置")});
            main.Controls.Add(steps,0,1);
            var actionPage=Ui.Stack(52,53,53,0);actionPage.Padding=new Padding(10);
            Ui.LoadChoices(kind,new[] {"装饰图标","图标＋悬停提示","连接已有行动","入口按钮","替换选中控件的材质","文本","只读提示区",L.T("资源余额"),"资源月变"},"装饰图标");
            Ui.FitChoices(kind);
            Ui.FitRow(actionPage,Row(Ui.Label(L.T("想添加什么？")),kind),0);
            Ui.Load(action,scene.Actions.Select(a=>a.Module+"."+a.Id),null);
            actionRow=Row(Ui.Label(L.T("已有行动")),action,restyle,replace);Ui.FitRow(actionPage,actionRow,1);
            restyle.CheckedChanged+=(o,e)=>UpdateSteps();
            tooltip.Multiline=true;tooltip.Height=47;tooltip.ScrollBars=ScrollBars.Vertical;
            wordsRow=Row(Ui.Label(L.T("说明文字")),tooltip,Ui.Label(L.T("直接写玩家会看到的话"),Ui.Small,Ui.Soft));
            Ui.Tip(tooltip,L.T("例如「这里显示蒙古议会的徽记」。也可输入工程里已有的本地化键。"));
            Ui.FitRow(actionPage,wordsRow,2);actionPage.Controls.Add(info,0,3);
            steps.TabPages[0].Controls.Add(actionPage);
            var asset=Ui.Stack(50,50,50,50,0);asset.Padding=new Padding(10);
            Ui.FitRow(asset,Row(Ui.Button(L.T("从素材库选图…"),(o,e)=>Existing(),false,148),Ui.Button(L.T("从电脑选 DDS/TGA…"),(o,e)=>Import(),false,163),advanced),0);
            gfx.Text=m.GfxPath;
            Ui.FitRow(asset,Row(Ui.Label(L.T("游戏图标名")),sprite,Ui.Label(L.T("帧数")),frames),1);
            Ui.FitRow(asset,Row(Ui.Label(L.T("登记文件")),gfx),2);
            Ui.FitRow(asset,Row(Ui.Label(L.T("贴图路径")),texture),3);asset.Controls.Add(thumbnail,0,4);steps.TabPages[1].Controls.Add(asset);
            advanced.CheckedChanged+=(o,e)=>{foreach(var box in new[]{sprite,gfx,texture,frames})box.ReadOnly=!advanced.Checked;};
            foreach(var box in new[]{sprite,gfx,texture,frames})box.ReadOnly=true;
            Ui.Tip(advanced,L.T("选图后会自动填写这些参数。需要指定登记名或相对路径时再勾选。"));
            var location=Ui.Stack(52,50,54,0);location.Padding=new Padding(10);
            Ui.Load(parent,new[]{m.Window.Node.Get("name")}.Concat(s.Items.Where(i=>i.Node.Key=="windowType"||i.Node.Key=="containerWindowType").Where(i=>i.RegionFrame==null).Select(i=>i.Name)),null);
            Ui.FitRow(location,Row(Ui.Label(L.T("放到哪个窗口")),parent),0);
            if(item!=null){x.Text=item.Raw.X.ToString();y.Text=item.Raw.Y.ToString();}
            name.Text=NextName("pf_editor_icon",value=>s.ByName(value)!=null||s.ByName(value+"_pf_hint")!=null);
            Ui.FitRow(location,Row(Ui.Label(L.T("组件名")),name,Ui.Label("X"),x,Ui.Label("Y"),y),1);
            Ui.FitRow(location,Row(Ui.Button(L.T("在预览图上点位置…"),(o,e)=>Pick(),false,152)),2);
            placementHelp=Ui.Help("");location.Controls.Add(placementHelp,0,3);
            steps.TabPages[2].Controls.Add(location);
            apply=Ui.Button(L.T("预览改动…"),async(o,e)=>await Finish(),true,144);
            back=Ui.Button(L.T("上一步"),(o,e)=>Navigate(-1));next=Ui.Button(L.T("下一步"),(o,e)=>Navigate(1));
            Ui.FitRow(main,Row(apply,back,next,Ui.Button(L.T("取消"),(o,e)=>Close())),2);
            steps.SelectedIndexChanged+=(o,e)=>UpdateSteps();
            kind.SelectedIndexChanged+=(o,e)=>Describe();action.SelectedIndexChanged+=(o,e)=>Describe();Describe();
        }
        private static FlowLayoutPanel Row(params Control[] c) {var row=Ui.Flow();row.Dock=DockStyle.Fill;row.Padding=new Padding(0,9,0,0);row.Controls.AddRange(c);return row;}
        private static string NextName(string initial,Func<string,bool> used) {
            string result=initial;
            for(int n=2;used(result);++n)result=initial+"_"+n;
            return result;
        }
        private bool NeedsImage {
            get { var mode=Ui.Selected(kind); return new[]{"装饰图标","图标＋悬停提示","入口按钮","替换选中控件的材质"}.Contains(mode)||mode=="连接已有行动"&&restyle.Checked; }
        }
        private bool PlacesControl {
            get { return !new[]{"入口按钮","连接已有行动","替换选中控件的材质"}.Contains(Ui.Selected(kind)); }
        }
        private int Neighbor(int direction) {
            bool chooseImage=NeedsImage;
            int i=steps.SelectedIndex+direction;
            while(i>=0&&i<3) {
                if(i==0||i==1&&chooseImage||i==2&&PlacesControl)return i;
                i+=direction;
            }
            return -1;
        }
        private void Navigate(int direction) { int index=Neighbor(direction);if(index>=0)steps.SelectedIndex=index; }
        private void UpdateSteps() { back.Enabled=Neighbor(-1)>=0;next.Enabled=Neighbor(1)>=0; }
        private void Describe() {
            string label=Ui.Selected(kind),body="";
            actionRow.Visible=label=="连接已有行动"||label==L.T("资源余额")||label=="资源月变";
            action.Visible=actionRow.Controls[0].Visible=label=="连接已有行动";
            restyle.Visible=label=="连接已有行动";
            replace.Visible=label==L.T("资源余额")||label=="资源月变";
            wordsRow.Visible=label=="文本"||label=="只读提示区"||label=="图标＋悬停提示";
            parent.Enabled=label!="图标＋悬停提示"&&label!="只读提示区";
            if(!parent.Enabled&&parent.Items.Count>0)parent.SelectedIndex=0;
            placementHelp.Text=PlacesControl?L.T("保持组件名和窗口的默认值即可。点「在预览图上点位置」，再点面板中的空白处。X 和 Y 会自动填写。"):
                label=="入口按钮"?L.T("入口位于游戏小地图旁。请到「2. 选择图片」更换图案，入口沿用当前布局的位置。"):
                L.T("这个选项沿用现有控件或行动列表的位置。");
            if(label=="入口按钮") {
                body=L.T("作用：点击小地图旁的入口，打开 ")+model.Type.Node.Key+L.T(" 面板。\r\n\r\n到「2. 选择图片」选图，然后点「预览改动」。");
                if(model.Hud==null)body+=L.T("当前类型没有有效的 HUD 区域。");
                else body+=L.T("\r\n\r\n现有图标  ")+model.EntrySprite;
                if(imported!=null||existing!=null)gfx.Text=model.GfxPath;
            } else if(label=="连接已有行动") {
                var candidate=scene.Actions.FirstOrDefault(k=>k.Module+"."+k.Id==Ui.Selected(action));
                body=candidate==null?L.T("当前未找到已有行动。请先在类型标签定义框架支持的行动。"):
                    L.T("所选行动会出现在面板的行动列表中，按已有条件执行。直接点「预览改动」即可。\r\n")+
                    L.T("如果另外选择图片，同一行动列表的可点击按钮会一起换样式。\r\n\r\n")+candidate.Tip;
            } else if(label==L.T("资源余额")||label=="资源月变") {
                body=L.T("当前资源  ")+model.PrimaryId+"\r\n"+(model.Primary==null?L.T("此类型尚无主资源。"):model.Primary.From.Path)+
                    L.T("\r\n现有布局每项只有一个主显示位。确认替换后会将读数绑定到新控件，保留已有资源 ID。");
            } else if(label=="替换选中控件的材质")
                body=selected==null?L.T("请先在画布选择目标。"):L.T("保留 ")+selected.Name+L.T(" 的位置和行为。视觉改动遵循当前共享布局选项。");
            else if(label=="装饰图标")body=L.T("只显示一张图片，适合放徽章、标志或分隔图案。选图后，再到面板上点一个位置。");
            else if(label=="图标＋悬停提示")body=L.T("显示一张图片，鼠标停在图上时显示你写的说明。点击图标不会执行改革等行动。\r\n\r\n例如在「说明文字」写「这里显示蒙古议会的徽记」，再选图片和位置。");
            else body=label=="只读提示区"?L.T("需要一块透明的悬停区时使用。直接填写说明文字，再点面板上的位置。"):
                L.T("填写玩家会在面板上看到的文字，再选它的位置。");
            info.Text=body;
            UpdateSteps();
        }
        private void ReplacePreview(Bitmap bitmap) {
            if(thumbnail.Image!=null)thumbnail.Image.Dispose();
            thumbnail.Image=bitmap;
        }
        private void Existing() {try {
            using(var dlg=new MaterialsDialog(model))if(dlg.ShowDialog(this)==DialogResult.OK)UseExisting(dlg.Chosen);
        }catch(Exception e){Ui.Error(this,e);}}
        private void UseExisting(Sprite selectedSprite) {
                existing=selectedSprite;imported=null;originalExt="";
                bool hud=Ui.Selected(kind)=="入口按钮";sprite.Text=existing.Name;
                if(hud&&existing.Path!=model.GfxPath)sprite.Text=NextName("GFX_"+model.Type.Node.Key+"_entry",value=>model.Images.Find(value).Count!=0);
                texture.Text=existing.Texture;frames.Text=existing.Frames.ToString(CultureInfo.InvariantCulture);
                gfx.Text=model.GfxPath==""?"interface/pf_editor.gfx":model.GfxPath;
                try{using(var raster=model.Images.Load(existing))ReplacePreview(new Bitmap(raster.Bitmap));}catch(Exception){ReplacePreview(null);}
        }
        private void Import() {try {
            string file=Ui.PickFile(this,L.T("EU4 贴图 (*.dds;*.tga)|*.dds;*.tga"));
            if(file==null)return;
            var data=Files.Read(file);
            string extension=Path.GetExtension(file).ToLowerInvariant();
            if(extension!=".dds"&&extension!=".tga")throw new InvalidDataException(L.T("请选择 DDS 或 TGA。"));
            using(var raster=Raster.Decode(data,extension)) {
                ReplacePreview(new Bitmap(raster.Bitmap));
                thumbnail.AccessibleDescription=raster.Width+"×"+raster.Height+(raster.HasAlpha?L.T("，有透明像素"):"");
                info.AppendText(L.T("\r\n\r\n自有素材  ")+raster.Width+" × "+raster.Height+(raster.HasAlpha?L.T("  ·  有透明像素"):L.T("  ·  不透明")));
            }
            imported=data;existing=null;originalExt=extension;
            sprite.Text=NextName("GFX_"+model.Type.Node.Key+"_custom",value=>model.Images.Find(value).Count!=0);
            texture.Text="gfx/interface/"+model.Type.Node.Key+"/"+NextName("editor_icon",value=>model.Mount.Get("gfx/interface/"+model.Type.Node.Key+"/"+value+extension,true)!=null)+extension;
            if(gfx.Text=="")gfx.Text="interface/pf_editor.gfx";frames.Text="1";
        }catch(Exception e){Ui.Error(this,e);}}
        private void Pick() {
            using(var d=new Form()) {
                Ui.Dialog(d,L.T("在面板上点选位置"),new Size(620,687));
                var pc=new PreviewCanvas{Dock=DockStyle.Fill};pc.Use(scene,null);d.Controls.Add(pc);
                pc.MouseClick+=(o,e)=> {
                    if(e.Button!=MouseButtons.Left)return;
                    Point p=pc.Local(e.Location);
                    if(!new Rectangle(Point.Empty,scene.Panel).Contains(p)){d.Text=L.T("请点面板内部");return;}
                    var chosen=Ui.Selected(parent);
                    var parentItem=scene.Items.FirstOrDefault(i=>i.Name==chosen);
                    if(parentItem!=null)p.Offset(-parentItem.Bounds.X,-parentItem.Bounds.Y);
                    x.Text=p.X.ToString();y.Text=p.Y.ToString();d.DialogResult=DialogResult.OK;
                };
                d.ShowDialog(this);
            }
        }
        private Plan BuildPlan(out int[] repairedRows) {
            string label=Ui.Selected(kind), targetSprite=sprite.Text.Trim(), targetGfx=gfx.Text.Trim();
            repairedRows=new int[0];
            var plan=new Plan(project,model.Mount,label);
            bool hud=label=="入口按钮";
            if(hud&&model.Hud==null)throw new InvalidDataException(L.T("现有类型没有 HUD 区域。请在类型和布局标签先登记。"));
            if (NeedsImage) {
                if (targetSprite=="")throw new InvalidDataException(L.T("请在「2. 选择图片」点「从素材库选图」或「从电脑选 DDS/TGA」。"));
                bool reuse=imported==null&&existing!=null&&existing.Name==targetSprite;
                if(hud&&targetGfx!=model.GfxPath)throw new InvalidDataException(L.T("登记文件须使用 HUD 区域指定的 ")+model.GfxPath);
                if(imported!=null&&!Path.GetExtension(texture.Text).Equals(originalExt,StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException(L.T("请保留源贴图的扩展名。改名不会改变图像编码。"));
                if(reuse&&!hud) {
                    if(model.Images.Find(targetSprite).Count!=1||existing.SourceRoot=="未找到")throw new InvalidDataException(L.T("该素材缺失或存在同名定义。请选择有效素材或建立新 ID。"));
                } else targetSprite=Editing.Register(plan,model,targetGfx,targetSprite,texture.Text.Trim(),Document.Int(frames.Text,0),imported,existing);
            }
            if(hud) {Editing.BindEntry(plan,model,targetSprite);return plan;}
            if(label=="连接已有行动") {
                var choice=scene.Actions.FirstOrDefault(a=>a.Module+"."+a.Id==Ui.Selected(action));
                if(choice==null)throw new InvalidDataException(L.T("请选择已有行动。"));
                plan.Set(model.Type.Path,model.Type.Node.Key,new[]{choice.Module,choice.Id,"ui","region"},"actions_region");
                if(!restyle.Checked)return plan;
                var actionTarget=Editing.EnsureVisual(plan,model,shared);
                var region=model.Layout.Node.Child("regions").Child("actions_region");
                string template=region.Get("control");
                foreach(var visual in scene.Items.Where(a=>a.Name.StartsWith(template+"_",StringComparison.Ordinal)&&a.Node.Key=="guiButtonType"))
                    Editing.ChangeTexture(plan,actionTarget,visual,targetSprite);
                return plan;
            }
            var target=Editing.EnsureVisual(plan,model,shared);
            if(label=="替换选中控件的材质") {
                if(selected==null)throw new InvalidDataException(L.T("请先选择控件。"));
                Editing.ChangeTexture(plan,target,selected,targetSprite,SelectedImageSize());
                if(selected.Name==model.Layout.Node.Get("background_control")) {
                    var users=shared?model.LayoutUsers(model.Layout.Node.Key):new[]{model.Type};
                    foreach(var user in users)plan.Set(user.Path,user.Node.Key,new[]{"interface","background"},targetSprite);
                }
                return plan;
            }
            int px,py;
            if(!Int32.TryParse(x.Text,out px)||!Int32.TryParse(y.Text,out py)||Geometry.Limit(px)!=px||Geometry.Limit(py)!=py)throw new InvalidDataException(L.T("X 和 Y 需要是有效整数。"));
            string adapter=label=="装饰图标"?"装饰":label=="图标＋悬停提示"?"带提示的图标":label=="只读提示区"?"提示区":label==L.T("资源余额")?"余额":label=="资源月变"?"月变":"文本";
            bool hasCaption=adapter=="文本"||adapter=="提示区"||adapter=="带提示的图标";
            string caption=hasCaption?tooltip.Text.Trim():"";
            if(hasCaption&&caption=="")throw new InvalidDataException(L.T("请在「1. 选择作用」填上说明文字。"));
            if(caption!=""&&model.Loc.Find(caption)==null)caption=Editing.CreateCaption(plan,model,name.Text.Trim(),caption);
            Size? iconSize=adapter=="带提示的图标"?SelectedImageSize():null;
            repairedRows=Editing.RepairNominationRows(plan,model,null,target);
            Editing.AddControl(plan,model,target,adapter,name.Text.Trim(),px,py,targetSprite,caption,Ui.Selected(parent),replace.Checked,iconSize);
            return plan;
        }
        private Size? SelectedImageSize() {
            if(imported!=null) {
                var known=Raster.HeaderSize(imported,originalExt);
                return new Size(known.Width/Math.Max(1,Document.Int(frames.Text,1)),known.Height);
            }
            if(existing==null)return null;
            var size=model.Images.SizeOf(existing.Name,Size.Empty);
            if(size.Width>0&&size.Height>0)return size;
            using(var bitmap=model.Images.Load(existing))return new Size(bitmap.Width/existing.Frames,bitmap.Height);
        }
        private bool validating;
        internal void PrepareGuidedIcon(string text) {
            Ui.Choose(kind,"图标＋悬停提示");
            tooltip.Text=text;
            next.PerformClick();
            if(steps.SelectedIndex!=1)throw new InvalidOperationException("图标向导无法进入选图。");
            UseExisting(model.Images.Find("GFX_pf075_hre_icon").Single());
            if(!sprite.ReadOnly||!gfx.ReadOnly||thumbnail.Image==null||sprite.Text!="GFX_pf075_hre_icon")
                throw new InvalidOperationException("选择已登记图片后没有完成默认填写或预览。");
            next.PerformClick();
            if(steps.SelectedIndex!=2)throw new InvalidOperationException("图标向导无法进入放置步骤。");
        }
        internal Plan CheckGuidedIcon(string text) {
            PrepareGuidedIcon(text);
            x.Text="145";y.Text="610";
            int[] repairedRows;
            return BuildPlan(out repairedRows);
        }
        private async Task Finish() {
            if(validating)return;
            validating=true;UseWaitCursor=true;apply.Enabled=false;steps.Enabled=false;back.Enabled=false;next.Enabled=false;
            try {
                int[] repairedRows;
                Plan plan=BuildPlan(out repairedRows);var changes=plan.Changes.ToArray();
                if(changes.Length==0){info.AppendText(L.T("\r\n\r\n这些绑定已存在，无需应用。"));steps.SelectedIndex=0;return;}
                var check=await Task.Run(()=>VisualEditValidation.Check(model,plan));
                if(IsDisposed)return;
                if(check.Blocking.Count>0){
                    MessageBox.Show(this,L.T("预检发现以下问题。选图和坐标已保留。")+"\r\n\r\n"+
                        String.Join("\r\n",check.Blocking.Select(p=>p.File+"  "+p.Message)),L.T("无法生成改动预览"),MessageBoxButtons.OK,MessageBoxIcon.Information);
                    return;
                }
                string summary=L.T("这些修改合为一次操作。Ctrl+Z 可一起撤销。");
                if(repairedRows.Length!=0)summary=String.Format(L.T("同时修复选举区第 {0} 排的原有位置冲突。请核对盾徽和评分的位置。"),
                    String.Join(", ",repairedRows))+"\r\n"+summary;
                if(shared&&Ui.Selected(kind)!="入口按钮")summary+=L.T("\r\n受影响布局使用者  ")+String.Join(", ",model.LayoutUsers(model.Layout.Node.Key).Select(s=>s.Node.Key));
                if(VisualEditValidation.Review(this,plan,summary,check.Existing)){DialogResult=DialogResult.OK;Close();}
            } catch(Exception ex){if(!IsDisposed)Ui.Error(this,ex);}
            finally{validating=false;if(!IsDisposed){UseWaitCursor=false;apply.Enabled=true;steps.Enabled=true;UpdateSteps();}}
        }
        protected override void Dispose(bool disposing){if(disposing&&thumbnail.Image!=null){thumbnail.Image.Dispose();thumbnail.Image=null;}base.Dispose(disposing);}
    }

    internal sealed class ExportDialog : Form {
        private readonly Project project;
        private readonly Model model;
        private readonly TextBox folder=Ui.Box("",437), id=Ui.Box("",218);
        private readonly ComboBox mode=Ui.Combo(173);
        private readonly CheckBox snapshot=new CheckBox { Text=L.T("已核对源差异，采用项目快照"),AutoSize=true,Margin=new Padding(6,7,6,0) };
        private readonly ListBox listing=new ListBox{Dock=DockStyle.Fill,IntegralHeight=false,HorizontalScrollbar=true,Font=Ui.Small};
        private readonly TextBox detail=Ui.View("",true);
        private readonly Button write, plan, show, fix, original;
        private ExportPlan current;
        private bool computing,writing;
        internal Problem SelectedProblem { get; private set; }
        internal ExportDialog(Project p,Model m) {
            project=p;model=m;
            Ui.Dialog(this,L.T("预检与导出"),new Size(1060,674));
            var table=Ui.Stack(48,43,43,0,51);table.Padding=new Padding(14,11,14,11);Controls.Add(table);
            table.Controls.Add(Ui.Label(L.T("预检与导出"),Ui.Heading),0,0);
            Ui.LoadChoices(mode,new[]{"可复制测试 Mod","复制用更新包"},"可复制测试 Mod");
            id.Text=m.Type==null?"political_editor_qa":m.Type.Node.Key+"_qa";
            folder.Text=project.Pathname==null?Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"Political Editor Exports"):Path.GetDirectoryName(project.Pathname);
            var top=Ui.Flow();top.Dock=DockStyle.Fill;top.Controls.Add(Ui.Label(L.T("模式")));top.Controls.Add(mode);top.Controls.Add(Ui.Label(L.T("输出名")));top.Controls.Add(id);
            top.Controls.Add(snapshot);top.Controls.Add(Ui.Button(L.T("源差异…"),(s,e)=>Conflicts(),false,84));Ui.FitRow(table,top,1);
            var line=Ui.Flow();line.Dock=DockStyle.Fill;line.Controls.Add(Ui.Label(L.T("保存位置")));line.Controls.Add(folder);
            line.Controls.Add(Ui.Button(L.T("浏览…"),(s,e)=>{string d=Ui.Folder(this,L.T("选择独立输出目录"),folder.Text);if(d!=null)folder.Text=d;}));Ui.FitRow(table,line,2);
            var split=new SplitContainer{Size=new Size(1015,460),SplitterDistance=349,Dock=DockStyle.Fill,SplitterWidth=6,BackColor=Ui.Light};
            Ui.Fill(split.Panel1,listing);Ui.Fill(split.Panel2,detail);table.Controls.Add(split,0,3);
            listing.SelectedIndexChanged+=(s,e)=>Detail();
            var actions=Ui.Flow();actions.Dock=DockStyle.Fill;actions.Padding=new Padding(0,8,0,0);
            plan=Ui.Button(L.T("重新预检"),async(s,e)=>await Check(),false,94);
            write=Ui.Button(L.T("生成内容包"),async(s,e)=>await Write(),true,112);
            show=Ui.Button(L.T("打开输出文件夹"),(s,e)=>{try{Ui.OpenFolder(project.Data.ExportFolder);}catch(Exception ex){Ui.Error(this,ex);}},false,138);
            fix=Ui.Button(L.T("定位并修复…"),(s,e)=>Fix(),false,111);
            original=Ui.Button(L.T("原始校验信息"),(s,e)=>{
                var problem=listing.SelectedItem as Problem;
                if(problem!=null&&!String.IsNullOrEmpty(problem.RawDetail))detail.Text=problem.File+"\r\n\r\n"+problem.Message+"\r\n\r\n"+problem.RawDetail;
            },false,115);
            actions.Controls.Add(plan);actions.Controls.Add(write);actions.Controls.Add(show);actions.Controls.Add(fix);actions.Controls.Add(original);
            actions.Controls.Add(Ui.Button(L.T("关闭"),(s,e)=>Close()));Ui.FitRow(table,actions,4);
            write.Enabled=show.Enabled=fix.Enabled=original.Enabled=false;
            listing.DoubleClick+=(s,e)=>{if(listing.SelectedItem is Problem)Fix();};
            foreach(var edit in new[]{folder,id})edit.TextChanged+=(s,e)=>Stale();
            mode.SelectedIndexChanged+=(s,e)=>Stale();snapshot.CheckedChanged+=(s,e)=>Stale();
            Shown+=async(s,e)=>await Check();FormClosing+=(s,e)=> {if(writing)e.Cancel=true;else if(show.Enabled)DialogResult=DialogResult.OK; };
        }
        private void Stale(){current=null;write.Enabled=fix.Enabled=original.Enabled=false;}
        private async Task Check() {
            if(computing||writing)return;
            computing=true;UseWaitCursor=true;write.Enabled=false;plan.Enabled=false;
            string dest=folder.Text.Trim(),key=id.Text.Trim();bool full=mode.SelectedIndex!=1,ignore=snapshot.Checked;
            try {
                var result=await Task.Run(()=>new ExportPlan(project,model,dest,key,full,Program.Temporary,ignore));
                if(IsDisposed||dest!=folder.Text.Trim()||key!=id.Text.Trim()||full!=(mode.SelectedIndex!=1)||ignore!=snapshot.Checked)return;
                current=result;listing.Items.Clear();listing.Items.Add(L.T("检查结果"));
                listing.Items.AddRange(current.Problems.Where(p=>p.Error).Cast<object>().ToArray());
                listing.Items.AddRange(current.Content.Keys.OrderBy(k=>k,Files.Windows).Cast<object>().ToArray());listing.SelectedIndex=0;
                write.Enabled=!current.Problems.Any(e=>e.Error);
            } catch(Exception e){if(!IsDisposed){current=null;listing.Items.Clear();detail.Text=e.Message;}}
            finally{computing=false;if(!IsDisposed){UseWaitCursor=false;plan.Enabled=true;}}
        }
        private void Detail() {
            if(current==null||listing.SelectedItem==null)return;
            string key=listing.SelectedItem.ToString();
            var problem=listing.SelectedItem as Problem;
            fix.Enabled=problem!=null||key==L.T("检查结果")&&current.Problems.Any(p=>p.Error&&p.File!="");
            original.Enabled=problem!=null&&!String.IsNullOrEmpty(problem.RawDetail);
            if(problem!=null) {
                detail.WordWrap=true;detail.ScrollBars=ScrollBars.Vertical;
                detail.Text=problem.File+"\r\n\r\n"+problem.Message+
                    (problem.Binding==null?"":"\r\n\r\n"+L.T("相关绑定 ")+problem.Binding);
            } else if(key==L.T("检查结果")) {
                detail.WordWrap=true;detail.ScrollBars=ScrollBars.Vertical;
                var problems=current.Problems;
                detail.Text=current.Target+"\r\n"+current.Content.Count+L.T(" 个内容文件\r\n")+
                    (problems.Any(p=>p.Error)?L.T("请修复后重新预检。"):String.Format(L.T("通过编辑器与 {0} 原生内容预检。"),NativeValidation.Version))+
                    "\r\n"+String.Join("\r\n",problems.Select(p=>p.File+"  "+p));
                Version installed;
                if(Editing.HasManualOffices(model)&&(!Version.TryParse(model.Version,out installed)||installed<new Version(NativeValidation.Version)))
                    detail.AppendText("\r\n"+String.Format(L.T("手动职位叠放与跨排位置需要框架 {0} 或更高版本。当前游戏目录检测到 {1}。"),NativeValidation.Version,model.Version));
            } else {
                detail.WordWrap=false;detail.ScrollBars=ScrollBars.Both;
                try{detail.Text=current.Diff(key);}catch(Exception e){detail.Text=e.Message;Program.Log(e);}
            }
        }
        private void Fix() {
            if(current==null)return;
            SelectedProblem=listing.SelectedItem as Problem??current.Problems.FirstOrDefault(p=>p.Error&&p.File!="");
            if(SelectedProblem!=null){DialogResult=DialogResult.Retry;Close();}
        }
        private void Conflicts(){try {
            var paths=project.SourceConflicts().ToArray();
            string[] options=paths.Length==0?new[]{L.T("当前源文件一致")}:paths;
            using(var diff=new DiffDialog(L.T("外部源文件变化"),options,k=>{
                if(k==L.T("当前源文件一致"))return L.T("未发现导入基线发生变化。");
                string actual=Files.At(project.Data.Roots.Mod,k);
                if(!project.Content.ContainsKey(k))return L.T("导入后新增的源文件。当前草稿没有该文件。\r\n\r\n")+k;
                return File.Exists(actual)?EditorDiff.Create(k,Files.Read(actual),project.Content[k].Data):L.T("来源文件已不存在。\r\n\r\n")+EditorDiff.Create(k,null,project.Content[k].Data);
            },L.T("确认后可采用项目原始快照生成隔离输出。"),false))diff.ShowDialog(this);
        }catch(Exception e){Ui.Error(this,e);}}
        private async Task Write() {
            if(current==null||writing)return;
            var request=current;writing=true;plan.Enabled=write.Enabled=false;UseWaitCursor=true;
            try {
                string target=await Task.Run(()=>request.Write(Program.Temporary));
                show.Enabled=true;detail.Text=L.T("已生成  ")+target+"\r\n\r\n"+(request.Full?L.T("从 mod 文件夹中复制同名文件夹和 .mod 到游戏用户目录。"):L.T("按 files 下的相对路径核对并复制。"))+
                    L.T("\r\n详情见 INSTALL.txt。");
            } catch(Exception e){Ui.Error(this,e);}
            finally{writing=false;plan.Enabled=true;UseWaitCursor=false;}
        }
    }

    internal sealed class QaDialog : Form {
        private readonly Project project;
        private readonly PreviewSettings preview;
        private readonly ComboBox sessions=Ui.Combo(330), which=Ui.Combo(178), outcome=Ui.Combo(113), exit=Ui.Combo(105);
        private readonly TextBox mods=Ui.Box("",285), notes=new TextBox{Dock=DockStyle.Fill,Multiline=true,ScrollBars=ScrollBars.Vertical},
            evidence=Ui.Box("",310), log=Ui.View("",true);
        private readonly Label how=Ui.Label("",Ui.Small,Ui.Soft);
        private QaSession active;
        private bool loading;
        internal QaDialog(Project p,PreviewSettings view) {
            project=p;preview=view;Ui.Dialog(this,L.T("一轮实机 QA"),new Size(1080,735));
            var table=Ui.Stack(45,43,45,69,79,40,0,44);table.Padding=new Padding(14,11,14,8);Controls.Add(table);
            table.Controls.Add(Ui.Label(L.T("按导出指纹记录实机验收"),Ui.Heading),0,0);
            table.Controls.Add(Row(Ui.Label(L.T("启用 Mod")),mods,Ui.Button(L.T("开始一轮"),(s,e)=>Start(),true,107),Ui.Label(L.T("会话")),sessions),0,1);
            table.Controls.Add(Row(Ui.Label(L.T("用例")),which,Ui.Label(L.T("观察结果")),outcome,Ui.Label(L.T("退出")),exit,Ui.Button(L.T("附截图…"),(s,e)=>Attach(),false,84)),0,2);
            how.Dock=DockStyle.Fill;how.AutoSize=false;table.Controls.Add(how,0,3);
            notes.Font=Ui.TextFont;table.Controls.Add(notes,0,4);
            table.Controls.Add(Row(Ui.Label(L.T("证据路径")),evidence,Ui.Button(L.T("保存本项"),(s,e)=>Store(),false,93),
                Ui.Button(L.T("读取本轮日志"),(s,e)=>Collect(),false,112),Ui.Button(L.T("导入日志…"),(s,e)=>Manual(),false,102)),0,5);
            table.Controls.Add(log,0,6);
            table.Controls.Add(Row(Ui.Button(L.T("导出本轮记录…"),(s,e)=>ExportEvidence(),false,137),Ui.Button(L.T("关闭"),(s,e)=>Close())),0,7);
            Ui.LoadChoices(which,QaEvidence.Cases,null);Ui.LoadChoices(outcome,new[]{"待实测","通过","失败","待复验"},null);Ui.LoadChoices(exit,new[]{"待核验","正常退出","异常退出","未知"},null);
            sessions.SelectedIndexChanged+=(s,e)=> {if(!loading)ShowSession();};
            which.SelectedIndexChanged+=(s,e)=>ViewCase();
            FillSessions();
        }
        private static FlowLayoutPanel Row(params Control[] controls){var row=Ui.Flow();row.Dock=DockStyle.Fill;row.Padding=new Padding(0,6,0,0);row.Controls.AddRange(controls);return row;}
        private void FillSessions() {
            var entries=project.Data.Qa.Sessions.Select((s,i)=>(i+1)+"  ·  "+s.StartUtc+"  ·  "+s.Country+"  "+s.Scale).ToArray();
            loading=true;Ui.Load(sessions,entries,entries.LastOrDefault());loading=false;ShowSession();
        }
        private void ShowSession() {
            int i=sessions.SelectedIndex;
            active=i<0?null:project.Data.Qa.Sessions[i];ViewCase();Display();
        }
        private void ViewCase() {
            int i=which.SelectedIndex;if(i<0)return;
            how.Text=L.T(QaEvidence.How[i]);
            var item=active==null?null:active.Results.FirstOrDefault(q=>q.Id==QaEvidence.Cases[i].Substring(0,3));
            notes.Text=item==null?"":item.Note;evidence.Text=item==null?"":item.Evidence;
            Ui.Choose(outcome,item==null?"待实测":item.Outcome);Ui.Choose(exit,active==null?"待核验":active.Exit);
        }
        private void Display() {
            if(active==null){ log.Text=L.T("请先导出内容包，再开始一轮。由你启动 Steam 和单人战役。");return; }
            string collected=String.Join("\r\n",active.Logs);
            log.Text=L.T("导出  ")+active.Fingerprint+"\r\nEU4  "+active.GameVersion+" / Political Framework "+active.RuntimeVersion+L.T("\r\n实例  ")+active.Instance+" · "+active.Country+
                " · "+active.Resolution+" · "+active.Scale+L.T("\r\n开始  ")+active.StartUtc+L.T("\r\n启用 Mod  ")+active.EnabledMods+"\r\n\r\n"+
                QaEvidence.Trace(collected)+L.T("\r\n\r\n本轮原始片段\r\n")+collected;
        }
        private void Start(){try{QaEvidence.Start(project,preview,mods.Text);FillSessions();}catch(Exception e){Ui.Error(this,e);}}
        private void Store() {
            if(active==null||which.SelectedIndex<0)return;
            var item=active.Results[which.SelectedIndex];item.Outcome=Ui.Selected(outcome);item.Note=notes.Text;item.Evidence=evidence.Text;
            active.Exit=Ui.Selected(exit);active.EndUtc=DateTime.UtcNow.ToString("o");project.Dirty=true;Display();
        }
        private void Attach(){string f=Ui.PickFile(this,L.T("截图 (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp"));if(f!=null)evidence.Text=f;}
        private void Collect(){if(active==null)return;try{
            active.Logs.Add(QaEvidence.Collect(active));project.Dirty=true;Display();
        }catch(Exception e){Ui.Error(this,e);}}
        private void Manual(){if(active==null)return;try{
            string source=Ui.PickFile(this,L.T("日志或报告 (*.log;*.txt)|*.log;*.txt"));
            if(source!=null){active.Logs.Add(L.T("手动附加  ")+source+"\r\n"+TextFile.Decode(Files.Read(source,1024*1024)).Text);project.Dirty=true;Display();}
        }catch(Exception e){Ui.Error(this,e);}}
        private void ExportEvidence(){if(active==null)return;try {
            using(var dialog=new SaveFileDialog {Filter=L.T("QA 记录 (*.json)|*.json"),FileName="Political-Editor-QA.json"})if(dialog.ShowDialog(this)==DialogResult.OK){
                project.ProtectOutput(dialog.FileName);Files.Atomic(dialog.FileName,TextFile.Utf8(Files.Json().Serialize(active)));
            }
        }catch(Exception e){Ui.Error(this,e);}}
    }
}
