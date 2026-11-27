using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Windows.Forms;

namespace PoliticalEditor {
    internal sealed class VisualEditValidation {
        internal readonly List<Problem> Blocking=new List<Problem>();
        internal readonly List<Problem> Existing=new List<Problem>();

        internal static VisualEditValidation Check(Model current,Plan plan) {
            var revised=new Model(new Mount(current.Mount.Roots,plan.After,CancellationToken.None),current.Type==null?"":current.Type.Node.Key,
                current.Instance==null?"":current.Instance.Id,current.Country,current.Date,current.Language,CancellationToken.None,current.Images);
            var result=new VisualEditValidation();
            var oldManaged=current.Problems.Where(p=>p.Error).ToArray();
            var newManaged=revised.Problems.Where(p=>p.Error).ToArray();
            foreach(var issue in newManaged) {
                bool same=oldManaged.Any(old=>Files.Windows.Equals(old.File,issue.File)&&old.Binding==issue.Binding&&old.Message==issue.Message);
                (same?result.Existing:result.Blocking).Add(issue);
            }
            if(newManaged.Length!=0||!Directory.Exists(current.Mount.Roots.Game))return result;
            var issues=NativeValidation.Run(revised,Program.Temporary);
            if(issues.Count==0)return result;
            var baseline=NativeValidation.Run(current,Program.Temporary);
            var changed=new HashSet<string>(plan.Changes,Files.Windows);
            foreach(var issue in issues) {
                bool same=baseline.Any(old=>Files.Windows.Equals(old.File,issue.File)&&old.LayoutId==issue.LayoutId&&
                    old.ControlId==issue.ControlId&&old.RawDetail==issue.RawDetail&&
                    (issue.PositionDetail!=null?old.PositionDetail==issue.PositionDetail&&old.PositionScope==issue.PositionScope:
                        !changed.Contains(issue.File)));
                (same?result.Existing:result.Blocking).Add(issue);
            }
            return result;
        }

        internal static bool Review(IWin32Window owner,Plan plan,string introduction,IEnumerable<Problem> existing=null) {
            var pending=existing==null?new Problem[0]:existing.ToArray();
            string original=L.T("工程原有问题");
            var changes=plan.Changes.ToArray();
            if(changes.Length==0)return false;
            if(pending.Length!=0) {
                changes=changes.Concat(new[]{original}).ToArray();
                introduction+="\r\n"+L.T("原有问题已列在左侧。本次修改可单独应用，导出前仍需处理原有问题。");
            }
            using(var diff=new DiffDialog(plan.Title,changes,key=>key==original?
                String.Join("\r\n\r\n",pending.Select(issue=>issue.File+"\r\n"+issue.Message)):plan.Diff(key),introduction,true,original)) {
                if(diff.ShowDialog(owner)!=DialogResult.OK)return false;
            }
            plan.Commit();
            return true;
        }
    }
}
