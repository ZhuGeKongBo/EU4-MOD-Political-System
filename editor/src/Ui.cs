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
    internal static class Ui {
        private sealed class Choice {
            internal readonly string Value;
            internal Choice(string value) { Value=value; }
            public override string ToString() { return L.T(Value); }
        }
        internal static readonly Color Ink=Color.FromArgb(23,40,57), Soft=Color.FromArgb(85,103,120), Line=Color.FromArgb(223,229,235), Light=Color.FromArgb(246,248,250),
            Navy=Color.FromArgb(35,62,87), Paper=Color.White, Blue=Color.FromArgb(45,91,131);
        internal static readonly Font TextFont=new Font("Microsoft YaHei UI",9f);
        internal static readonly Font CodeFont=new Font("Consolas",10f);
        internal static readonly Font Small=new Font("Microsoft YaHei UI",8.5f);
        internal static readonly Font Heading=new Font("Microsoft YaHei UI",14f,FontStyle.Bold);
        internal static readonly Font Big=new Font("Microsoft YaHei UI",21f,FontStyle.Bold);
        private static readonly ToolTip FullText = new ToolTip { ShowAlways=true,InitialDelay=460,ReshowDelay=80,AutoPopDelay=14000 };
        internal static Button Button(string text,EventHandler click,bool primary=false,int width=86) {
            var b=new Button { Text=text,Width=width,Height=33,Margin=new Padding(0,0,7,0),FlatStyle=FlatStyle.Flat,Font=TextFont,
                BackColor=primary?Navy:Paper,ForeColor=primary?Paper:Ink, UseVisualStyleBackColor=false,Cursor=Cursors.Hand,TabStop=true,
                AutoSize=true,AutoSizeMode=AutoSizeMode.GrowOnly,MinimumSize=new Size(width,33),Padding=new Padding(6,1,6,1) };
            b.FlatAppearance.BorderColor=primary?Navy:Line;
            b.FlatAppearance.MouseOverBackColor=primary?Blue:Light;
            b.TextChanged+=(s,e)=>Tip(b,b.Text);
            Tip(b,text);
            if (click!=null) b.Click+=click;
            return b;
        }
        internal static Label Label(string text,Font font=null,Color? color=null) {
            return new Label { Text=text,AutoSize=true,Font=font??TextFont,ForeColor=color??Ink,Margin=new Padding(0,4,8,3) };
        }
        internal static FlowLayoutPanel Flow(bool wrap=false) {
            return new FlowLayoutPanel { AutoSize=false,WrapContents=wrap,FlowDirection=FlowDirection.LeftToRight,Margin=Padding.Empty,BackColor=Paper };
        }
        internal static TableLayoutPanel Stack(params int[] sizes) {
            var table=new TableLayoutPanel { ColumnCount=1,RowCount=sizes.Length,Dock=DockStyle.Fill,Padding=Padding.Empty,Margin=Padding.Empty,BackColor=Paper };
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            foreach (var size in sizes) table.RowStyles.Add(size==0?new RowStyle(SizeType.Percent,100):new RowStyle(SizeType.Absolute,size));
            return table;
        }
        internal static void Tip(Control control,string text) { FullText.SetToolTip(control,text??""); }
        internal static string TipText(Control control) { return FullText.GetToolTip(control); }
        internal static ComboBox Combo(int width=180) {
            var combo=new ComboBox { Width=width,Height=28, DropDownStyle=ComboBoxStyle.DropDownList,Font=TextFont,Margin=new Padding(0,4,8,0), MaxDropDownItems=15 };
            combo.SelectedIndexChanged+=(s,e)=>Tip(combo,combo.SelectedItem==null?"":combo.SelectedItem.ToString());
            combo.DropDown+=(s,e)=>{
                int wanted=combo.Width;
                using(var g=combo.CreateGraphics())foreach(var item in combo.Items)
                    wanted=Math.Max(wanted,TextRenderer.MeasureText(g,item.ToString(),combo.Font).Width+SystemInformation.VerticalScrollBarWidth+16);
                combo.DropDownWidth=Math.Max(combo.Width,Math.Min(wanted,Screen.FromControl(combo).WorkingArea.Width-30));
            };
            return combo;
        }
        internal static void FitChoices(ComboBox combo) {
            EventHandler resize=(s,e)=> {
                if(combo.Items.Count==0)return;
                using(var g=combo.CreateGraphics())combo.Width=combo.Items.Cast<object>()
                    .Max(i=>TextRenderer.MeasureText(g,i.ToString(),combo.Font).Width)+SystemInformation.VerticalScrollBarWidth+16;
            };
            combo.FontChanged+=resize;
            combo.HandleCreated+=resize;
            combo.SelectedIndexChanged+=resize;
            if(combo.IsHandleCreated)resize(combo,EventArgs.Empty);
        }
        internal static void FitRow(TableLayoutPanel table,FlowLayoutPanel row,int index) {
            row.WrapContents=true;row.AutoSize=false;
            var style=table.RowStyles[index];
            style.SizeType=SizeType.Absolute;
            bool pending=false;
            row.Layout+=(s,e)=>{
                if(pending||!row.IsHandleCreated||row.IsDisposed)return;
                pending=true;
                row.BeginInvoke((Action)(()=>{
                    pending=false;
                    if(row.IsDisposed)return;
                    // A second pass reads the actual wrapped positions, including native ComboBox heights.
                    int last=0;
                    foreach(Control child in row.Controls)if(child.Visible)
                        last=Math.Max(last,child.Bottom+child.Margin.Bottom);
                    int wanted=last==0?0:last+row.Padding.Bottom+3;
                    if(style.Height!=wanted)style.Height=wanted;
                }));
            };
            row.VisibleChanged+=(s,e)=>row.PerformLayout();
            table.Controls.Add(row,0,index);
        }
        internal static TextBox Box(string text,int width=180) {
            var box=new TextBox { Text=text??"",Width=width,Font=TextFont,Margin=new Padding(0,4,8,0) };
            Tip(box,box.Text);
            box.TextChanged+=(s,e)=>Tip(box,box.Text);
            return box;
        }
        internal static TextBox View(string text,bool dark=false) {
            return new TextBox { Text=text??"",Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Both,WordWrap=false,Dock=DockStyle.Fill,Font=CodeFont,
                BorderStyle=BorderStyle.FixedSingle,BackColor=dark?Color.FromArgb(27,38,48):Light,ForeColor=dark?Color.FromArgb(218,227,234):Ink,Margin=Padding.Empty };
        }
        internal static TextBox Help(string text) {
            var box=View(text);
            box.WordWrap=true;box.ScrollBars=ScrollBars.Vertical;box.Font=TextFont;
            return box;
        }
        internal static void Dialog(Form form,string title,Size size) {
            form.Text=title; form.ClientSize=size; form.StartPosition=FormStartPosition.CenterParent; form.MinimumSize=new Size(Math.Min(size.Width,600),Math.Min(size.Height,350));
            form.Font=TextFont; form.BackColor=Paper; form.ForeColor=Ink; form.ShowIcon=false; form.AutoScaleMode=AutoScaleMode.Dpi;
        }
        internal static void Error(IWin32Window owner,Exception e) {
            Program.Log(e);
            MessageBox.Show(owner,e.Message,L.T("操作未完成"),MessageBoxButtons.OK,MessageBoxIcon.Information);
        }
        internal static string Folder(IWin32Window owner,string text,string path) {
            using (var f=new FolderBrowserDialog { Description=text,SelectedPath=Directory.Exists(path)?path:"",ShowNewFolderButton=true })
                return f.ShowDialog(owner)==DialogResult.OK?f.SelectedPath:null;
        }
        internal static string PickFile(IWin32Window owner,string filter) {
            using (var dialog=new OpenFileDialog { Filter=filter, CheckFileExists=true,Multiselect=false,DereferenceLinks=false })
                return dialog.ShowDialog(owner)==DialogResult.OK?dialog.FileName:null;
        }
        internal static string Selected(ComboBox box) {
            var choice=box.SelectedItem as Choice;
            return choice!=null?choice.Value:box.SelectedItem==null?"":box.SelectedItem.ToString();
        }
        internal static void Choose(ComboBox box,string value) {
            var choice=box.Items.Cast<object>().FirstOrDefault(item=>item is Choice?((Choice)item).Value==value:item.ToString()==value);
            if(choice!=null)box.SelectedItem=choice;
        }
        internal static void LoadChoices(ComboBox box,IEnumerable<string> options,string value) {
            box.BeginUpdate();
            try {
                box.Items.Clear(); box.Items.AddRange(options.Select(o=>(object)new Choice(o)).ToArray());
                if(box.Items.Count==0)return;
                box.SelectedIndex=0;
                if(value!=null)Choose(box,value);
            } finally { box.EndUpdate(); }
        }
        internal static void Load(ComboBox box,IEnumerable<string> options,string value) {
            box.BeginUpdate(); box.Items.Clear(); box.Items.AddRange(options.Cast<object>().ToArray());
            if (value!=null&&box.Items.Contains(value)) box.SelectedItem=value; else if(box.Items.Count>0)box.SelectedIndex=0;
            box.EndUpdate();
        }
        internal static void Fill(Control host,Control child) { child.Dock=DockStyle.Fill; child.Margin=Padding.Empty; host.Controls.Add(child); }
        internal static void OpenFolder(string path) {
            if (!Directory.Exists(path)) throw new IOException(L.T("未找到该输出目录。"));
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("explorer.exe","\"" + path + "\"") { UseShellExecute=true });
        }
    }

    internal sealed class RootDialog : Form {
        private readonly TextBox game, user, mod, dependencies;
        private readonly bool requireMod;
        internal RootMap Roots;
        internal RootDialog(RootMap map,bool import) {
            Ui.Dialog(this,import?L.T("打开已有 Mod"):L.T("项目路径与挂载顺序"),new Size(775,400)); requireMod=import;
            var table=Ui.Stack(55,59,59,59,0,43); table.Padding=new Padding(17,13,17,12); Controls.Add(table);
            var headline=Ui.Label(import?L.T("选择框架 Mod 和游戏位置"):L.T("定位数据来源"),Ui.Heading); table.Controls.Add(headline,0,0);
            game=Row(table,1,L.T("EU4 安装目录"),map.Game); user=Row(table,2,L.T("EU4 用户目录"),map.User); mod=Row(table,3,"源 Mod",map.Mod);
            var section=Ui.Stack(21,0);
            section.Controls.Add(Ui.Label(L.T("依赖 Mod 文件夹  ·  低优先级在上，每行一项"),Ui.Small,Ui.Soft),0,0);
            dependencies=new TextBox { Multiline=true,ScrollBars=ScrollBars.Vertical,Dock=DockStyle.Fill,Text=String.Join("\r\n",map.Dependencies??new string[0]) };
            section.Controls.Add(dependencies,0,1);table.Controls.Add(section,0,4);
            var bar=Ui.Flow();bar.Dock=DockStyle.Fill;bar.FlowDirection=FlowDirection.RightToLeft;bar.Padding=new Padding(0,8,0,0);table.Controls.Add(bar,0,5);
            bar.Controls.Add(Ui.Button(L.T("使用这些路径"),(s,e)=>Done(),true,125));bar.Controls.Add(Ui.Button(L.T("取消"),(s,e)=>{DialogResult=DialogResult.Cancel;Close();}));
        }
        private TextBox Row(TableLayoutPanel table,int row,string label,string initial) {
            var section=Ui.Stack(20,0);
            section.Controls.Add(Ui.Label(L.T(label),Ui.Small,Ui.Soft),0,0);
            var line=new TableLayoutPanel { Dock=DockStyle.Fill,ColumnCount=2,Margin=Padding.Empty };
            line.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));line.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,88));
            var edit=Ui.Box(initial);edit.Dock=DockStyle.Top;line.Controls.Add(edit,0,0);
            line.Controls.Add(Ui.Button(L.T("浏览…"),(s,e)=>{string path=Ui.Folder(this,L.T(label),edit.Text);if(path!=null)edit.Text=path;}),1,0);
            section.Controls.Add(line,0,1);table.Controls.Add(section,0,row);return edit;
        }
        private void Done() {
            try {
                string g=game.Text.Trim(),u=user.Text.Trim(),m=mod.Text.Trim();
                if (g.Length!=0 && !File.Exists(Path.Combine(g,"eu4.exe"))) throw new IOException(L.T("安装目录应包含 eu4.exe。"));
                if (requireMod && (m=="" || !File.Exists(Path.Combine(m,"descriptor.mod")))) throw new IOException(L.T("源 Mod 文件夹应包含 descriptor.mod。"));
                if (u.Length!=0&&!Directory.Exists(u)) throw new IOException(L.T("EU4 用户目录尚不存在。"));
                var deps=dependencies.Lines.Select(p=>p.Trim()).Where(p=>p!="").Select(Files.Normalize).Distinct(Files.Windows).ToArray();
                foreach(var d in deps) if(!File.Exists(Path.Combine(d,"descriptor.mod")))throw new IOException(L.T("依赖文件夹缺少 descriptor.mod ") + d);
                Roots=new RootMap { Game=g.Length==0?"":Files.Normalize(g),User=u.Length==0?"":Files.Normalize(u),Mod=m.Length==0?"":Files.Normalize(m),Dependencies=deps };
                if (Roots.Mod!="" && deps.Any(p=>Files.Windows.Equals(Roots.Mod,p))) throw new IOException(L.T("源 Mod 已挂载。请从依赖中移除该重复路径。"));
                DialogResult=DialogResult.OK;Close();
            } catch(Exception ex){Ui.Error(this,ex);}
        }
    }

    internal sealed class NewDialog : Form {
        private readonly TextBox id, instance, date, country;
        private readonly ComboBox sourceFile, source, layoutFile, layout;
        private readonly CheckBox copy;
        private readonly Model model;
        private readonly TextBox preview;
        internal string TypeId, InstanceId, Date, Country, ParentId, LayoutId;
        internal bool Copy;
        private bool filling;
        internal NewDialog(Model current) {
            model=current;
            Ui.Dialog(this,model==null?L.T("从默认版本新建"):L.T("从模板新建"),new Size(735,486));
            var table=Ui.Stack(50,48,48,model==null?35:51,model==null?1:51,0,43);table.Padding=new Padding(17,14,17,13);Controls.Add(table);
            table.Controls.Add(Ui.Label(model==null?L.T("新建一套政治规则"):L.T("选择类型与布局模板"),Ui.Heading),0,0);
            id=Ui.Box(model==null?"new_polity":"new_charter",175);instance=Ui.Box(model==null?"new_polity_1444":"new_charter_1444",190);
            date=Ui.Box("1444.1.1",120); country=Ui.Box(model==null?"ASK":model.Country,85);
            table.Controls.Add(Line(L.T("类型 ID"),id,L.T("实例 ID"),instance),0,1);table.Controls.Add(Line(L.T("历史开局日"),date,L.T("领袖标签"),country),0,2);
            if (model!=null) {
                sourceFile=Ui.Combo(323);source=Ui.Combo(170);layoutFile=Ui.Combo(323);layout=Ui.Combo(170);
                table.Controls.Add(Line(L.T("类型文件"),sourceFile,L.T("类型"),source),0,3);table.Controls.Add(Line(L.T("布局文件"),layoutFile,L.T("布局"),layout),0,4);
                Ui.Load(sourceFile,model.Types.Values.Select(x=>x.Path).Distinct(Files.Windows),model.Type==null?null:model.Type.Path);
                Ui.Load(layoutFile,model.Layouts.Values.Select(x=>x.Path).Distinct(Files.Windows),model.Layout==null?null:model.Layout.Path);
                sourceFile.SelectedIndexChanged+=(s,e)=>FillTypes();layoutFile.SelectedIndexChanged+=(s,e)=>FillLayouts();
                source.SelectedIndexChanged+=(s,e)=>Summary();layout.SelectedIndexChanged+=(s,e)=>Summary();
            } else table.Controls.Add(Ui.Label(L.T("默认工程将填入天朝布局、一个历史实例和游戏入口。"),Ui.Small,Ui.Soft),0,3);
            preview=Ui.View("");table.Controls.Add(preview,0,5);
            var btn=Ui.Flow();btn.Dock=DockStyle.Fill;btn.Padding=new Padding(0,7,0,0);table.Controls.Add(btn,0,6);
            copy=new CheckBox { Text=L.T("展开并复制成独立类型和 GUI"),AutoSize=true,Visible=model!=null,Margin=new Padding(8,5,8,0) };
            btn.Controls.Add(Ui.Button(L.T("建立草稿"),(s,e)=>Done(),true,100));btn.Controls.Add(Ui.Button(L.T("取消"),(s,e)=>{DialogResult=DialogResult.Cancel;Close();}));btn.Controls.Add(copy);
            if(model!=null){FillTypes();FillLayouts();}
            else preview.Text=L.T("默认布局  →  celestial_layout\r\n原生窗口  →  pf075_celestial_panel\r\n入口区域  →  pf075_style_entries\r\n资源起始值 55.00\r\n\r\n先看预览，再修改草稿。原 Mod 不会因新建工程而改变。");
        }
        private FlowLayoutPanel Line(string a,Control first,string b,Control second) {
            var row=Ui.Flow();row.Dock=DockStyle.Fill;row.Controls.Add(Ui.Label(a));row.Controls.Add(first);row.Controls.Add(Ui.Label(b));row.Controls.Add(second);return row;
        }
        private void FillTypes() { filling=true;Ui.Load(source,model.Types.Values.Where(x=>x.Path==Ui.Selected(sourceFile)).Select(x=>x.Node.Key),model.Type==null?"celestial_style":model.Type.Node.Key);filling=false;Summary(); }
        private void FillLayouts() { filling=true;Ui.Load(layout,model.Layouts.Values.Where(x=>x.Path==Ui.Selected(layoutFile)).Select(x=>x.Node.Key),model.Layout==null?"celestial_layout":model.Layout.Node.Key);filling=false;Summary(); }
        private void Summary() {
            if (filling||preview==null||model==null) return;
            Source selected;
            var rule=model.Types.ContainsKey(Ui.Selected(source))?model.Resolve(Ui.Selected(source),new List<string>()):null;
            if(model.Layouts.TryGetValue(Ui.Selected(layout),out selected)&&rule!=null) {
                string sprite=rule.Read("","interface","entry_button","icon");
                var spriteTarget=model.Images==null?null:model.Images.Find(sprite).FirstOrDefault();
                preview.Text=String.Join("\r\n",new[]{source.Text+"  →  "+layout.Text,selected.Node.Get("gui_file")+"  →  "+selected.Node.Get("window"),
                    L.T("入口  ")+sprite+(spriteTarget==null?"":"  →  "+spriteTarget.Texture),L.T("视觉修改默认复制共享布局和窗口。")});
            }
        }
        private void Done() {
            try {
                TypeId=id.Text.Trim(); InstanceId=instance.Text.Trim(); Date=date.Text.Trim(); Country=country.Text.Trim().ToUpperInvariant();
                if(!Files.IsId(TypeId)||!Files.IsId(InstanceId)||TypeId.Length>50||InstanceId.Length>50) throw new IOException(L.T("请使用最多 50 位的英文 ID。"));
                if(Model.DateKey(Date)<0||Country.Length!=3||!Country.All(c=>Char.IsLetterOrDigit(c)&&c<=127)) throw new IOException(L.T("请填写如 1444.1.1 和 ASK 的日期与标签。"));
                ParentId=source==null?"":Ui.Selected(source);LayoutId=layout==null?"":Ui.Selected(layout); Copy=copy.Checked;
                DialogResult=DialogResult.OK;Close();
            } catch(Exception e){Ui.Error(this,e);}
        }
    }

    internal sealed class DiffDialog : Form {
        internal DiffDialog(string title,IEnumerable<string> keys,Func<string,string> text,string introduction,bool accept,string wrappedKey=null) {
            Ui.Dialog(this,title,new Size(1000,595));var outer=Ui.Stack(56,0,49);outer.Padding=new Padding(15,12,15,10);Controls.Add(outer);
            outer.Controls.Add(new Label { Text=introduction,Dock=DockStyle.Fill,ForeColor=Ui.Soft,AutoEllipsis=true,Padding=new Padding(2,4,2,0) },0,0);
            var split=new SplitContainer { Size=new Size(958,470),FixedPanel=FixedPanel.Panel1,SplitterDistance=298, Dock=DockStyle.Fill,SplitterWidth=6 };
            var list=new ListBox { HorizontalScrollbar=true,Dock=DockStyle.Fill,IntegralHeight=false,Font=Ui.Small };
            var content=Ui.View("",true);Ui.Fill(split.Panel1,list);Ui.Fill(split.Panel2,content);outer.Controls.Add(split,0,1);
            list.Items.AddRange(keys.Cast<object>().ToArray());list.SelectedIndexChanged+=(s,e)=> {
                if(list.SelectedItem==null)return;
                string key=list.SelectedItem.ToString();
                content.WordWrap=key==wrappedKey;content.ScrollBars=content.WordWrap?ScrollBars.Vertical:ScrollBars.Both;
                try{content.Text=text(key);}catch(Exception ex){content.Text=ex.Message;Program.Log(ex);}
            };if(list.Items.Count>0)list.SelectedIndex=0;
            var bar=Ui.Flow();bar.Dock=DockStyle.Fill;bar.Padding=new Padding(0,9,0,0);outer.Controls.Add(bar,0,2);
            if(accept)bar.Controls.Add(Ui.Button(L.T("确认应用"),(s,e)=>{DialogResult=DialogResult.OK;Close();},true,108));
            bar.Controls.Add(Ui.Button(accept?L.T("返回"):L.T("关闭"),(s,e)=>{DialogResult=DialogResult.Cancel;Close();}));
        }
    }
}
