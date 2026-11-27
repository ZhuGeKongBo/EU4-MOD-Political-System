using System;
using System.Drawing;
using System.Windows.Forms;

namespace PoliticalEditor {
    internal sealed class ElectionAreaDialog : Form {
        private readonly NumericUpDown x,y,width,height;
        internal Rectangle Area { get { return new Rectangle((int)x.Value,(int)y.Value,(int)width.Value,(int)height.Value); } }

        internal ElectionAreaDialog(ElectionArea area) {
            Ui.Dialog(this,L.T("排布选举区…"),new Size(680,285));
            var page=Ui.Stack(42,39,39,0,49);page.Padding=new Padding(14,10,14,8);Controls.Add(page);
            page.Controls.Add(Ui.Label(L.T("调整选举列表的位置和大小"),Ui.Heading),0,0);
            x=Number(area.Current.X,0,area.Panel.Width-area.Minimum.Width);
            y=Number(area.Current.Y,0,area.Panel.Height-area.Minimum.Height);
            width=Number(area.Current.Width,area.Minimum.Width,area.Panel.Width);
            height=Number(area.Current.Height,area.Minimum.Height,area.Panel.Height);
            AddRow(page,1,Ui.Label(L.T("左上角 X")),x,Ui.Label(L.T("左上角 Y")),y);
            AddRow(page,2,Ui.Label(L.T("宽度")),width,Ui.Label(L.T("高度")),height);
            var help=Ui.Help(String.Format(L.T("最小 {0} × {1}。扩大宽度可为左右移动留空间，扩大高度可增加每排上下余量。现有盾徽尺寸、每排标题和竖向滑条会一起纳入排布。"),
                area.Minimum.Width,area.Minimum.Height));
            page.Controls.Add(help,0,3);
            var accept=Ui.Button(L.T("预览改动…"),(s,e)=>{
                if(!new Rectangle(Point.Empty,area.Panel).Contains(Area)) {
                    Ui.Error(this,new ArgumentException(L.T("选举区超出了面板。请减小左上角坐标或宽高。")));
                    return;
                }
                DialogResult=DialogResult.OK;
            },true,125);
            var cancel=Ui.Button(L.T("取消"),(s,e)=>DialogResult=DialogResult.Cancel);
            AddRow(page,4,accept,cancel);AcceptButton=accept;CancelButton=cancel;
            Ui.Tip(x,L.T("选举列表左边缘的位置"));Ui.Tip(y,L.T("选举列表上边缘的位置"));
            Ui.Tip(width,L.T("加宽后，左右的盾徽之间有更多空间"));Ui.Tip(height,L.T("加高后，每排之间有更多空间"));
        }

        private static NumericUpDown Number(int value,int minimum,int maximum) {
            return new NumericUpDown { Minimum=minimum,Maximum=maximum,
                Value=Math.Min(maximum,Math.Max(minimum,value)),Width=91,Font=Ui.TextFont,Margin=new Padding(0,4,12,0) };
        }

        private static void AddRow(TableLayoutPanel page,int i,params Control[] fields) {
            var row=Ui.Flow();row.Dock=DockStyle.Fill;row.Controls.AddRange(fields);Ui.FitRow(page,row,i);
        }
    }
}
