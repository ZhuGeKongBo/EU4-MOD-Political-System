using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace PoliticalEditor {
    internal enum FindAction { Next, Previous, Replace, ReplaceAll }

    internal struct FindMatch {
        internal int Index, Number, Total;
        internal bool Wrapped;
    }

    internal static class CodeSearch {
        internal static IEnumerable<int> Matches(string text,string query,bool matchCase) {
            if(query.Length==0)yield break;
            var comparison=matchCase?StringComparison.Ordinal:StringComparison.OrdinalIgnoreCase;
            for(int start=0;start<=text.Length-query.Length;) {
                int found=text.IndexOf(query,start,comparison);
                if(found<0)yield break;
                yield return found;
                start=found+query.Length;
            }
        }
        internal static FindMatch Find(string text,string query,int from,bool previous,bool matchCase) {
            int first=-1,last=-1,choice=-1,count=0,number=0;
            foreach(var index in Matches(text,query,matchCase)) {
                ++count;
                if(first<0)first=index;
                last=index;
                if(previous?index<from:choice<0&&index>=from) { choice=index;number=count; }
            }
            bool wrap=choice<0&&count!=0;
            if(wrap){choice=previous?last:first;number=previous?count:1;}
            return new FindMatch{ Index=choice, Number=number, Total=count, Wrapped=wrap };
        }
        internal static string Replace(string raw,string query,string changed,bool matchCase,int single,out int count,out int first) {
            var visible=Document.ViewText(raw);
            var chosen=single<0?Matches(visible,query,matchCase):new[]{single};
            var output=new StringBuilder(raw.Length);
            string replacement=Document.ViewText(changed).Replace("\n",raw.Contains("\r\n")?"\r\n":"\n");
            int scan=0,view=0,written=0;
            count=0;first=-1;
            foreach(int position in chosen) {
                int start=Advance(raw,ref scan,ref view,position);
                int finish=Advance(raw,ref scan,ref view,position+query.Length);
                output.Append(raw,written,start-written).Append(replacement);
                written=finish;
                if(count==0)first=position;
                ++count;
            }
            if(count==0)return raw;
            output.Append(raw,written,raw.Length-written);
            return output.ToString();
        }
        private static int Advance(string raw,ref int offset,ref int visual,int stop) {
            while(visual<stop&&offset<raw.Length) {
                if(raw[offset++]=='\r'&&offset<raw.Length&&raw[offset]=='\n')++offset;
                ++visual;
            }
            return offset;
        }
    }

    internal sealed class FindReplaceDialog : Form {
        private readonly TextBox query=Ui.Box("",370), replacement=Ui.Box("",370);
        private readonly CheckBox matchCase=new CheckBox{Text=L.T("区分大小写"),AutoSize=true,Margin=new Padding(5,7,5,0)};
        private readonly Label file=Ui.Label("",Ui.Small,Ui.Soft), feedback=Ui.Label("",Ui.Small,Ui.Soft);
        private readonly Button next,previous,replace,replaceAll;
        private readonly Func<FindAction,string,string,bool,string> execute;
        internal FindReplaceDialog(Func<FindAction,string,string,bool,string> run) {
            execute=run;
            Ui.Dialog(this,L.T("查找和替换"),new Size(650,269));
            ShowInTaskbar=false;
            KeyPreview=true;
            var page=Ui.Stack(35,38,38,43,0);
            page.Padding=new Padding(12,9,12,8); Controls.Add(page);
            file.MaximumSize=new Size(620,0);
            page.Controls.Add(file,0,0);
            Row(page,1,Ui.Label(L.T("查找文字")),query,matchCase);
            Row(page,2,Ui.Label(L.T("替换为")),replacement);
            next=Ui.Button(L.T("下一处"),(s,e)=>Run(FindAction.Next),true,79);
            previous=Ui.Button(L.T("上一处"),(s,e)=>Run(FindAction.Previous),false,79);
            replace=Ui.Button(L.T("替换一处"),(s,e)=>Run(FindAction.Replace),false,86);
            replaceAll=Ui.Button(L.T("全部替换"),(s,e)=>Run(FindAction.ReplaceAll),false,86);
            Row(page,3,next,previous,replace,replaceAll,Ui.Button(L.T("关闭"),(s,e)=>Close()));
            feedback.MaximumSize=new Size(620,0);
            page.Controls.Add(feedback,0,4);
            query.TextChanged+=(s,e)=> { feedback.Text=""; };
            AcceptButton=next;
            KeyDown+=(s,e)=>{
                if(e.KeyCode==Keys.Escape){Close();e.SuppressKeyPress=true;}
                if(e.KeyCode==Keys.F3){Run(e.Shift?FindAction.Previous:FindAction.Next);e.SuppressKeyPress=true;}
            };
        }
        private static void Row(TableLayoutPanel page,int row,params Control[] children) {
            var flow=Ui.Flow();flow.Dock=DockStyle.Fill;flow.Padding=new Padding(0,3,0,4);
            flow.Controls.AddRange(children);Ui.FitRow(page,flow,row);
        }
        internal void File(string path,bool canReplace) {
            file.Text=path.Length==0?L.T("当前没有打开代码文件。"):L.T("当前文件 ")+path;
            Ui.Tip(file,file.Text);
            next.Enabled=previous.Enabled=path.Length>0;
            replace.Enabled=replaceAll.Enabled=path.Length>0&&canReplace;
        }
        internal void Start(string selected,bool replacing) {
            if(!String.IsNullOrWhiteSpace(selected)&&selected.Length<=220&&!selected.Contains("\n")&&!selected.Contains("\r"))query.Text=selected;
            var input=replacing&&query.Text.Length>0?replacement:query;
            input.Focus();input.SelectAll();
        }
        internal void Run(FindAction action) {
            if(query.Text.Length==0){feedback.Text=L.T("请填写要查找的文字。");return;}
            try { feedback.Text=execute(action,query.Text,replacement.Text,matchCase.Checked); }
            catch(Exception e){Program.Log(e);feedback.Text=e.Message;}
        }
    }
}
