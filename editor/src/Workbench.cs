using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using UiTimer = System.Windows.Forms.Timer;

namespace PoliticalEditor {
    internal sealed class WorkbenchSession {
        internal Project Project;
        internal Model LastGood;
        internal PreviewSettings Preview;
        internal Dictionary<int,string> Files;
        internal Rectangle Bounds;
        internal FormWindowState WindowState;
        internal string Language, Control, File, Subwindow, Zoom;
        internal int Tab, Caret, Selection, SubwindowOffset;
        internal bool PanelVisible, Shared;
    }

    internal sealed class Workbench : Form {
        private readonly TableLayoutPanel page;
        private readonly Panel body = new Panel { Dock=DockStyle.Fill,Margin=Padding.Empty };
        private readonly ToolStripStatusLabel state, revision, environment;
        private readonly ToolTip tips = new ToolTip();
        private readonly Button build, save, undo, redo, test, add, export, qa, reference;
        private Button remove, bindings, find, interfaceLanguage, officeLayout, compactScores;
        private readonly ContextMenuStrip languageMenu = new ContextMenuStrip();
        private readonly ComboBox type=Ui.Combo(173), instance=Ui.Combo(171), language=Ui.Combo(96), resolution=Ui.Combo(102), zoom=Ui.Combo(72), scale=Ui.Combo(74);
        private readonly ComboBox control=Ui.Combo(238), file=Ui.Combo(290);
        private readonly TextBox country=Ui.Box("ASK",49), date=Ui.Box("1444.1.1",123), x=Ui.Box("",56),y=Ui.Box("",56);
        private readonly CheckBox shared = new CheckBox { Text=L.T("编辑共享布局"),AutoSize=true,Margin=new Padding(7,7,3,0) };
        private readonly CheckBox found = new CheckBox { Text=L.T("已发现"),AutoSize=true,Checked=true,Margin=new Padding(7,7,3,0) };
        private readonly CheckBox moveOfficeSeat = new CheckBox { Text=L.T("整组移动席位"),AutoSize=true,Margin=new Padding(7,7,3,0) };
        private readonly ComboBox entryState = Ui.Combo(91);
        private readonly Label pathLabel=Ui.Label("",Ui.Small,Ui.Soft), statusBanner=Ui.Label("",Ui.Small,Ui.Soft), selectedNote=Ui.Label("",Ui.Small,Ui.Soft);
        private readonly ListBox problems=new ListBox { Dock=DockStyle.Fill, IntegralHeight=false,HorizontalScrollbar=true,Font=Ui.Small,BorderStyle=BorderStyle.None };
        private readonly TabControl tabs=new TabControl { Dock=DockStyle.Fill,Padding=new Point(14,6),Font=Ui.TextFont,ShowToolTips=true,Multiline=true };
        private readonly RichTextBox editor=new RichTextBox { Dock=DockStyle.Fill,Font=Ui.CodeFont,BackColor=Color.FromArgb(27,39,50),ForeColor=Color.FromArgb(221,230,237),
            BorderStyle=BorderStyle.None,AcceptsTab=true,WordWrap=false,DetectUrls=false,HideSelection=false,ScrollBars=RichTextBoxScrollBars.Both,Margin=Padding.Empty };
        private readonly Dictionary<int,string> overrides=new Dictionary<int,string>();
        private readonly UiTimer debounce=new UiTimer{Interval=260}, recovery=new UiTimer{Interval=16000};
        private readonly PreviewCanvas canvas=new PreviewCanvas { Dock=DockStyle.Fill,Margin=Padding.Empty };
        private SplitContainer split;
        private Control landing, workspace;
        private Project project;
        private Model model, lastGood;
        private Scene scene;
        private PreviewSettings preview=new PreviewSettings();
        private FindReplaceDialog search;
        internal WorkbenchSession NextWindow { get; private set; }
        private CancellationTokenSource parsing;
        private int parseSequence;
        private string openFile="", lastRecovery="";
        private bool controlsLoading, editing, parsingNow, opening, bufferDirty, queuedClose;
        private int initialWidth=1290;

        internal Workbench() {
            Text=L.T("EU4 政治体系编辑器");
            ClientSize=new Size(initialWidth,845);MinimumSize=new Size(1050,730);StartPosition=FormStartPosition.CenterScreen;
            AutoScaleMode=AutoScaleMode.Dpi;Font=Ui.TextFont;BackColor=Ui.Paper;ForeColor=Ui.Ink;KeyPreview=true;
            page=Ui.Stack(46,46,0,27);Controls.Add(page);
            var title=Ui.Flow();title.Dock=DockStyle.Fill;title.Padding=new Padding(13,7,8,0);
            title.Controls.Add(Ui.Label(L.T("EU4 政治体系编辑器"),Ui.Heading));
            title.Controls.Add(new Label {Text=L.T("Windows  /  离线工作台"),AutoSize=true,ForeColor=Ui.Soft,Margin=new Padding(10,12,0,0) });
            page.Controls.Add(title,0,0);
            var toolbar=Ui.Flow();toolbar.Dock=DockStyle.Fill;toolbar.Padding=new Padding(13,4,8,0);
            toolbar.Controls.Add(Ui.Button(L.T("新建…"),async(s,e)=>await NewDefault(),false,74));
            build=Ui.Button(L.T("从模板…"),async(s,e)=>await NewTemplate(),false,91);toolbar.Controls.Add(build);
            toolbar.Controls.Add(Ui.Button(L.T("打开 Mod…"),async(s,e)=>await ImportMod(),false,100));
            toolbar.Controls.Add(Ui.Button(L.T("打开项目…"),async(s,e)=>await OpenProject(),false,100));
            save=Ui.Button(L.T("保存项目"),(s,e)=>Save(),false,88);toolbar.Controls.Add(save);
            undo=Ui.Button(L.T("撤销"),(s,e)=>Undo(false),false,52);toolbar.Controls.Add(undo);
            redo=Ui.Button(L.T("重做"),(s,e)=>Undo(true),false,52);toolbar.Controls.Add(redo);
            add=Ui.Button(L.T("添加组件…"),(s,e)=>Add(),false,94);toolbar.Controls.Add(add);
            test=Ui.Button(L.T("离线试玩"),(s,e)=>Play(),false,83);toolbar.Controls.Add(test);
            qa=Ui.Button(L.T("QA 记录"),(s,e)=>Qa(),false,72);toolbar.Controls.Add(qa);
            export=Ui.Button(L.T("预检与导出…"),(s,e)=>Export(),true,110);toolbar.Controls.Add(export);
            interfaceLanguage=Ui.Button(L.T("语言…"),(s,e)=>ShowLanguages(),false,78);
            interfaceLanguage.Disposed+=(s,e)=>languageMenu.Dispose();
            foreach(var item in new[]{Tuple.Create("简体中文",L.Chinese),Tuple.Create("English",L.English)}) {
                var target=item.Item2;
                var entry=new ToolStripMenuItem(item.Item1){Checked=target==L.Current,Tag=target};
                // Let the dropdown finish handling the click before closing its owner.
                entry.Click+=(s,e)=>BeginInvoke((Action)(()=>SwitchLanguage(target)));
                languageMenu.Items.Add(entry);
            }
            Ui.Tip(interfaceLanguage,interfaceLanguage.Text+"\n"+L.T("更改本程序的界面语言"));
            toolbar.Controls.Add(interfaceLanguage);
            Ui.FitRow(page,toolbar,1);page.Controls.Add(body,0,2);
            var footer=new StatusStrip { SizingGrip=false,Dock=DockStyle.Fill,BackColor=Ui.Light,Padding=new Padding(9,0,3,0),Font=Ui.Small,ShowItemToolTips=true };
            state=new ToolStripStatusLabel(L.T("就绪")) { Spring=true,TextAlign=ContentAlignment.MiddleLeft };revision=new ToolStripStatusLabel();
            environment=new ToolStripStatusLabel(L.T("编辑器 ")+Program.Version+L.T("  ·  适配 ")+NativeValidation.Version);
            foreach(var item in new[]{state,revision,environment})item.TextChanged+=(s,e)=>item.ToolTipText=item.Text;
            footer.Items.AddRange(new ToolStripItem[]{state,revision,environment});page.Controls.Add(footer,0,3);
            reference=Ui.Button(L.T("语法参考"),(s,e)=>Reference(),false,83);
            landing=Landing();workspace=Workspace();
            body.Controls.Add(workspace);body.Controls.Add(landing);workspace.Hide();
            debounce.Tick+=(s,e)=>{debounce.Stop();Flush(true);};
            recovery.Tick+=(s,e)=>Recover();
            recovery.Start();
            FormClosing+=CloseProject;
            KeyDown+=KeysDown;
            Buttons();
        }
        private Control Landing() {
            var host=new Panel {Dock=DockStyle.Fill,BackColor=Ui.Light,Padding=new Padding(36,28,36,30)};
            var shell=Ui.Stack(64,54,212,75,0);shell.BackColor=Ui.Light;host.Controls.Add(shell);
            shell.Controls.Add(Ui.Label(L.T("制作自己的政治体系"),Ui.Big),0,0);
            shell.Controls.Add(Ui.Label(L.T("在工作副本里编辑类型、历史和原生界面。确认差异后，生成供你复制进游戏的内容包。"),null,Ui.Soft),0,1);
            var cards=new TableLayoutPanel { BackColor=Ui.Light,Dock=DockStyle.Fill,ColumnCount=3,RowCount=1,Margin=Padding.Empty };
            for(int n=0;n<3;++n)cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100f/3f));
            cards.Controls.Add(Card(L.T("从默认版新建"),L.T("从一套可验证的天朝布局、入口、资源和历史实例起步。"), L.T("新建项目"),async(s,e)=>await NewDefault()),0,0);
            cards.Controls.Add(Card(L.T("读取已有 Mod"),L.T("选定现有体系，检查继承、GUI、GFX 和贴图来源。也可从已有定义新建。"),L.T("打开 Mod"),async(s,e)=>await ImportMod()),1,0);
            cards.Controls.Add(Card(L.T("继续工作"),L.T("打开保存的 .pfproject.json，恢复草稿、数据来源和 QA 记录。"),L.T("打开项目"),async(s,e)=>await OpenProject()),2,0);
            shell.Controls.Add(cards,0,2);
            var bar=Ui.Flow();bar.Dock=DockStyle.Fill;bar.BackColor=Ui.Light;bar.Padding=new Padding(0,24,0,0);
            bar.Controls.Add(Ui.Button(L.T("打开幕府练习"),async(s,e)=>await Practice(),false,132));
            bar.Controls.Add(Ui.Button(L.T("恢复自动备份"),async(s,e)=>await Restore(),false,132));shell.Controls.Add(bar,0,3);
            return host;
        }
        private Control Card(string title,string detail,string action,EventHandler click) {
            var card=Ui.Stack(51,57,51,0);card.Margin=new Padding(0,0,14,0);card.Padding=new Padding(16,13,12,10);
            card.Controls.Add(Ui.Label(title,Ui.Heading),0,0);
            card.Controls.Add(new Label {Text=detail,Dock=DockStyle.Fill,ForeColor=Ui.Soft,Padding=new Padding(2,0,1,0)},0,1);
            card.Controls.Add(Ui.Button(action,click,true,109),0,2);return card;
        }
        private Control Workspace() {
            var shell=Ui.Stack(42,0,91);shell.BackColor=Ui.Light;
            var context=Ui.Flow();context.Dock=DockStyle.Fill;context.Padding=new Padding(13,4,5,0);
            foreach(var c in new Control[] {Ui.Label(L.T("体系")),type,Ui.Label(L.T("实例")),instance,Ui.Label(L.T("国家")),country,Ui.Label(L.T("历史日")),date,Ui.Label(L.T("游戏文本")),language})context.Controls.Add(c);
            context.Controls.Add(Ui.Button(L.T("路径…"),(s,e)=>Map(),false,68));context.Controls.Add(reference);
            Ui.FitRow(shell,context,0);
            split=new SplitContainer {Dock=DockStyle.Fill,Orientation=Orientation.Vertical,Size=new Size(initialWidth,565),SplitterWidth=6,
                SplitterDistance=initialWidth/2,BackColor=Ui.Line,Panel1MinSize=359,Panel2MinSize=355,Margin=Padding.Empty };
            shell.Controls.Add(split,0,1);
            var left=Ui.Stack(38,0,37,42);left.BackColor=Ui.Light;
            var view=Ui.Flow();view.Dock=DockStyle.Fill;view.Padding=new Padding(9,3,0,0);
            foreach(var c in new Control[]{Ui.Label(L.T("分辨率")),resolution,Ui.Label(L.T("UI 比例")),scale,Ui.Label(L.T("画布")),zoom,Ui.Button(L.T("命中详情"),(s,e)=>Inspect(),false,78)})view.Controls.Add(c);
            Ui.FitRow(left,view,0);left.Controls.Add(canvas,0,1);
            var inspect=Ui.Flow(true);inspect.Dock=DockStyle.Fill;inspect.Padding=new Padding(9,4,0,0);
            inspect.Controls.Add(control);inspect.Controls.Add(Ui.Label("X"));inspect.Controls.Add(x);inspect.Controls.Add(Ui.Label("Y"));inspect.Controls.Add(y);
            inspect.Controls.Add(Ui.Button(L.T("应用坐标"),(s,e)=>ApplyXY(),false,76));
            remove=Ui.Button(L.T("删除组件…"),(s,e)=>DeleteSelected(),false,94);
            inspect.Controls.Add(remove);Ui.FitRow(left,inspect,2);
            var menu=new ContextMenuStrip();
            var removeMenu=menu.Items.Add(L.T("删除选中的组件…"),null,(s,e)=>DeleteSelected());
            var bindingMenu=menu.Items.Add(L.T("绑定与修复…"),null,(s,e)=>OpenBindings());
            menu.Opening+=(s,e)=> { removeMenu.Enabled=remove.Enabled;bindingMenu.Enabled=bindings.Enabled; };
            canvas.ContextMenuStrip=menu;
            canvas.MouseDown+=(s,e)=> {
                if(e.Button==MouseButtons.Right&&!preview.Playing) {
                    var item=canvas.HitAt(canvas.Local(e.Location),true);
                    if(item!=null)Selected(item,true);
                }
            };
            var options=Ui.Flow();options.Dock=DockStyle.Fill;options.Padding=new Padding(9,3,0,0);
            options.Controls.Add(shared);options.Controls.Add(Ui.Button(L.T("改手动…"),(s,e)=>Manual(),false,74));options.Controls.Add(found);options.Controls.Add(entryState);
            officeLayout=Ui.Button(L.T("排布选举区…"),(s,e)=>ArrangeOffices(),false,115);
            Ui.Tip(officeLayout,officeLayout.Text+"\n"+L.T("调整选举列表的位置、宽度和高度，同时排布盾徽与评分。"));
            options.Controls.Add(officeLayout);
            compactScores=Ui.Button(L.T("评分贴右下…"),(s,e)=>CompactScores(),false,112);
            Ui.Tip(compactScores,compactScores.Text+"\n"+L.T("把评分和悬停区移到各自大盾徽的右下角，数字左对齐。"));
            options.Controls.Add(compactScores);
            options.Controls.Add(moveOfficeSeat);
            Ui.Tip(moveOfficeSeat,L.T("拖大盾徽时同时移动本席位的评分、悬停区、小盾徽和职位文字。取消勾选可单独移动大盾徽。"));
            moveOfficeSeat.CheckedChanged+=(s,e)=>Selected(canvas.Selected,false);
            selectedNote.AutoEllipsis=true;selectedNote.MaximumSize=new Size(248,41);options.Controls.Add(selectedNote);
            Ui.Tip(shared,shared.Text);Ui.Tip(found,L.T("已发现领袖首都"));
            Ui.FitRow(left,options,3);Ui.Fill(split.Panel1,left);
            var right=Ui.Stack(39,0,25);
            var source=Ui.Flow();source.Dock=DockStyle.Fill;source.Padding=new Padding(8,4,0,0);source.Controls.Add(file);
            source.Controls.Add(Ui.Button(L.T("来源"),(s,e)=>Sources(),false,65));source.Controls.Add(Ui.Button(L.T("差异"),(s,e)=>DocumentDiff(),false,63));
            find=Ui.Button(L.T("查找替换…"),(s,e)=>OpenSearch(false),false,104);
            Ui.Tip(find,find.Text+"\n"+L.T("在当前文件查找或替换")+" · Ctrl+F / Ctrl+H");
            source.Controls.Add(find);
            Ui.FitRow(right,source,0);
            var captions=new[]{L.T("类型：规则、入口、资源和行动"),L.T("布局：面板区域和控件绑定"),L.T("GUI：原生面板与控件位置"),L.T("GFX：图标名字与贴图"),L.T("历史：开局领袖、成员及职位"),L.T("本地化：标题、说明及提示文字")};
            int tabIndex=0;
            foreach (string header in new[]{L.T("类型"),L.T("布局"),"GUI","GFX",L.T("历史"),L.T("本地化")})
                tabs.TabPages.Add(new TabPage(header){BackColor=Ui.Light,ToolTipText=captions[tabIndex++]});
            tabs.TabPages[0].Controls.Add(editor);
            right.Controls.Add(tabs,0,1);pathLabel.MaximumSize=new Size(10000,24);right.Controls.Add(pathLabel,0,2);Ui.Fill(split.Panel2,right);
            var messages=Ui.Stack(33,0);messages.Padding=new Padding(13,1,13,3);
            var check=Ui.Flow();check.Dock=DockStyle.Fill;
            bindings=Ui.Button(L.T("绑定与修复…"),(s,e)=>OpenBindings(),false,112);
            Ui.Tip(bindings,bindings.Text+"\n"+L.T("查看绑定用途，移除显示、改绑或修复缺失控件。"));
            check.Controls.Add(bindings);check.Controls.Add(statusBanner);
            Ui.FitRow(messages,check,0);messages.Controls.Add(problems,0,1);shell.Controls.Add(messages,0,2);
            Ui.Load(resolution,new[]{"1920×1080","2560×1440","1440×900","1280×720"},"1920×1080");
            Ui.Load(scale,new[]{"100%","115%","130%","150%"},"100%");
            Ui.Load(zoom,new[]{"75%","90%","100%","120%","150%"},"100%");
            Ui.LoadChoices(entryState,new[]{"可见可点","可见置灰","隐藏"},"可见可点");
            foreach(var picker in new[]{resolution,scale,zoom,entryState,language})Ui.FitChoices(picker);
            foreach(var combo in new[]{resolution,scale,zoom,entryState})combo.SelectedIndexChanged+=(s,e)=>SetupPreview();
            found.CheckedChanged+=(s,e)=>SetupPreview();
            type.SelectedIndexChanged+=(s,e)=> { if(!controlsLoading&&project!=null){Flush(false);project.Data.Type=Ui.Selected(type);project.Data.Instance="";preview.Offsets.Clear();preview.Passed.Clear();overrides.Clear();project.Touch();Rebuild();} };
            instance.SelectedIndexChanged+=(s,e)=> { if(!controlsLoading&&project!=null){Flush(false);ChangeInstance(instance.SelectedIndex);}};
            language.SelectedIndexChanged+=(s,e)=> { if(!controlsLoading&&project!=null){Flush(false);project.Data.Language=Ui.Selected(language);overrides.Remove(5);project.Touch();Rebuild();}};
            country.Validated+=(s,e)=>Scenario();date.Validated+=(s,e)=>Scenario();
            country.KeyDown+=(s,e)=>{if(e.KeyCode==System.Windows.Forms.Keys.Enter)Scenario();};
            date.KeyDown+=(s,e)=>{if(e.KeyCode==System.Windows.Forms.Keys.Enter)Scenario();};
            editor.TextChanged+=(s,e)=>{if(!editing){bufferDirty=true;debounce.Stop();debounce.Start();state.Text=L.T("草稿等待解析");}};
            tabs.SelectedIndexChanged+=(s,e)=>{if(!controlsLoading){Flush(true);tabs.SelectedTab.Controls.Add(editor);FillFiles();}};
            file.SelectedIndexChanged+=(s,e)=>{if(!controlsLoading&&file.SelectedItem!=null){Flush(true);overrides[tabs.SelectedIndex]=Ui.Selected(file);LoadEditor(Ui.Selected(file));}};
            problems.DoubleClick+=(s,e)=>ProblemJump();
            control.SelectedIndexChanged+=(s,e)=>{if(!controlsLoading&&control.SelectedItem is VisualItem)Selected((VisualItem)control.SelectedItem,true);};
            canvas.SelectItem+=item=>Selected(item,true);
            canvas.MoveItem+=(item,p)=>Do(()=>MoveVisual(item,p));
            canvas.ScrollList+=(key,value)=>{preview.Offsets[key]=value;RefreshScene(false);};
            canvas.PickInstance+=ChangeInstance;
            canvas.ActivateItem+=Activate;
            canvas.Tell+=message=>state.Text=message;
            return shell;
        }
        private void Do(Action action) { try { action(); } catch(Exception e){Ui.Error(this,e);} }
        private async Task LoadProject(Func<Project> get) {
            if (opening) return;
            opening=true;Buttons();state.Text=L.T("正在读取项目");
            try {
                var p=await Task.Run(get);
                Set(p);
                await RebuildTask();
            } catch(Exception e){Ui.Error(this,e);}
            finally{opening=false;Buttons();}
        }
        private bool CanLeave() {
            Flush(false);
            if(project==null||!project.Dirty)return true;
            var result=MessageBox.Show(this,L.T("保存当前项目后再切换？"),"项目草稿",MessageBoxButtons.YesNoCancel,MessageBoxIcon.Question);
            if(result==DialogResult.Cancel)return false;
            return result==DialogResult.No||Save();
        }
        private async Task NewDefault() {
            if(opening||!CanLeave())return;
            using(var d=new NewDialog(null))if(d.ShowDialog(this)==DialogResult.OK) {
                RootMap roots=project==null?RootMap.Detect():project.Data.Roots;
                await LoadProject(()=>Editing.Default(roots,d.TypeId,d.InstanceId,d.Date,d.Country));
            }
        }
        private async Task Practice() {
            if(!CanLeave())return;
            await LoadProject(()=>Editing.Default(RootMap.Detect(),"shogun_system","shogun_system","1444.1.1","ASK",true));
        }
        private async Task NewTemplate() {
            if(project==null) {
                await Practice();
                if (project==null||model==null)return;
            }
            Flush(false);
            await RebuildTask();
            if(model==null||model.Types.Count==0)return;
            Do(()=> {
                using(var d=new NewDialog(model)) if(d.ShowDialog(this)==DialogResult.OK) {
                    var plan=Editing.FromTemplate(project,model,d.ParentId,d.LayoutId,d.TypeId,d.InstanceId,d.Date,d.Country,d.Copy);
                    if(Review(plan,L.T("确认后新增类型、历史和本地化草稿。"))) {
                        project.Data.Type=d.TypeId;project.Data.Instance=d.InstanceId;project.Data.Date=d.Date;project.Data.Country=d.Country;
                        overrides.Clear();Rebuild();
                    }
                }
            });
        }
        private async Task ImportMod() {
            if(opening||!CanLeave())return;
            using(var d=new RootDialog(project==null?RootMap.Detect():project.Data.Roots,true)) if(d.ShowDialog(this)==DialogResult.OK) {
                await LoadProject(()=>{
                    var p=Project.Import(d.Roots,CancellationToken.None);
                    p.Data.Title=Path.GetFileName(d.Roots.Mod);
                    if(p.Content.ContainsKey(Model.TypeDir+"shogun_system.txt"))p.Data.Type="shogun_system";
                    return p;
                });
            }
        }
        private async Task OpenProject(string chosen=null) {
            if(opening||!CanLeave())return;
            string path=chosen??Ui.PickFile(this,L.T("政治体系项目 (*.pfproject.json)|*.pfproject.json|JSON (*.json)|*.json"));
            if(path!=null)await LoadProject(()=>Project.Open(path));
        }
        private async Task Restore() {
            string path=Directory.Exists(Program.Recovery)?Directory.GetFiles(Program.Recovery,"*.pfproject.json").OrderByDescending(File.GetLastWriteTimeUtc).FirstOrDefault():null;
            if(path==null){state.Text=L.T("目前没有可恢复的草稿");return;}
            await OpenProject(path);
            if(project!=null&&Files.Windows.Equals(project.Pathname,path)){project.Pathname=null;project.SavedHash=null;project.Dirty=true;}
        }
        internal async Task OpenPath(string path) { await OpenProject(path); }
        internal void DebugOpen(Project p,Model m) { Set(p);Present(m); }
        private void Set(Project p) {
            if(parsing!=null)parsing.Cancel();
            project=p;model=lastGood=null;openFile="";bufferDirty=false;overrides.Clear();
            if(search!=null&&!search.IsDisposed)search.File("",false);
            preview=new PreviewSettings();canvas.PanelVisible=true;canvas.Subwindow="";canvas.Use(null,null);
            lastRecovery=Path.Combine(Program.Recovery,Guid.NewGuid().ToString("N")+".pfproject.json");
            landing.Hide();workspace.Show();Text=p.Data.Title+L.T("  —  EU4 政治体系编辑器");
        }
        private async void Rebuild() { await RebuildTask(); }
        private async Task RebuildTask() {
            if(project==null||IsDisposed)return;
            if(parsing!=null)parsing.Cancel();
            var source=new CancellationTokenSource();parsing=source;
            var p=project;var snapshot=p.Content;var roots=p.Data.Roots.Copy();
            string wanted=p.Data.Type,which=p.Data.Instance,tag=p.Data.Country,day=p.Data.Date,lang=p.Data.Language;
            var previous=lastGood;
            int ticket=++parseSequence;parsingNow=true;state.Text=L.T("解析草稿与素材来源");Buttons();
            try {
                var result=await Task.Run(()=>{
                    var mount=new Mount(roots,snapshot,source.Token);
                    Sprites cache=null;
                    if(previous!=null&&previous.Images!=null&&Files.Windows.Equals(previous.Mount.Roots.Game,roots.Game) &&
                       (previous.Mount.Roots.Dependencies??new string[0]).SequenceEqual(roots.Dependencies??new string[0],Files.Windows) &&
                       MatchGfx(previous.Mount.Own,snapshot))cache=previous.Images;
                    return new Model(mount,wanted,which,tag,day,lang,source.Token,cache);
                },source.Token);
                if(ticket==parseSequence&&ReferenceEquals(p,project)&&ReferenceEquals(snapshot,p.Content)&&!IsDisposed) {
                    Present(result);
                    if (canvas.Focused) canvas.Invalidate();
                }
            } catch(OperationCanceledException) { }
            catch(Exception e) {
                if(ticket==parseSequence&&!IsDisposed){Program.Log(e);statusBanner.Text=L.T("读取失败  ·  保留已解析预览");problems.Items.Clear();problems.Items.Add(new Problem(openFile,0,e.Message));state.Text=e.Message;}
            } finally {
                if(ticket==parseSequence&&!IsDisposed){parsingNow=false;Buttons();}
                if(ReferenceEquals(parsing,source))parsing=null;
                source.Dispose();
            }
        }
        private static bool MatchGfx(Dictionary<string,Entry>a,Dictionary<string,Entry>b) {
            var first=a.Where(p=>p.Key.EndsWith(".gfx")||p.Key.StartsWith("gfx/")||p.Key=="descriptor.mod").ToArray();
            var second=b.Count(p=>p.Key.EndsWith(".gfx")||p.Key.StartsWith("gfx/")||p.Key=="descriptor.mod");
            return first.Length==second&&first.All(p=>b.ContainsKey(p.Key)&&ReferenceEquals(p.Value.Data,b[p.Key].Data));
        }
        private void Present(Model result) {
            model=result;
            var selected=canvas.Selected==null?null:canvas.Selected.Name;
            var localProblems=result.Problems.GroupBy(p=>p.File+"\n"+p.Offset+"\n"+p.Message).Select(g=>g.First()).ToArray();
            bool usable=result.Window!=null&&result.Rule!=null&&!localProblems.Any(p=>p.Error);
            if(usable||lastGood==null&&result.Window!=null) {
                lastGood=result;
                RefreshScene(true,selected);
            }
            controlsLoading=true;
            try {
                Ui.Load(type,result.Types.Keys.OrderBy(k=>result.Types[k].Node.Get("abstract")=="yes").ThenBy(k=>k),result.Type==null?"":result.Type.Node.Key);
                var choices=result.Instances.Select(i=>i.Id+(i.Future?L.T("  · 尚未建立"):"")).ToArray();
                Ui.Load(instance,choices,result.Instance==null?"":result.Instance.Id+(result.Instance.Future?L.T("  · 尚未建立"):""));
                Ui.Load(language,Localisation.FrameworkHeaders.Contains(project.Data.Language)?Localisation.FrameworkHeaders:Localisation.FrameworkHeaders.Concat(new[]{project.Data.Language}),project.Data.Language);
                country.Text=project.Data.Country;date.Text=project.Data.Date;
                if (!bufferDirty&&result.Type!=null)project.Data.Type=result.Type.Node.Key;
                if (result.Instance!=null)project.Data.Instance=result.Instance.Id;
            } finally{controlsLoading=false;}
            statusBanner.Text=usable?localProblems.Length==0?L.T("检查结果  ·  未发现结构问题"):L.T("检查结果  ·  ")+localProblems.Length+L.T(" 条提示"):
                L.T("检查结果  ·  ")+localProblems.Count(p=>p.Error)+L.T(" 项错误  ·  保留最后一次可解析预览");
            problems.BeginUpdate();problems.Items.Clear();problems.Items.AddRange(localProblems.Cast<object>().ToArray());problems.EndUpdate();
            if(!bufferDirty)FillFiles();
            state.Text=usable?(result.Instance!=null&&result.Instance.SampleMembers?L.T("已读取  ·  成员样例仅用于离线预览"):L.T("已读取  ·  可编辑工作副本")):L.T("需要检查文件绑定");
            environment.Text=L.T("运行时 ")+result.Version+L.T("  ·  校验 ")+NativeValidation.Version;
            Buttons();
        }
        private void FillFiles() {
            if(project==null||model==null)return;
            int index=tabs.SelectedIndex;
            if(index<0)return;
            var paths=project.Content.Keys.Concat(model.Mount.Paths).Distinct(Files.Windows).Where(p=>TabMatch(index,p)).OrderBy(p=>p,Files.Windows).ToList();
            string chosen=model.TabPath(index), desired;
            if(chosen!=""&&!paths.Contains(chosen,Files.Windows))paths.Insert(0,chosen);
            if(overrides.TryGetValue(index,out desired)&&paths.Contains(desired,Files.Windows))chosen=desired;
            controlsLoading=true;try{Ui.Load(file,paths,chosen);}finally{controlsLoading=false;}
            LoadEditor(Ui.Selected(file));
        }
        private static bool TabMatch(int index,string p) {
            switch(index) {
                case 0:return p.StartsWith(Model.TypeDir)||p=="reference/political_systems/default.txt";
                case 1:return p.StartsWith(Model.LayoutDir);
                case 2:return p.EndsWith(".gui");
                case 3:return p.EndsWith(".gfx");
                case 4:return p.StartsWith(Model.HistoryDir)||p.StartsWith("history/countries/")||p.StartsWith("history/provinces/")||p=="common/political_systems/instances.txt";
                default:return p.StartsWith("localisation/")||p.StartsWith("common/political_systems/localisation/");
            }
        }
        private void LoadEditor(string path) {
            if(project==null||model==null||bufferDirty&&path==openFile)return;
            Entry entry;
            if(path=="")entry=null;
            else if(!project.Content.TryGetValue(path,out entry))entry=model.Mount.Get(path,true);
            if(path==openFile&&entry!=null&&editor.Text==Document.ViewText(TextFile.Decode(entry.Data).Text))return;
            editor.SuspendLayout();editing=true;
            int caret=path==openFile?editor.SelectionStart:0;
            try {
                openFile=path;
                editor.Text=entry==null?"":Document.ViewText(TextFile.Decode(entry.Data).Text);
                editor.SelectionStart=Math.Min(caret,editor.TextLength);editor.SelectionLength=0;
                editor.ReadOnly=entry==null;bufferDirty=false;
            } finally { editing=false;editor.ResumeLayout(); }
            pathLabel.Text=path==""?L.T("当前标签没有可编辑的文件"):path+"  ·  "+L.T(model.Mount.Source(path));
            tips.SetToolTip(pathLabel,pathLabel.Text);
            if(search!=null&&!search.IsDisposed)search.File(path,entry!=null);
            Buttons();
        }
        private void Flush(bool redraw) {
            if(!bufferDirty||project==null||openFile=="")return;
            debounce.Stop();
            try {
                Entry entry; if(!project.Content.TryGetValue(openFile,out entry)&&model!=null)entry=model.Mount.Get(openFile,true);
                string raw=entry==null?"":TextFile.Decode(entry.Data).Text;
                string changed=Document.MergeEditorText(raw,editor.Text);
                if(changed!=raw){
                    var draft=project.Draft();Project.PutText(draft,openFile,changed,entry);project.Apply(draft,L.T("编辑 ")+openFile);
                }
                bufferDirty=false;
                if(redraw)Rebuild();
            }catch(Exception e){Ui.Error(this,e);}
        }
        private void Buttons() {
            bool loaded=project!=null;
            build.Enabled=!opening&&!parsingNow;
            save.Enabled=loaded&&!opening;
            undo.Enabled=loaded&&project.CanUndo&&!opening;redo.Enabled=loaded&&project.CanRedo&&!opening;
            add.Enabled=loaded&&model!=null&&model.Window!=null&&!parsingNow&&!opening;
            if(remove!=null)remove.Enabled=add.Enabled&&!preview.Playing&&canvas.Selected!=null;
            if(bindings!=null)bindings.Enabled=loaded&&model!=null&&model.Window!=null&&!parsingNow&&!opening;
            if(find!=null)find.Enabled=loaded&&openFile!=""&&!opening;
            if(officeLayout!=null)officeLayout.Visible=loaded&&!preview.Playing&&Editing.HasElectionArea(model);
            if(officeLayout!=null)officeLayout.Enabled=officeLayout.Visible&&!parsingNow&&!opening;
            if(compactScores!=null) {
                compactScores.Visible=officeLayout.Visible;
                compactScores.Enabled=officeLayout.Enabled;
            }
            moveOfficeSeat.Visible=loaded&&!preview.Playing&&Editing.CanMoveOfficeSeat(model);
            if(interfaceLanguage!=null)interfaceLanguage.Enabled=!opening;
            test.Enabled=loaded&&lastGood!=null&&!opening;
            export.Enabled=loaded&&model!=null&&!parsingNow&&!opening;
            qa.Enabled=loaded&&!opening;reference.Enabled=loaded;
            revision.Text=loaded?L.T("修订 ")+project.Data.Revision+(project.Dirty?L.T("  · 未保存"):""):"";
        }
        private bool Save() {
            if(project==null)return true;
            Flush(false);
            if(bufferDirty)return false;
            try {
                string path=project.Pathname;
                if(path==null)using(var d=new SaveFileDialog {Filter=L.T("政治体系项目 (*.pfproject.json)|*.pfproject.json"),AddExtension=true,DefaultExt="pfproject.json",FileName=(project.Data.Type??"workspace")+".pfproject.json",OverwritePrompt=true}) {
                    if(d.ShowDialog(this)!=DialogResult.OK)return false;path=d.FileName;
                }
                project.Save(path);state.Text=L.T("已保存  ·  ")+Path.GetFileName(path);Buttons();return true;
            }catch(Exception e){Ui.Error(this,e);return false;}
        }
        private void Recover() {
            if(project==null||!project.Dirty||opening||lastRecovery=="")return;
            try{Flush(false);if(!bufferDirty)project.Save(lastRecovery,true);}catch(Exception e){Program.Log(e);state.Text=L.T("自动备份失败  ·  可尝试手动保存");}
        }
        private void CloseProject(object sender,FormClosingEventArgs e) {
            if(queuedClose)return;
            if(NextWindow==null&&!CanLeave()){e.Cancel=true;return;}
            queuedClose=true;if(parsing!=null)parsing.Cancel();debounce.Stop();recovery.Stop();tips.Dispose();
            if(search!=null&&!search.IsDisposed)search.Close();
        }
        private void Undo(bool forward) {
            Flush(false);if(project==null)return;
            string name=project.Undo(forward);
            if(name.Length==0)return;
            state.Text=(forward?L.T("已重做  "):L.T("已撤销  "))+name;
            Rebuild();
        }
        private void Map() {
            if(project==null)return;
            using(var d=new RootDialog(project.Data.Roots,false)) if(d.ShowDialog(this)==DialogResult.OK) {
                Flush(false);
                project.Data.Roots=d.Roots;project.Touch();lastGood=null;Rebuild();
            }
        }
        private void Scenario() {
            if(controlsLoading||project==null)return;
            string c=country.Text.Trim().ToUpperInvariant(),d=date.Text.Trim();
            if(c.Length!=3||!c.All(k=>k>='A'&&k<='Z'||k>='0'&&k<='9')||Model.DateKey(d)<0) {state.Text=L.T("请输入 3 位国家标签和有效历史日");return;}
            if(c==project.Data.Country&&d==project.Data.Date)return;
            Flush(false);project.Data.Country=c;project.Data.Date=d;preview.Offsets.Clear();preview.Passed.Clear();project.Touch();Rebuild();
        }
        private void ChangeInstance(int index) {
            if(model==null||index<0||index>=model.Instances.Count)return;
            string id=model.Instances[index].Id;
            if(project.Data.Instance==id)return;
            Flush(false);project.Data.Instance=id;preview.Offsets.Clear();preview.Passed.Clear();project.Touch();Rebuild();
        }
        private void SetupPreview() {
            if(controlsLoading)return;
            string res=Ui.Selected(resolution);var p=res.Split('×');
            if(p.Length==2)preview.Resolution=new Size(Document.Int(p[0],1920),Document.Int(p[1],1080));
            preview.Scale=Math.Max(50,Document.Int(Ui.Selected(scale).TrimEnd('%'),100))/100m;
            canvas.Zoom=Math.Max(50,Document.Int(Ui.Selected(zoom).TrimEnd('%'),100))/100m;
            preview.MapDiscovered=found.Checked;preview.Enabled=entryState.SelectedIndex!=1;preview.Visible=entryState.SelectedIndex!=2;
            if(preview.Playing&&(!preview.MapDiscovered||!preview.Visible)){canvas.PanelVisible=false;canvas.Subwindow="";}
            RefreshScene(false);
        }
        private void RefreshScene(bool populate,string name=null) {
            if(lastGood==null||lastGood.Window==null)return;
            name=name??(canvas.Selected==null?null:canvas.Selected.Name);
            scene=new Scene(lastGood,preview);canvas.Use(scene,name);
            if(!populate)return;
            controlsLoading=true;
            try {
                control.BeginUpdate();control.Items.Clear();
                control.Items.AddRange(scene.Items.Where(i=>i.Positioned&&!Editing.IsIconHint(lastGood.Window.Doc,i.Node)).Cast<object>().ToArray());
                control.EndUpdate();
                if(canvas.Selected!=null)control.SelectedItem=canvas.Selected;
            }finally{controlsLoading=false;}
            if(canvas.Selected!=null)Selected(canvas.Selected,false);
            else { x.Text=y.Text=""; selectedNote.Text=""; }
            Buttons();
        }
        private void Selected(VisualItem item,bool locate) {
            if(item==null)return;
            canvas.Selected=item;canvas.Invalidate();
            canvas.AllowedPlacement=preview.Playing?null:Editing.ElectionPlacement(lastGood,item,moveOfficeSeat.Checked);
            x.Text=item.Raw.X.ToString();y.Text=item.Raw.Y.ToString();x.ReadOnly=y.ReadOnly=item.Locked;
            string placement=canvas.AllowedPlacement==null?"":canvas.AllowedPlacement.Description;
            selectedNote.Text=canvas.AllowedPlacement!=null?canvas.AllowedPlacement.Summary:item.Locked?L.T("自动排布  ·  单槽锁定"):
                item.RegionFrame!=null?L.T("自动区域框"):L.T("原生局部坐标");
            tips.SetToolTip(selectedNote,(item.Name??"")+"\r\n"+placement+"\r\n"+(item.Tooltip??""));
            controlsLoading=true;try{if(control.Items.Contains(item))control.SelectedItem=item;}finally{controlsLoading=false;}
            if(locate&&lastGood!=null)Locate(lastGood.GuiPath,item.Node.Start);
            Buttons();
        }
        private void Locate(string path,int start) {
            if(path==""||model==null)return;
            Flush(false);
            int tab=Enumerable.Range(0,6).FirstOrDefault(i=>TabMatch(i,path));
            if(!TabMatch(tab,path)) { state.Text=path;return; }
            controlsLoading=true;try{
                tabs.SelectedIndex=tab;tabs.SelectedTab.Controls.Add(editor);overrides[tab]=path;
            }finally{controlsLoading=false;}
            FillFiles();
            if(openFile==path) {
                Entry e;
                if(!project.Content.TryGetValue(path,out e))e=model.Mount.Get(path,true);
                if(e==null)return;
                string raw=TextFile.Decode(e.Data).Text;
                int visual=Document.ViewText(raw.Substring(0,Math.Max(0,Math.Min(start,raw.Length)))).Length;
                editor.SelectionStart=Math.Min(visual,editor.TextLength);
                editor.SelectionLength=0;editor.ScrollToCaret();
            }
        }
        private void ProblemJump() { var p=problems.SelectedItem as Problem;if(p!=null)OpenProblem(p); }
        private async void OpenProblem(Problem p) {
            if(model==null)return;
            if(p.Binding!=null&&p.LayoutId!=null&&model.Layout!=null&&model.Layout.Node.Key!=p.LayoutId) {
                var typeOfLayout=model.LayoutUsers(p.LayoutId).FirstOrDefault(s=>s.Node.Get("abstract")!="yes");
                if(typeOfLayout!=null) {
                    project.Data.Type=typeOfLayout.Node.Key;project.Data.Instance="";project.Touch();overrides.Clear();
                    await RebuildTask();
                }
            }
            if(p.Binding!=null&&model!=null&&model.Window!=null&&(p.LayoutId==null||model.Layout.Node.Key==p.LayoutId)){OpenBindings(p.Binding);return;}
            Source layout;
            if(p.LayoutId!=null&&p.Binding==null&&model.Layouts.TryGetValue(p.LayoutId,out layout))Locate(layout.Path,layout.Node.Start);
            else Locate(p.File,p.Offset);
        }
        private void ApplyXY() { Do(()=>{
            if(canvas.Selected==null)return;
            int px,py;
            if(!Int32.TryParse(x.Text,out px)||!Int32.TryParse(y.Text,out py)||Geometry.Limit(px)!=px||Geometry.Limit(py)!=py)throw new InvalidDataException(L.T("请输入有效的原生 X、Y 整数。"));
            MoveVisual(canvas.Selected,new Point(px,py));
        });}
        private bool GoodVisual() {
            if (model==null||model!=lastGood||bufferDirty||model.Problems.Any(p=>p.Error)||parsingNow)
                throw new InvalidOperationException(L.T("请等待草稿解析成功，再操作画布控件。"));
            return true;
        }
        private bool Shared() {
            if(!shared.Checked)return false;
            var text=String.Join("\r\n",model.LayoutUsers(model.Layout.Node.Key).Select(s=>s.Node.Key));
            if(MessageBox.Show(this,L.T("下列体系引用此布局。是否一起更新？\r\n\r\n")+text,L.T("修改共享定义"),MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)
                throw new OperationCanceledException(L.T("已取消共享改动。"));
            return true;
        }
        private void MoveVisual(VisualItem item,Point point) {
            if(item.Raw==point)return;
            if(!GoodVisual())return;
            var space=canvas.AllowedPlacement;
            if(space!=null&&ReferenceEquals(canvas.Selected,item)) {
                Point placed;
                if(!space.TryNearest(point,out placed)) {
                    Selected(item,false);state.Text=space.Description;return;
                }
                if(placed!=point) {
                    x.Text=placed.X.ToString();y.Text=placed.Y.ToString();
                    state.Text=String.Format(L.T("已贴到最近的可放位置 ({0}, {1})。"),placed.X,placed.Y);
                }
                point=placed;
            }
            if(item.Raw==point)return;
            var plan=Editing.Move(project,model,item,point.X,point.Y,Shared(),moveOfficeSeat.Checked);plan.Commit();Rebuild();
        }
        private void ArrangeOffices() { Do(()=>{
            GoodVisual();
            using(var dialog=new ElectionAreaDialog(Editing.ReadElectionArea(model))) {
                if(dialog.ShowDialog(this)!=DialogResult.OK)return;
                var plan=Editing.ArrangeElection(project,model,dialog.Area,Shared());
                ApplyVisualPlan(plan,L.T("重新排布选举列表里的标题、盾徽、评分、悬停区和竖向滑条。可用 Ctrl+Z 一次撤销。"));
            }
        });}
        private void CompactScores() { Do(()=>{
            GoodVisual();
            var plan=Editing.CompactElectionScores(project,model,Shared());
            ApplyVisualPlan(plan,L.T("把评分和悬停区叠放在盾徽右下角。其它控件保留坐标。可用 Ctrl+Z 一次撤销。"));
        });}
        private bool Review(Plan p,string title) { return VisualEditValidation.Review(this,p,title); }
        private void Manual() {Do(()=>{
            if(canvas.Selected==null)return;
            GoodVisual();
            string area=canvas.Selected.AutoRegion??canvas.Selected.RegionFrame;
            if(area==null)throw new InvalidOperationException(L.T("先选择自动框或框内的自动槽。"));
            if(Review(Editing.UnpackFrame(project,model,scene,area,Shared()),L.T("把当前画布坐标写回槽位。垂直滑条与现有绑定继续使用。")))Rebuild();
        });}
        private void Play() {
            preview.Playing=!preview.Playing;test.Text=preview.Playing?L.T("编辑布局"):L.T("离线试玩");canvas.Subwindow="";
            canvas.PanelVisible=!preview.Playing||preview.MapDiscovered&&preview.Visible;
            RefreshScene(true);
            if(preview.Playing)state.Text=L.T("离线试玩  ·  操作记录不等同游戏实测");
        }
        private void Activate(VisualItem item) {
            if(item.Action=="diplomacy"){state.Text=L.T("模拟外交跳转  ·  ")+item.Country+L.T("  ·  主面板保留");return;}
            if(item.Action.StartsWith("action:",StringComparison.Ordinal)) {
                preview.Passed.Add(item.Action.Substring(7)); RefreshScene(true);
                state.Text=L.T("样例行动已完成。游戏费用、权限及效果待实测。");
            }
        }
        private void Inspect() {
            if(scene==null)return;
            string heading=L.T("面板定位  ")+scene.OnScreen+"  ·  "+scene.Panel+L.T("\r\n入口锚点  ")+scene.EntryPosition+"\r\n"+
                L.T("EU4 UI 比例 ")+preview.Scale.ToString("P0")+L.T("  ·  编辑器缩放 ")+canvas.Zoom.ToString("P0")+"\r\n";
            var item=canvas.Selected;
            if(item!=null) {
                var p=new Point(item.Bounds.X+item.Bounds.Width/2,item.Bounds.Y+item.Bounds.Height/2);
                var hit=canvas.HitAt(p,false);
                heading+=L.T("\r\n控件 ")+item.Name+"  ·  "+item.Bounds+L.T("\r\n当前命中 ")+(hit==null?L.T("无"):hit.Name)+"\r\n"+
                    L.T("点击 ")+(item.Click?L.T("可用"):L.T("只读／装饰"))+"  ·  "+(item.Country??"")+"\r\n"+ item.Tooltip+"\r\n";
                if(item.Sprite!=null&&model.Images!=null)heading+=String.Join("\r\n", model.Images.Find(item.Sprite).Select(s=>s.Name+" → "+s.Path+" → "+s.Texture+" → "+s.SourceRoot));
            }
            using(var d=new DiffDialog(L.T("预览命中详情"),new[]{L.T("当前控件")},_=>heading,L.T("坐标与点击均来自离线模拟。EU4 的真实命中请在 QA 中验收。"),false))d.ShowDialog(this);
        }
        private void Sources() {
            if(model==null)return;
            var sources=model.Chain.Select(s=>s.Node.Key+" ← "+s.Path).ToArray();
            string explanation=String.Join("\r\n",sources)+L.T("\r\n\r\n布局 ")+(model.Layout==null?L.T("未解析"):model.Layout.ToString())+L.T("\r\n窗口 ")+model.GuiPath+L.T("\r\n入口 ")+model.EntrySprite+
                L.T("\r\n挂载顺序\r\n")+String.Join("\r\n",new[]{model.Mount.Roots.Game}.Concat(model.Mount.Roots.Dependencies).Concat(new[]{model.Mount.Roots.Mod,"项目草稿"}))+
                L.T("\r\n\r\n覆盖与排除\r\n")+String.Join("\r\n",model.Mount.Removed);
            using(var d=new DiffDialog(L.T("解析来源"),new[]{L.T("继承与挂载"),openFile},k=>k==L.T("继承与挂载")?explanation:k+"  →  "+model.Mount.Source(k),L.T("包含继承、同路径覆盖与 replace_path。"),false))d.ShowDialog(this);
        }
        private void DocumentDiff() {
            if(project==null||openFile=="")return;Flush(false);
            Do(()=>{
                Entry entry;
                if(!project.Content.TryGetValue(openFile,out entry))entry=model.Mount.Get(openFile,true);
                if(entry==null)return;
                using(var d=new DiffDialog(L.T("文件差异"),new[]{openFile},k=>EditorDiff.Create(k,entry.Original,entry.Data),L.T("比较项目草稿与导入时的快照。"),false))d.ShowDialog(this);
            });
        }
        private void Reference() {
            var bytes=Files.Resource("starter.reference");
            if(bytes==null)return;
            using(var d=new DiffDialog(L.T("语法参考"),new[]{"default.txt"},k=>Document.ViewText(TextFile.Decode(bytes).Text).Replace("\n","\r\n"),
                L.T("示例仅用于编辑参考。游戏读取 common 和 history 中的实际定义。"),false))d.ShowDialog(this);
        }
        private void Add() {Do(()=>{
            GoodVisual();
            using(var dialog=new ComponentDialog(project,model,scene,canvas.Selected,shared.Checked)) if(dialog.ShowDialog(this)==DialogResult.OK)Rebuild();
        });}
        private void DeleteSelected() { Do(()=>{
            if(preview.Playing||canvas.Selected==null)return;
            var bound=ControlBindings.ForItem(model,canvas.Selected);
            if(bound!=null&&!bound.Hint){OpenBindings(bound.Path);return;}
            GoodVisual();
            var plan=Editing.DeleteControl(project,model,canvas.Selected,Shared());
            ApplyVisualPlan(plan,L.T("确认后从当前工程删除选中的组件和对应的悬停绑定。按 Ctrl+Z 可恢复。"));
        });}
        private void ApplyVisualPlan(Plan plan,string description) {
            if(!plan.Changes.Any()) { state.Text=L.T("绑定已经符合当前设置。");return; }
            var check=VisualEditValidation.Check(model,plan);
            var error=check.Blocking.FirstOrDefault();
            if(error!=null)throw new InvalidDataException(L.T("本次修改新增了问题，或改变了已有问题的位置。")+"\r\n"+error.File+"\r\n"+error.Message);
            if(VisualEditValidation.Review(this,plan,description,check.Existing))Rebuild();
        }
        private void OpenBindings(string path=null) { Do(()=>{
            if(project==null||model==null||model.Window==null||parsingNow)return;
            if(bufferDirty){Flush(true);state.Text=L.T("正读取刚编辑的代码。稍后可打开绑定与修复。");return;}
            var current=ControlBindings.ForItem(model,canvas.Selected);
            using(var dialog=new BindingDialog(model,path??(current==null?null:current.Path),shared.Checked)) {
                if(dialog.ShowDialog(this)!=DialogResult.OK)return;
                if(dialog.Choice==BindingChoice.Locate){Locate(model.Layout.Path,dialog.Binding.Location.Start);return;}
                bool editShared=Shared();
                if(dialog.Choice==BindingChoice.Remove)
                    ApplyVisualPlan(Editing.RemoveBinding(project,model,lastGood,scene,dialog.Binding,editShared),
                        L.T("查看会清理哪些控件与布局引用。确认后可用 Ctrl+Z 恢复。"));
                if(dialog.Choice==BindingChoice.Rebind)
                    ApplyVisualPlan(Editing.RebindControl(project,model,lastGood,dialog.Binding,dialog.Target,dialog.Clone,editShared),
                        L.T("查看替代控件和新的布局引用。确认后可用 Ctrl+Z 恢复。"));
                if(dialog.Choice==BindingChoice.Restore)
                    ApplyVisualPlan(Editing.RestoreControl(project,model,lastGood,dialog.Binding,editShared),
                        L.T("查看恢复的控件和对齐后的悬停区。确认后可用 Ctrl+Z 恢复。"));
            }
        });}
        private void OpenSearch(bool replace) {
            if(project==null||openFile.Length==0)return;
            if(search==null||search.IsDisposed) {
                search=new FindReplaceDialog(RunSearch);
                search.File(openFile,!editor.ReadOnly);
                search.StartPosition=FormStartPosition.Manual;
                var area=Screen.FromControl(this).WorkingArea;
                search.Location=new Point(Math.Max(area.Left,Math.Min(Right-search.Width-24,area.Right-search.Width)),Math.Max(area.Top,Top+110));
                search.Show(this);
            }
            search.Activate();
            search.Start(editor.SelectedText,replace);
        }
        internal string RunSearch(FindAction action,string query,string replacement,bool matchCase) {
            if(openFile==""||query.Length==0)return L.T("请先打开要查找的文件。");
            bool reverse=action==FindAction.Previous;
            bool replacing=action==FindAction.Replace||action==FindAction.ReplaceAll;
            var text=editor.Text;
            var result=CodeSearch.Find(text,query,editor.SelectionStart+(reverse?0:editor.SelectionLength),reverse,matchCase);
            if(result.Index<0)return L.T("当前文件未找到匹配文字。");
            if(!replacing) {
                editor.Select(result.Index,query.Length);editor.ScrollToCaret();
                return String.Format(L.T("第 {0} 处，共 {1} 处"),result.Number,result.Total)+(result.Wrapped?L.T(" · 已绕回文件另一端"):"");
            }
            if(editor.ReadOnly)return L.T("当前文件无法编辑。");
            int index=-1;
            if(action==FindAction.Replace) {
                var comparison=matchCase?StringComparison.Ordinal:StringComparison.OrdinalIgnoreCase;
                index=String.Equals(editor.SelectedText,query,comparison)?editor.SelectionStart:result.Index;
            }
            Flush(false);
            if(bufferDirty)return L.T("请先处理当前代码的保存错误。");
            var plan=new Plan(project,model.Mount,L.T("在当前文件替换文本"));
            var entry=plan.Get(openFile);
            if(entry==null)return L.T("当前文件无法编辑。");
            string raw=TextFile.Decode(entry.Data).Text;
            int count,first;
            var next=CodeSearch.Replace(raw,query,replacement,matchCase,index,out count,out first);
            if(count==0)return L.T("当前文件未找到匹配文字。");
            if(next==raw)return L.T("替换文字与原文相同。");
            plan.Text(openFile,next);plan.Commit();
            LoadEditor(openFile);
            editor.Select(first,Math.Min(Document.ViewText(replacement).Length,editor.TextLength-first));
            editor.ScrollToCaret();
            Rebuild();
            return String.Format(L.T("已在当前文件替换 {0} 处。回到代码后按 Ctrl+Z 可撤销。"),count);
        }
        private void ShowLanguages() {
            languageMenu.Show(interfaceLanguage,new Point(0,interfaceLanguage.Height));
        }
        private void SwitchLanguage(string languageCode) { Do(()=>{
            if(queuedClose||IsDisposed||Disposing||opening||languageCode==L.Current)return;
            Flush(false);
            if(bufferDirty)return;
            L.Save(Program.LanguagePreference,languageCode);
            NextWindow=new WorkbenchSession {
                Language=languageCode,Project=project,LastGood=lastGood,Preview=preview,
                Files=new Dictionary<int,string>(overrides),Bounds=WindowState==FormWindowState.Normal?Bounds:RestoreBounds,WindowState=WindowState,
                Tab=tabs.SelectedIndex,File=openFile,Caret=editor.SelectionStart,Selection=editor.SelectionLength,
                Control=canvas.Selected==null?null:canvas.Selected.Name,Subwindow=canvas.Subwindow,SubwindowOffset=canvas.SubwindowOffset,
                PanelVisible=canvas.PanelVisible,Zoom=Ui.Selected(zoom),Shared=shared.Checked
            };
            Close();
        });}
        internal async Task RestoreWindow(WorkbenchSession session) {
            if(session.Project==null)return;
            Set(session.Project);lastGood=session.LastGood;
            preview=session.Preview;shared.Checked=session.Shared;
            controlsLoading=true;
            try {
                tabs.SelectedIndex=Math.Max(0,session.Tab);tabs.SelectedTab.Controls.Add(editor);
                foreach(var file in session.Files)overrides[file.Key]=file.Value;
                resolution.SelectedItem=preview.Resolution.Width+"×"+preview.Resolution.Height;
                scale.SelectedItem=Math.Round(preview.Scale*100).ToString()+"%";zoom.SelectedItem=session.Zoom;
                entryState.SelectedIndex=!preview.Visible?2:!preview.Enabled?1:0;
                found.Checked=preview.MapDiscovered;
            } finally { controlsLoading=false; }
            SetupPreview();
            canvas.PanelVisible=session.PanelVisible;canvas.Subwindow=session.Subwindow;canvas.SubwindowOffset=session.SubwindowOffset;
            test.Text=preview.Playing?L.T("编辑布局"):L.T("离线试玩");
            await RebuildTask();
            RefreshScene(true,session.Control);
            if(openFile==session.File) {
                int start=Math.Min(session.Caret,editor.TextLength);
                editor.Select(start,Math.Min(session.Selection,editor.TextLength-start));editor.ScrollToCaret();
            }
        }
        private async void Export() {
            Flush(false);
            await RebuildTask();
            if(project==null||model==null)return;
            Do(()=>{using(var dialog=new ExportDialog(project,model)) {
                var result=dialog.ShowDialog(this);
                if(dialog.SelectedProblem!=null)OpenProblem(dialog.SelectedProblem);
                else if(result==DialogResult.OK){state.Text=L.T("导出完成  ·  ")+project.Data.ExportFolder;Buttons();}
            }});
        }
        private void Qa() {if(project!=null)Do(()=>{using(var dialog=new QaDialog(project,preview))dialog.ShowDialog(this);Buttons();});}
        private void KeysDown(object sender,KeyEventArgs e) {
            if(e.Control&&e.KeyCode==System.Windows.Forms.Keys.S){Save();e.SuppressKeyPress=true;}
            if(e.Control&&e.KeyCode==System.Windows.Forms.Keys.Z){Undo(e.Shift);e.SuppressKeyPress=true;}
            if(e.Control&&e.KeyCode==System.Windows.Forms.Keys.Y){Undo(true);e.SuppressKeyPress=true;}
            if(e.Control&&(e.KeyCode==System.Windows.Forms.Keys.F||e.KeyCode==System.Windows.Forms.Keys.H)){OpenSearch(e.KeyCode==System.Windows.Forms.Keys.H);e.SuppressKeyPress=true;}
            if(e.KeyCode==System.Windows.Forms.Keys.Delete&&!e.Control&&(canvas.Focused||control.Focused)&&remove.Enabled){DeleteSelected();e.SuppressKeyPress=true;}
            if(e.KeyCode==System.Windows.Forms.Keys.Escape&&preview.Playing){canvas.Escape();e.SuppressKeyPress=true;}
            if(e.KeyCode==System.Windows.Forms.Keys.F5){Play();e.SuppressKeyPress=true;}
        }
        internal void CheckDesktop(Action<string> note) {
            if(!IsHandleCreated||editor.TextLength==0||Ui.Selected(type)!="shogun_system"||scene==null)throw new InvalidOperationException("WinForms 草稿或面板没有呈现。");
            canvas.Refresh();Application.DoEvents();
            var stat=scene.ByName("primary_resource_value");
            if(stat==null||!canvas.HitAt(new Point(stat.Bounds.X+10,stat.Bounds.Y+10),false).Tooltip.Contains("55.00"))
                throw new InvalidOperationException("资源显示与命中来源不一致。");
            var begin=canvas.ClientAt(new Point(stat.Bounds.X+20,stat.Bounds.Y+10));
            int screenDelta=Enumerable.Range(1,70).OrderBy(n=>Math.Abs(canvas.Local(new Point(begin.X+n,begin.Y)).X-canvas.Local(begin).X-26)).First();
            var end=new Point(begin.X+screenDelta,begin.Y);
            int draggedX=stat.Raw.X+canvas.Local(end).X-canvas.Local(begin).X;
            SendMessage(canvas.Handle,0x0201,(IntPtr)1,MousePoint(begin));
            SendMessage(canvas.Handle,0x0200,(IntPtr)1,MousePoint(end));
            SendMessage(canvas.Handle,0x0202,IntPtr.Zero,MousePoint(end));
            WaitUntil(()=>!parsingNow);
            var changed=scene.ByName("primary_resource_value");
            if(changed.Raw.X!=draggedX||changed.Raw.Y!=130||!editor.Text.Contains("x = "+draggedX+" y = 130"))
                throw new InvalidOperationException("实际鼠标拖动没有改到 "+draggedX+" 并回写代码。当前 "+changed.Raw+"，"+
                    "选中 "+(canvas.Selected==null?"无":canvas.Selected.Name)+"，路径 "+openFile+"，修订 "+project.Data.Revision+"，"+state.Text);
            Undo(false);WaitUntil(()=>!parsingNow);
            if(scene.ByName("primary_resource_value").Raw.X!=254)throw new InvalidOperationException("工作台撤销未还原坐标。");
            Locate(model.GuiPath,model.Window.Node.Start);
            var original=editor.Text;
            var d=model.Window.Doc;
            var patched=d.Position(model.Window.Node.Named("primary_resource_value"),280,130);
            editor.Text=Document.ViewText(patched);
            Flush(true);WaitUntil(()=>!parsingNow);
            if(scene.ByName("primary_resource_value").Raw.X!=280)throw new InvalidOperationException("文本修改未反映到画布。");
            editor.Text+="\r\ninvalid = {\r\n";
            Flush(true);WaitUntil(()=>!parsingNow);
            if(model==lastGood||scene.ByName("primary_resource_value").Raw.X!=280||!editor.Text.Contains("invalid = {"))
                throw new InvalidOperationException("失效草稿没有保留最后有效画面。");
            editor.Text=original;Flush(true);WaitUntil(()=>!parsingNow);
            if(model!=lastGood||scene.ByName("primary_resource_value").Raw.X!=254)throw new InvalidOperationException("修复文本后预览未恢复。");
            note("E02 通过实际 WinForms 鼠标拖动、一次撤销、代码编辑、错误草稿和恢复。鼠标坐标 254,130 → "+draggedX+",130 → 254,130。");
            test.PerformClick();Application.DoEvents();
            var list=scene.Lists.First(l=>l.Key=="members_region");
            var first=scene.ByName("member_flag_00").Country;
            var wheel=canvas.PointToScreen(canvas.ClientAt(new Point(list.Bounds.X+25,list.Bounds.Y+30)));
            SendMessage(canvas.Handle,0x020A,(IntPtr)(-120<<16),MousePoint(wheel));
            if(preview.Offsets["members_region"]!=1||scene.ByName("member_flag_00").Country==first)
                throw new InvalidOperationException("成员滚轮未同步更新盾徽目标。");
            scale.SelectedItem="130%";Application.DoEvents();canvas.Refresh();
            var latest=scene.ByName("primary_resource_value");
            if(canvas.HitAt(canvas.Local(canvas.ClientAt(new Point(latest.Bounds.X+10,latest.Bounds.Y+10))),false)!=latest)
                throw new InvalidOperationException("130% UI 缩放的数字命中未对齐。");
            var flag=scene.Items.First(i=>i.Country=="REB"&&!i.Hidden);
            if(flag.Click||!flag.Tooltip.Contains("空缺"))throw new InvalidOperationException("REB 盾徽呈现有误。");
            found.Checked=false;
            if(canvas.PanelVisible)throw new InvalidOperationException("离线入口资格丢失未关闭面板。");
            found.Checked=true;
            test.PerformClick();scale.SelectedItem="100%";preview.Offsets.Clear();RefreshScene(true);
            note("离线试玩  ·  实际控件响应滚轮、130% 数值命中、空缺盾徽和入口隐藏。");
        }
        internal Point CheckElectionDrag(Action<string> note,string name="office_flag_00",Point? target=null,bool expectSnap=true) {
            WaitUntil(()=>!parsingNow);
            var flag=scene.ByName(name);
            Selected(flag,false);
            if(canvas.AllowedPlacement==null||canvas.AllowedPlacement.Areas.Length==0||!selectedNote.Text.Contains("X ")||
                !selectedNote.Text.Contains("Y ")||!tips.GetToolTip(selectedNote).Contains("绿线"))
                throw new InvalidOperationException("排布后没有可见落点指引。");
            canvas.Refresh();
            var origin=new Point(flag.Bounds.X+Math.Min(20,flag.Bounds.Width/2),flag.Bounds.Y+Math.Min(22,flag.Bounds.Height/2));
            var start=canvas.ClientAt(origin);
            var stop=target.HasValue?canvas.ClientAt(new Point(origin.X+target.Value.X-flag.Raw.X,origin.Y+target.Value.Y-flag.Raw.Y)):
                Point.Add(start,new Size(73,43));
            var at=canvas.Local(start);var end=canvas.Local(stop);
            if(canvas.HitAt(at,true)!=flag)
                throw new InvalidOperationException("测试拖动起点没有命中 "+name+" "+at+"。");
            var request=new Point(flag.Raw.X+end.X-at.X,flag.Raw.Y+end.Y-at.Y);
            Point expected;
            if(!canvas.AllowedPlacement.TryNearest(request,out expected)||expected==flag.Raw||expectSnap==(expected==request))
                throw new InvalidOperationException("测试拖动的落点不符合预期。");
            SendMessage(canvas.Handle,0x0201,(IntPtr)1,MousePoint(start));
            SendMessage(canvas.Handle,0x0200,(IntPtr)1,MousePoint(stop));
            SendMessage(canvas.Handle,0x0202,IntPtr.Zero,MousePoint(stop));
            WaitUntil(()=>!parsingNow);
            var actual=scene.ByName(flag.Name).Raw;
            if(actual!=expected)throw new InvalidOperationException("实际拖动落在 "+actual+"，预期 "+expected+"。");
            note("实际画布拖动从 "+flag.Raw+" 请求到 "+request+"，落在 "+expected+"。");
            return expected;
        }
        internal void CheckTextLayout(Action<string> note) {
            var all=Descendants(this).ToArray();
            var controls=all.Where(c=>c.Visible&&(c is Button||c==resolution||c==language||c==entryState||c==scale||c==zoom)).ToArray();
            var fonts=controls.Select(c=>c.Font).ToArray();
            var large=new List<Font>();
            Size windowSize=Size;
            try {
                Width=MinimumSize.Width;
                for(int i=0;i<controls.Length;++i) {
                    var current=fonts[i];var enlarged=new Font(current.FontFamily,current.SizeInPoints*1.5f,current.Style);
                    large.Add(enlarged);controls[i].Font=enlarged;
                }
                PerformLayout();Application.DoEvents();
                foreach(var c in controls) {
                    if(c.Text==""||c==test&&scene==null)continue;
                    using(var graphics=c.CreateGraphics()) {
                        var measured=TextRenderer.MeasureText(graphics,c.Text,c.Font);
                        int usable=c is ComboBox?c.ClientSize.Width-SystemInformation.VerticalScrollBarWidth:c.ClientSize.Width-c.Padding.Horizontal;
                        if(measured.Width>usable+2||c.Bottom>c.Parent.ClientSize.Height+2||c.Right>c.Parent.ClientSize.Width+2)
                            throw new InvalidOperationException("放大字体后发生截字或换行遮挡 "+c.Text+" "+c.Bounds+" 父控件 "+c.Parent.ClientSize);
                    }
                    if(!Ui.TipText(c).Contains(c.Text))throw new InvalidOperationException("按钮或下拉框缺少完整提示 "+c.Text);
                }
                if(!tabs.ShowToolTips||tabs.TabPages.Cast<TabPage>().Any(p=>p.ToolTipText.Length==0))
                    throw new InvalidOperationException("代码标签页缺少提示。");
                note("窄窗口及 150% 字号检查通过。工具栏按钮、短选项、标签页具备完整悬停文案。");
            } finally {
                for(int i=0;i<controls.Length;++i)controls[i].Font=fonts[i];
                foreach(var font in large)font.Dispose();
                Size=windowSize;
                PerformLayout();Application.DoEvents();
            }
        }
        internal WorkbenchSession CheckSearchAndSwitch(Action<string> note) {
            Locate(model.GuiPath,model.Window.Node.Start);
            string original=editor.Text;
            editor.Text+= "\n# pfFindDemo pfFindDemo PFFINDDEMO\n";
            Flush(true);WaitUntil(()=>!parsingNow);
            editor.Select(0,0);
            OpenSearch(false);
            if(search==null||!search.Visible)throw new InvalidOperationException("右侧查找窗口未打开。");
            if(!RunSearch(FindAction.Next,"pfFindDemo","",true).Contains("共 2 处")||editor.SelectedText!="pfFindDemo")
                throw new InvalidOperationException("查找模式没有定位第一处或正确区分大小写。");
            if(!RunSearch(FindAction.Next,"pfFindDemo","",true).Contains("第 2 处")||
                !RunSearch(FindAction.Next,"pfFindDemo","",true).Contains("已绕回"))
                throw new InvalidOperationException("查找下一处或绕回文件失败。");
            if(!RunSearch(FindAction.Previous,"pfFindDemo","",true).Contains("第 2 处"))
                throw new InvalidOperationException("查找上一处失败。");
            RunSearch(FindAction.Replace,"pfFindDemo","singleReplacement",true);WaitUntil(()=>!parsingNow);
            if(!editor.Text.Contains("pfFindDemo singleReplacement PFFINDDEMO"))throw new InvalidOperationException("替换一处误改了其他匹配。");
            RunSearch(FindAction.ReplaceAll,"pffinddemo","allReplacement",false);WaitUntil(()=>!parsingNow);
            if(!editor.Text.Contains("allReplacement singleReplacement allReplacement"))throw new InvalidOperationException("全部替换遗漏当前文件中的匹配。");
            Undo(false);WaitUntil(()=>!parsingNow);
            if(!editor.Text.Contains("pfFindDemo singleReplacement PFFINDDEMO"))throw new InvalidOperationException("全部替换未收成一个撤销事务。");
            search.Close();
            editor.Text=original+"\n# draft_preserved_through_language_switch\n";
            note("实际 Windows 工作台验证当前文件查找、区分大小写、上一处／下一处、绕回、单次和全部替换以及一次撤销。");
            CheckLanguageMenu(null);
            CheckLanguageMenu(L.Chinese);
            CheckLanguageMenu(L.English);
            if(NextWindow==null||!ReferenceEquals(project,NextWindow.Project)||L.Read(Program.LanguagePreference)!=L.English)
                throw new InvalidOperationException("英文界面偏好未保存，或切换时丢失工程。");
            note("实际语言菜单通过 Esc 关闭、重开、重复选择当前语言，并通过菜单项的 Windows 点击消息切换到 English。");
            return NextWindow;
        }
        internal void CheckLanguageMenu(string target,bool useMouse=true) {
            interfaceLanguage.PerformClick();
            var choices=languageMenu.Items.OfType<ToolStripMenuItem>().ToArray();
            if(!languageMenu.Visible||choices.Length!=2||choices.Any(item=>item.Checked!=((string)item.Tag==L.Current)))
                throw new InvalidOperationException("语言菜单未打开或当前选项未正确勾选。");
            var menu=languageMenu.Handle;
            bool posted;
            if(target==null)posted=PostMessage(menu,0x0100,(IntPtr)Keys.Escape,IntPtr.Zero);
            else if(useMouse) {
                var area=choices.Single(item=>(string)item.Tag==target).Bounds;
                var center=MousePoint(new Point(area.Left+area.Width/2,area.Top+area.Height/2));
                posted=PostMessage(menu,0x0200,IntPtr.Zero,center)&&PostMessage(menu,0x0201,(IntPtr)1,center)&&
                    PostMessage(menu,0x0202,IntPtr.Zero,center);
            } else {
                choices.Single(item=>(string)item.Tag==target).PerformClick();
                languageMenu.Close(ToolStripDropDownCloseReason.ItemClicked);
                posted=true;
            }
            if(!posted)throw new InvalidOperationException("未能发送语言菜单输入。");
            if(target==null||target==L.Current) {
                WaitUntil(()=>!languageMenu.Visible);
                if(NextWindow!=null||IsDisposed||languageMenu.IsDisposed)
                    throw new InvalidOperationException("取消或重复选择语言时关闭了工作台或释放了菜单。");
            } else WaitUntil(()=>NextWindow!=null);
        }
        internal void CheckEnglish(WorkbenchSession session,Action<string> note) {
            var pending=RestoreWindow(session);
            WaitUntil(()=>pending.IsCompleted);
            pending.GetAwaiter().GetResult();
            if(interfaceLanguage.Text!="Language…"||bindings.Text!="Bindings and repair…"||find.Text!="Find and replace…"||
                tabs.TabPages[5].Text!="Localisation"||entryState.Text!="Visible and enabled"||
                project.Data.Language!="l_english"||!editor.Text.Contains("# draft_preserved_through_language_switch"))
                throw new InvalidOperationException("English 模式的菜单、页签、偏好或未保存修改没有恢复。");
            if(!project.CanUndo||preview.Scale!=1m)
                throw new InvalidOperationException("切换界面语言时丢失撤销记录或预览状态。");
            CheckTextLayout(note);
            note("English 界面及悬停文案通过。切换保留当前文件、游戏文本语言、未保存草稿和撤销记录。程序偏好从磁盘重新读回。");
        }
        private static IEnumerable<Control> Descendants(Control control) {
            foreach(Control child in control.Controls) {
                yield return child;
                foreach(var sub in Descendants(child))yield return sub;
            }
        }
        private static IntPtr MousePoint(Point p) { return (IntPtr)((p.X&65535)|((p.Y&65535)<<16)); }
        private static void WaitUntil(Func<bool> done) {
            var clock=System.Diagnostics.Stopwatch.StartNew();
            while(!done()&&clock.ElapsedMilliseconds<15000){Application.DoEvents();Thread.Sleep(7);}
            if(!done())throw new TimeoutException("编辑器未在验收时限内刷新。");
            Application.DoEvents();
        }
        [DllImport("user32.dll",CharSet=CharSet.Unicode)]
        private static extern IntPtr SendMessage(IntPtr window,int message,IntPtr wParam,IntPtr lParam);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)]
        private static extern bool PostMessage(IntPtr window,int message,IntPtr wParam,IntPtr lParam);
    }
}
