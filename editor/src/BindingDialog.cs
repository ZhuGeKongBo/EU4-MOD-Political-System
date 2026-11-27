using System;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace PoliticalEditor {
    internal enum BindingChoice { None, Locate, Remove, Rebind, Restore }

    internal sealed class BindingDialog : Form {
        private readonly Model model;
        private readonly bool shared;
        private readonly ComboBox binding=Ui.Combo(345), replacement=Ui.Combo(310);
        private readonly TextBox name=Ui.Box("",215), info=Ui.Help("");
        private readonly Button remove,rebind,restore;
        private readonly Label nameLabel;
        private ControlBinding[] entries;
        internal BindingChoice Choice { get; private set; }
        internal ControlBinding Binding { get { return binding.SelectedIndex<0?null:entries[binding.SelectedIndex]; } }
        internal bool Clone { get { return replacement.SelectedIndex==0; } }
        internal string Target { get { return Clone?name.Text.Trim():Ui.Selected(replacement); } }

        internal BindingDialog(Model current,string selected,bool editShared) {
            model=current;shared=editShared;
            Ui.Dialog(this,L.T("绑定与修复"),new Size(880,450));
            var page=Ui.Stack(48,42,44,42,0,49);
            page.Padding=new Padding(15,13,15,11);
            Controls.Add(page);
            page.Controls.Add(Ui.Label(L.T("组件绑定与修复"),Ui.Heading),0,0);
            Row(page,1,Ui.Label(L.T("组件用途")),binding);
            Row(page,2,Ui.Label(L.T("替换位置")),replacement);
            nameLabel=Ui.Label(L.T("新控件名称"));
            Row(page,3,nameLabel,name);
            page.Controls.Add(info,0,4);
            remove=Ui.Button(L.T("移除显示…"),(s,e)=>Done(BindingChoice.Remove),true,112);
            rebind=Ui.Button(L.T("更换绑定…"),(s,e)=>Done(BindingChoice.Rebind),false,112);
            restore=Ui.Button(L.T("恢复控件…"),(s,e)=>Done(BindingChoice.Restore),false,112);
            Row(page,5,remove,rebind,restore,Ui.Button(L.T("定位到布局"),(s,e)=>Done(BindingChoice.Locate),false,105),
                Ui.Button(L.T("取消"),(s,e)=>Close()));
            entries=ControlBindings.Read(model).ToArray();
            binding.Items.AddRange(entries.Select(b=>(object)((b.Missing(model).Any()?L.T("缺失")+" · ":"")+b.Title+" · "+
                (b.Name.Length==0?L.T("未显示"):b.Name)+(b.IsGroup?" × "+b.Count:""))).ToArray());
            binding.SelectedIndexChanged+=(s,e)=>Describe();
            replacement.SelectedIndexChanged+=(s,e)=>UpdateReplacement();
            int index=Array.FindIndex(entries,b=>b.Path==selected);
            if(index<0)index=Array.FindIndex(entries,b=>b.Missing(model).Any());
            if(index<0)index=Array.FindIndex(entries,b=>b.Path==ControlBindings.LeaderLabel);
            if(entries.Length>0)binding.SelectedIndex=Math.Max(0,index);
            else Describe();
        }
        private static void Row(TableLayoutPanel page,int row,params Control[] children) {
            var flow=Ui.Flow();flow.Dock=DockStyle.Fill;flow.Padding=new Padding(0,5,0,0);flow.Controls.AddRange(children);
            Ui.FitRow(page,flow,row);
        }
        private void Describe() {
            var b=Binding;
            if(b==null){info.Text=L.T("当前面板没有已配置的控件绑定。");remove.Enabled=rebind.Enabled=restore.Enabled=false;return;}
            var missing=b.Missing(model).ToArray();
            string create=b.Removal=="leader"?L.T("新建一个姓名文字位置"):L.T("沿用原样式，创建替代控件");
            Ui.Load(replacement,new[]{create}.Concat(Editing.ReplacementControls(model,b,shared)),create);
            if(b.Removal!="leader")name.Text=Editing.ReplacementName(model,b);
            else name.Text="";
            string guidance;
            switch(b.Removal) {
                case "leader":
                    guidance=L.T("移除姓名会同时清理它的悬停区。领袖盾徽仍可查看领袖资料。恢复会补全缺失的姓名并重新对齐悬停区。");
                    break;
                case "primary":
                    guidance=L.T("移除会一起清理余额、月变化和该摘要的提示。体系资源仍按原规则计算。");
                    break;
                case "nomination":
                    guidance=L.T("恢复控件会对齐评分和悬停区。手动布局可以叠放评分和盾徽。移除显示会一起清理评分和悬停区，保留选举计算。");
                    break;
                case "manual":
                    guidance=L.T("移除自动排列框前，工具会把盾徽当前显示位置写入 GUI，转为手动排列。");
                    break;
                case "field":
                    guidance=L.T("这项显示可以移除。工具会同时更新布局引用，仍被其他用途使用的控件会保留。");
                    break;
                default:
                    if(b.Path=="regions.special_members_region.group_heading"||b.Path=="regions.special_members_region.group_tooltip_control")
                        guidance=L.T("每排标题和它的透明悬停区成对移动。「恢复控件…」会把越界的标题移回允许的位置。空间不足时可用「排布选举区…」。");
                    else guidance=L.T("这项控件是当前区域运行所需的。用「更换绑定」先接上替代控件，再清理原控件。若手动删过代码，可用「恢复控件」。");
                    break;
            }
            info.Text=guidance+"\r\n\r\n"+L.T("布局引用 ")+model.Layout.Node.Key+" → "+b.Path+
                (b.Name!=""?" = "+b.Name:"")+"\r\n"+model.Layout.Path+"\r\n"+
                L.T("窗口 ")+model.Window.Node.Get("name")+"  ·  "+model.GuiPath+
                (missing.Length==0?"":"\r\n\r\n"+L.T("缺失或类型不匹配的控件 ")+String.Join(", ",missing))+
                (b.IsGroup?"\r\n\r\n"+L.T("这是一组列表槽位。更换绑定会更新整组，避免列表缺行。"):"");
            remove.Enabled=b.Removal!=""&&b.Name!=""&&(b.Removal!="manual"||missing.Length==0);
            restore.Enabled=b.Removal=="leader"||b.Removal=="nomination"||missing.Length!=0||Editing.CanAlign(b.Path);
            remove.Text=b.Removal=="manual"?L.T("转为手动…"):L.T("移除显示…");
            UpdateReplacement();
        }
        private void UpdateReplacement() {
            var b=Binding;
            if(b==null)return;
            if(replacement.SelectedIndex<0)return;
            name.Visible=nameLabel.Visible=Clone&&b.Removal!="leader";
            rebind.Enabled=!Clone||b.Removal=="leader"||!b.Missing(model).Any();
        }
        private void Done(BindingChoice choice) {
            if(Binding==null)return;
            Choice=choice;DialogResult=DialogResult.OK;Close();
        }
    }
}
