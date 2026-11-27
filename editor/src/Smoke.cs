using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace PoliticalEditor {
    internal static partial class Smoke {
        internal static int Run(string dir) {
            Directory.CreateDirectory(dir);
            var root=RootMap.Detect();
            var fixture=Environment.GetEnvironmentVariable("PF_EDITOR_BASELINE_MOD_ROOT");
            if(!String.IsNullOrEmpty(fixture))root.Mod=fixture;
            string gameExe=Path.Combine(root.Game,"eu4.exe"), originalHash=Files.FileHash(gameExe);
            var originals=Files.Walk(root.Mod,"").Where(Project.IsModFile).ToDictionary(p=>p,p=>Files.FileHash(Files.At(root.Mod,p)),Files.Windows);
            using(var w=new StreamWriter(Path.Combine(dir,"results.txt"),false,Files.Utf8)) {
                w.AutoFlush=true;
                w.WriteLine("EU4 Political Editor "+Program.Version);
                w.WriteLine("Windows "+Environment.OSVersion.Version+" | .NET Framework CLR "+Environment.Version);
                string beta=Environment.GetEnvironmentVariable("PF_EDITOR_BETA_MOD_ROOT");
                if(!String.IsNullOrWhiteSpace(beta))CheckPublicBeta(root,beta,w);
                CheckCustomOfficeLayout(root,w,dir);
                string compactDraft=Environment.GetEnvironmentVariable("PF_EDITOR_COMPACT_SCORE_DRAFT");
                if(!String.IsNullOrEmpty(compactDraft))CheckCompactScores(Project.Open(compactDraft),w,dir);
                string scoreDraft=Environment.GetEnvironmentVariable("PF_EDITOR_SCORE_MOVE_DRAFT");
                if(scoreDraft!=null)CheckIndependentScoreMove(Project.Open(scoreDraft),w,dir);
                string leaderDraft=Environment.GetEnvironmentVariable("PF_EDITOR_LEADER_REMOVE_DRAFT");
                if(leaderDraft!=null)CheckIndependentLeaderRemoval(Project.Open(leaderDraft),w,dir);
                string electionDraft=Environment.GetEnvironmentVariable("PF_EDITOR_ELECTION_MOVE_DRAFT");
                if(electionDraft!=null)CheckElectionMovement(Project.Open(electionDraft),w,dir);
                string nominationDraft=Environment.GetEnvironmentVariable("PF_EDITOR_NOMINATION_DRAFT");
                if(nominationDraft!=null)CheckMongolIcon(Project.Open(nominationDraft),true,w,dir,null);
                var installed=Project.Import(root,CancellationToken.None);
                installed.Data.Type="shogun_system";installed.Data.Date="1444.11.11";
                var supplied=Check(installed,w,fixture==null?"导入本机 Mod":"导入上版通过验证的 Mod 快照",null);
                string unedited=Path.Combine(dir,"roundtrip.pfproject.json");
                installed.Save(unedited);var roundtrip=Project.Open(unedited);
                Assert(originals.Count==roundtrip.Content.Count&&roundtrip.Content.All(p=>Files.Hash(p.Value.Data)==originals[p.Key]&&!p.Value.Changed),
                    "原样存读工程改变了源文件字节。");
                var recovered=Check(roundtrip,w,"重开既有工程",supplied.Images);
                Assert(recovered.Instance.Leader=="ASK"&&recovered.PrimaryId=="shogun_resource"&&recovered.Amount==55,"未正确读取用户稿的历史或资源。");
                Assert(recovered.Images.Find("GFX_eoc_bg_dom").Any(s=>s.Texture.EndsWith("eoc_bg_domination.dds")),"原版贴图解析失败。");
                w.WriteLine("E01 原始 Mod "+originals.Count+" 个文件均字节级往返。当前实例 ASK 和 shogun_resource=55.00。");
                var north=new Model(recovered.Mount,"pf087_north_type","pf087_north","FRA","1444.11.11","l_english",CancellationToken.None,recovered.Images);
                var south=new Model(recovered.Mount,"pf087_south_type","pf087_south","FRA","1444.11.11","l_english",CancellationToken.None,recovered.Images);
                var nscene=new Scene(north,new PreviewSettings());
                var sscene=new Scene(south,new PreviewSettings());
                Assert(nscene.ByName("primary_resource_value").Tooltip.Contains("北部账册")&&nscene.ByName("primary_resource_value").Tooltip.Contains("18.75")&&
                    nscene.ByName("primary_resource_growth").Text=="+1.25"&&sscene.ByName("primary_resource_value").Tooltip.Contains("南部账册")&&
                    sscene.ByName("primary_resource_growth").Text=="+5.50","同一本地化模板未按当前实例提供数据和封顶明细。");
                w.WriteLine("共享本地化预览  ·  北部 18.75 / +1.25，南部 42.50 / +5.50。参数由内容读取。");
                var starter=Editing.Default(root,"editor_default","editor_beginning","1444.1.1","ASK");
                var seed=Check(starter,w,"默认项目",null);
                var inherited=Editing.FromTemplate(starter,seed,"celestial_style","celestial_layout","charter_child","charter_child_1444","1444.1.1","ASK",false);
                inherited.Commit();starter.Data.Type="charter_child";starter.Data.Instance="charter_child_1444";
                var child=Check(starter,w,"继承模板",seed.Images);
                Assert(child.Chain.Count==2&&child.Rule.Read("","members","source")=="leader_subjects","模板继承未解析。");
                var copied=Editing.FromTemplate(starter,child,"celestial_style","celestial_layout","charter_copy","charter_copy_1444","1444.1.1","ASK",true);
                copied.Commit();starter.Data.Type="charter_copy";starter.Data.Instance="charter_copy_1444";
                var copiedModel=Check(starter,w,"展开并复制模板",child.Images);
                var election=Editing.FromTemplate(starter,copiedModel,"hre_style","hre_layout","editor_old_vote","editor_old_vote_1444","1444.1.1","HAB",false);
                var oldLayout=copiedModel.Layouts["hre_layout"];
                var optional=oldLayout.Node.Child("regions").Child("special_members_region").Child("support_shield");
                election.Text(oldLayout.Path,oldLayout.Doc.Replace(optional.Start,optional.End,""));
                election.Commit();starter.Data.Type="editor_old_vote";starter.Data.Instance="editor_old_vote_1444";starter.Data.Country="HAB";
                var legacy=Check(starter,w,"旧版预设样本",copiedModel.Images);
                Assert(legacy.Window!=null&&legacy.Problems.Any(p=>!p.Error&&p.Message.Contains("小盾徽")),"旧版布局缺小盾徽时未产生可操作的兼容提示。");
                var project=Editing.Default(root,"shogun_system","shogun_system","1444.1.1","ASK",true);
                var model=Check(project,w,"幕府练习",seed.Images);
                w.WriteLine("幕府历史日 "+model.Instance.Date+" | 预览 "+model.Date+" | 成员 "+model.Instance.Members.Count+" | 史料推定 "+model.Instance.SampleMembers);
                string practice=Path.Combine(dir,"Shogun-Practice.pfproject.json");
                project.Save(practice);
                Assert(Project.Open(practice).Content.Count==project.Content.Count,"练习工程重开失败。");
                Assert(L.Read(Program.LanguagePreference)==L.Chinese,"首次启动默认界面语言应为简体中文。");
                WorkbenchSession englishSession;
                using(var form=new Workbench()) {
                    form.Show();Application.DoEvents(); Capture(form,Path.Combine(dir,"start.png"));
                    form.DebugOpen(project,model);Application.DoEvents(); Capture(form,Path.Combine(dir,"workbench.png"));
                    form.CheckTextLayout(w.WriteLine);
                    form.CheckDesktop(w.WriteLine);
                    englishSession=form.CheckSearchAndSwitch(w.WriteLine);
                }
                L.Use(L.Read(Program.LanguagePreference));
                using(var form=new Workbench()) {
                    form.Show();Application.DoEvents();
                    form.CheckEnglish(englishSession,w.WriteLine);
                    Capture(form,Path.Combine(dir,"english-workbench.png"));
                    form.CheckLanguageMenu(L.Chinese,false);
                }
                L.Use(L.Read(Program.LanguagePreference));
                Assert(L.Current==L.Chinese,"重新读取偏好后未切回简体中文。");
                w.WriteLine("English → 简体中文通过同一菜单命令并记录语言偏好。");
                int replaced,first;
                Assert(CodeSearch.Replace("a\r\n# needle\n# needle\r\n","needle","done",true,-1,out replaced,out first)=="a\r\n# done\n# done\r\n"&&replaced==2,
                    "查找替换改变了无关行的换行符。");
                CheckMongolBindings(Project.Open(unedited),w,dir,supplied.Images);
                if(nominationDraft==null)CheckMongolIcon(Project.Open(unedited),false,w,dir,supplied.Images);
                // Use the real imported Mod's work copy for package tests.
                var real=Check(installed,w,"桌面工程后的源 Mod 快照",supplied.Images);
                var mview=new Scene(real,new PreviewSettings());
                var primary=mview.ByName("primary_resource_value");
                var plan=Editing.Move(installed,real,primary,280,130,false);
                var oldWindow=real.Window.Doc.At(real.Window.Node);
                plan.Commit();
                var moved=Check(installed,w,"独立布局坐标",real.Images);
                Assert(moved.Layout.Node.Key=="shogun_system_editor"&&Geometry.Raw(moved.Window.Node.Named(primary.Name)).X==280,"共享布局未独立复制。");
                var baseWindow=moved.Window.Doc.Root.Child("guiTypes").Named("pf075_celestial_panel");
                Assert(oldWindow==moved.Window.Doc.At(baseWindow),"独立编辑修改了共享窗口。");
                var scene=new Scene(moved,new PreviewSettings());
                var reuse=new Plan(installed,moved.Mount,"复用已有入口");
                var other=moved.Images.Find("GFX_pf075_hre_icon").Single();
                Editing.Register(reuse,moved,moved.GfxPath,other.Name,other.Texture,1,null,other);Editing.BindEntry(reuse,moved,other.Name);
                Assert(reuse.Changes.SequenceEqual(new[]{moved.Type.Path}),"已登记入口没有只修改引用。");
                reuse.Commit();var reused=Check(installed,w,"复用已登记原生入口",moved.Images);
                string texture="gfx/interface/shogun_system/entry_01.dds", sprite="GFX_muromachi_entry_custom";
                var bytes=Dds();
                using(var test=Raster.Decode(bytes,".dds"))Assert(test.Width==24&&test.Height==24&&test.HasAlpha,"DDS 解码不正确。");
                var import=new Plan(installed,reused.Mount,"导入幕府入口素材");
                Editing.Register(import,reused,reused.GfxPath,sprite,texture,1,bytes,null);Editing.BindEntry(import,reused,sprite);
                Assert(import.Changes.Count()==4,"导入应同步更新贴图、GFX、HUD GUI 和类型。");
                import.Commit();var custom=Check(installed,w,"导入自有 DDS 并同步 5 个 HUD 槽",null);
                Assert(custom.EntrySprite==sprite&&custom.Images.Find(sprite).Single().Texture==texture,"新入口素材绑定失败。");
                installed.Undo(false);Assert(!installed.Content.ContainsKey(texture),"一次撤销没有移除整次素材登记。");
                installed.Undo(true);var restored=Check(installed,w,"素材事务重做",custom.Images);
                var component=new Plan(installed,restored.Mount,"装饰与只读提示");
                var t=Editing.EnsureVisual(component,restored,false);
                Editing.AddControl(component,restored,t,"装饰","pf_editor_badge",280,130,sprite,"",t.Window,false);
                var readOnlyKey=restored.Rule.Read("","resources",restored.PrimaryId,"tooltip");
                Editing.AddControl(component,restored,t,"提示区","pf_editor_hit",63,602,"",readOnlyKey,t.Window,false);
                bool occupied=false;
                try{Editing.AddControl(component,restored,t,"余额","occupied",254,130,"","",t.Window,false);}catch(InvalidOperationException){occupied=true;}
                Assert(occupied,"占用的主资源槽没有提示显式替换。");
                component.Commit();var plus=Check(installed,w,"装饰与只读数据绑定",restored.Images);
                var view=new Scene(plus,new PreviewSettings());
                using(var guide=new ComponentDialog(installed,plus,view,null,false)) {
                    guide.Show();Application.DoEvents();
                    var captioned=guide.CheckGuidedIcon("这里显示蒙古议会的徽记。");
                    Capture(guide,Path.Combine(dir,"add-guide.png"));
                    Assert(captioned.Changes.Any(file=>file.EndsWith(".yml")),"向导未生成说明文字");
                    captioned.Commit();guide.Close();
                }
                var guided=Check(installed,w,"从向导添加图片和直接输入的说明",plus.Images);
                var guidedScene=new Scene(guided,new PreviewSettings{Playing=true});
                var guideIcon=guidedScene.ByName("pf_editor_icon");
                var guideTip=guidedScene.ByName("pf_editor_icon_pf_hint");
                Assert(guideIcon!=null&&guideTip!=null&&!guideTip.Click&&guideTip.Hit&&guideTip.Tooltip.Contains("蒙古议会")&&
                    guideIcon.Bounds==guideTip.Bounds,"图标和提示区没有对齐");
                var movedGuide=Editing.Move(installed,guided,guideIcon,guideIcon.Raw.X+5,guideIcon.Raw.Y+4,false);
                movedGuide.Commit();guided=Check(installed,w,"图标移动带动提示区",guided.Images);
                guidedScene=new Scene(guided,new PreviewSettings{Playing=true});
                guideIcon=guidedScene.ByName("pf_editor_icon");guideTip=guidedScene.ByName("pf_editor_icon_pf_hint");
                Assert(guideIcon.Bounds==guideTip.Bounds,"图标移动后提示区错位");
                using(var previewCanvas=new PreviewCanvas()) {
                    previewCanvas.Use(guidedScene,null);
                    var pos=new Point(guideIcon.Bounds.Left+1,guideIcon.Bounds.Top+1);
                    Assert(previewCanvas.HitAt(pos,true)==guideIcon&&previewCanvas.HitAt(pos,false)==guideTip,"编辑和试玩的命中对象不正确");
                }
                var removal=Editing.DeleteControl(installed,guided,guideIcon,false);
                removal.Commit();
                var removed=Check(installed,w,"删除图标及其提示绑定",guided.Images);
                var removedScene=new Scene(removed,new PreviewSettings());
                Assert(removedScene.ByName("pf_editor_icon")==null&&removedScene.ByName("pf_editor_icon_pf_hint")==null&&
                    removed.Layout.Node.Child("tooltips").Child("pf_editor_icon_pf_hint")==null,"删除后残留图标或绑定");
                bool protectedBinding=false;
                try{Editing.DeleteControl(installed,removed,removedScene.ByName("primary_resource_value"),false);}
                catch(InvalidOperationException){protectedBinding=true;}
                Assert(protectedBinding,"删除仍在使用的资源组件没有提示改绑");
                installed.Undo(false);
                var restoredIcon=Check(installed,w,"一次撤销恢复图标和提示",guided.Images);
                var undoScene=new Scene(restoredIcon,new PreviewSettings());
                Assert(undoScene.ByName("pf_editor_icon")!=null&&undoScene.ByName("pf_editor_icon_pf_hint").Tooltip.Contains("蒙古议会"),"撤销未恢复图标和提示");
                Editing.DeleteControl(installed,restoredIcon,undoScene.ByName("pf_editor_icon"),false).Commit();
                w.WriteLine("1.0.1 向导、说明文字、图标与提示同步、绑定保护和撤销测试通过。");
                using(var canvas=new PreviewCanvas()) {
                    canvas.Use(view,null);
                    var hit=canvas.HitAt(new Point(287,138),false);
                    Assert(hit!=null&&hit.Name=="primary_resource_value","装饰抢占数字提示。");
                    var readOnly=canvas.HitAt(new Point(65,605),false);
                    Assert(readOnly!=null&&readOnly.Name=="pf_editor_hit"&&!readOnly.Click&&readOnly.Tooltip.Length>0,"只读提示未绑定。");
                }
                var disabled=new Scene(new Model(new Mount(installed.Data.Roots,installed.Content,CancellationToken.None),installed.Data.Type,
                    installed.Data.Instance,"FRA",installed.Data.Date,installed.Data.Language,CancellationToken.None,plus.Images),new PreviewSettings());
                Assert(disabled.ByName("action_button_00").Hidden&&!disabled.ByName("action_readonly_00").Hidden&&disabled.ByName("action_readonly_00").Tooltip.Contains("仅领袖"),"行动置灰时提示或输入有误。");
                string edited=Path.Combine(dir,"Edited-Shogun.pfproject.json");
                installed.Save(edited);var again=Project.Open(edited);
                var next=Check(again,w,"编辑工程重开",null);
                Assert(next.EntrySprite==sprite&&again.Content.ContainsKey(texture)&&Geometry.Raw(next.Window.Node.Named(primary.Name)).X==280,"重开工作副本丢失组件。");
                w.WriteLine("E03 独立布局、HUD 图标、数字/提示命中、置灰行动、跨文件撤销和项目重开通过。");
                var full=new ExportPlan(again,next,dir,"shogun_ui_qa",true,Program.Temporary);
                foreach(var problem in full.Problems)w.WriteLine("完整包预检 "+problem.File+" "+problem);
                string output=full.Write(Program.Temporary);
                var patch=new ExportPlan(again,next,dir,"shogun_ui_patch",false,Program.Temporary);
                string incremental=patch.Write(Program.Temporary);
                var conflict=new ExportPlan(again,next,dir,"shogun_ui_qa",true,Program.Temporary);
                Assert(conflict.Problems.Any(p=>p.Error&&p.Message.Contains("已存在")),"同名导出未阻止。");
                var transferred=again.Data.Roots.Copy();transferred.Mod=Path.Combine(output,"mod","shogun_ui_qa");transferred.User=Path.Combine(dir,"another-eu4-user");
                var imported=Project.Import(transferred,CancellationToken.None);
                imported.Data.Type="shogun_system";imported.Data.Date="1444.11.11";
                var fresh=Check(imported,w,"独立目录重读完整包",null);
                Assert(fresh.EntrySprite==sprite&&File.Exists(Path.Combine(incremental,"files",texture)),"导出包未保留绑定及贴图。");
                var read=Files.Json().Deserialize<ExportRecord>(TextFile.Decode(Files.Read(Path.Combine(output,"export-manifest.json"))).Text);
                Assert(read.Content.All(f=>Files.FileHash(Files.At(output,f.File))==f.Sha256),"导出清单校验不符。");
                Assert(Files.FileHash(gameExe)==originalHash&&originals.All(pair=>Files.FileHash(Files.At(root.Mod,pair.Key))==pair.Value)&&
                    originals.Count==Files.Walk(root.Mod,"").Count(Project.IsModFile),"原游戏或源 Mod 发生变化。");
                w.WriteLine("E04 "+full.Content.Count+" 个完整包文件与 "+patch.Content.Count+" 个更新包文件导出，检查 SHA-256、重读和冲突。源游戏及 "+originals.Count+" 个 Mod 文件不变。");
                var qaRoot=Path.Combine(dir,"qa-simulated-user"); Directory.CreateDirectory(Path.Combine(qaRoot,"logs"));
                string path=Path.Combine(qaRoot,"logs","error.log");
                File.WriteAllText(path,"Old entry click to exclude.\n",Files.Utf8);
                again.Data.Roots.User=qaRoot;
                var qa=QaEvidence.Start(again,new PreviewSettings{Scale=1.3m},"shogun_ui_patch");
                File.AppendAllText(path,"type loaded\nentry click received\npanel create failed because primary_resource_value missing\n",Files.Utf8);
                var newer=QaEvidence.Collect(qa);
                var stages=QaEvidence.Trace(newer);
                Assert(!newer.Contains("Old entry")&&newer.Contains("entry click received")&&stages.Contains("panel create failed")&&qa.Results.All(q=>q.Outcome=="待实测"),
                    "QA 证据时间窗、失败归因或状态错误。");
                w.WriteLine("QA 时间窗从旧偏移读取，捕获到点击之后窗口创建失败，保留 5 个用例待实测。");
                File.WriteAllText(Path.Combine(dir,"QA-Simulated-Example.txt"),stages+"\r\n\r\n"+newer,Files.Utf8);
                w.WriteLine("PASS  ·  Windows EXE、E01–E04 和离线 QA 会话。没有启动 Steam 或 EU4。");
            }
            return 0;
        }

        private static void CheckPublicBeta(RootMap roots,string content,StreamWriter log) {
            var clean=roots.Copy();
            clean.Mod=content;
            var project=Project.Import(clean,CancellationToken.None);
            project.Data.Date="1444.11.11";
            project.Data.Type=project.Data.Instance="shogun_system";
            project.Data.Country="ASK";
            var shogun=Check(project,log,"Beta 0.1 clean Workshop content, Ashikaga 1444",null);
            Assert(shogun.Instance.Leader=="ASK"&&shogun.EntrySprite=="GFX_pf075_celestial_icon"&&
                shogun.Window.Node.Get("name")=="pf075_celestial_panel"&&shogun.Amount==55,"Wrong Shogunate starting configuration.");
            project.Data.Type=project.Data.Instance="mongol_system";
            project.Data.Country="KHA";
            var mongol=Check(project,log,"Beta 0.1 clean Workshop content, Mongolia 1444",shogun.Images);
            Assert(mongol.Instance.Leader=="KHA"&&mongol.EntrySprite=="GFX_pf075_hre_icon"&&
                mongol.Window.Node.Get("name")=="pf_mongol_system_editor"&&mongol.Instance.Roles["tumen"].Count==6,"Wrong Mongol starting configuration.");
            log.WriteLine("Public content loaded by the shipping editor. Expected leaders, entry sprites, layouts and dated role assignments.");
        }
        private static void CheckMongolBindings(Project project,StreamWriter log,string directory,Sprites images) {
            project.Data.Type="mongol_system";project.Data.Instance="";
            var original=Check(project,log,"蒙古面板原始快照",images);
            var name=new Scene(original,new PreviewSettings()).ByName("leader_name");
            Editing.Move(project,original,name,312,262,false).Commit();
            var moved=Check(project,log,"移动领袖姓名与配套悬停区",images);
            var window=moved.Window.Node;
            string label=Editing.LeaderName(moved), hit=Editing.LeaderSurface(moved);
            Assert(moved.Layout.Node.Key=="mongol_system_editor"&&Geometry.Raw(window.Named(hit))==new Point(312,262),
                "当前体系未独立修改，或悬停区未随姓名移动。");
            var misalign=new Plan(project,moved.Mount,"模拟代码直接移动文字但未移动悬停区");
            misalign.Text(moved.GuiPath,moved.Window.Doc.Position(window.Named(hit),47,200));
            misalign.Commit();
            var broken=BuildModel(project,images);
            var issues=NativeValidation.Run(broken,Program.Temporary).ToArray();
            const string rawGui="text must fit inside its read-only tooltip surface: instantTextBoxType";
            var cause=issues.SingleOrDefault(e=>e.RawDetail==rawGui);
            var effect=issues.SingleOrDefault(e=>e.RawDetail=="missing or invalid interface.layout"&&e.File.EndsWith("Mongol_ulus.txt",StringComparison.OrdinalIgnoreCase));
            Assert(cause!=null&&cause.Binding==ControlBindings.LeaderLabel&&cause.LayoutId==moved.Layout.Node.Key&&
                cause.Message.Contains("恢复控件")&&!cause.Message.Contains(rawGui),
                "原生悬停区错误未关联到当前布局的领袖姓名，或缺少中文修复步骤。\n"+String.Join("\n",issues.Select(e=>e.File+" "+e.Message)));
            Assert(effect!=null&&effect.LayoutId==cause.LayoutId&&effect.Message.Contains("先处理同一布局"),"体系后续错误未关联到需修复的布局。");
            log.WriteLine("两条原生报错在蒙古面板工作副本中实际复现。显示给用户的内容如下。");
            log.WriteLine(cause.File+" "+cause.Message);log.WriteLine(effect.File+" "+effect.Message);
            using(var repair=new BindingDialog(broken,cause.Binding,false)) {
                repair.Show();Application.DoEvents();
                Assert(repair.Binding.Path==ControlBindings.LeaderLabel,"绑定窗口没有选中实际出错的组件。");
                Capture(repair,Path.Combine(directory,"binding-repair.png"));repair.Close();
            }
            Editing.RestoreControl(project,broken,moved,Binding(broken,ControlBindings.LeaderLabel),false).Commit();
            moved=Check(project,log,"恢复控件自动对齐并清除连带错误",images);
            var drop=new Plan(project,moved.Mount,"模拟用户在 GUI 源码中删除领袖姓名");
            var removed=moved.Window.Node.Named(label);
            drop.Text(moved.GuiPath,moved.Window.Doc.Replace(removed.Start,removed.End,""));drop.Commit();
            string draft=Path.Combine(directory,"Mongol-Deleted-Label.pfproject.json");
            project.Save(draft);project=Project.Open(draft);
            var deleted=BuildModel(project,images);
            Assert(deleted.Problems.Any(p=>p.Error&&p.Binding==ControlBindings.LeaderLabel),"已删姓名的旧草稿未提供可恢复绑定。");
            Editing.RestoreControl(project,deleted,null,Binding(deleted,ControlBindings.LeaderLabel),false).Commit();
            var restored=Check(project,log,"重开草稿后恢复已删除的姓名",images);
            string leaderShield=restored.Layout.Node.Child("regions").Child("leader_region").Get("control");
            Editing.RemoveBinding(project,restored,null,null,Binding(restored,ControlBindings.LeaderLabel),false).Commit();
            var hidden=Check(project,log,"移除姓名及悬停绑定，保留领袖盾徽",images);
            Assert(Editing.LeaderName(hidden)==""&&Editing.LeaderSurface(hidden)==""&&hidden.Window.Node.Named(label)==null&&
                hidden.Window.Node.Named(hit)==null&&hidden.Window.Node.Named(leaderShield)!=null,"领袖盾徽或姓名绑定的清理有误。");
            project.Undo(false);
            var undone=Check(project,log,"一次撤销恢复文字和悬停区",images);
            Editing.RebindControl(project,undone,null,Binding(undone,ControlBindings.LeaderLabel),"",true,false).Commit();
            var rebound=Check(project,log,"把姓名更换到一个新文字控件",images);
            Assert(Editing.LeaderName(rebound)!=label&&rebound.Window.Node.Named(label)==null,
                "新姓名绑定没有清理旧控件。");

            var title=Binding(rebound,"title_control");
            Editing.RemoveBinding(project,rebound,null,null,title,false).Commit();
            var noTitle=Check(project,log,"移除面板标题及引用，共用神罗布局保留",images);
            Assert(noTitle.Layout.Node.Child(title.Path)==null&&noTitle.Window.Node.Named(title.Name)==null&&
                noTitle.Window.Doc.Root.Child("guiTypes").Named(original.Window.Node.Get("name")).Named(title.Name)!=null,"移除标题时改动了其它体系或未清理引用。");
            var amount=Binding(noTitle,"regions.primary_resource_region.value_control");
            Editing.RemoveBinding(project,noTitle,null,null,amount,false).Commit();
            var noPrimary=Check(project,log,"移除成组的资源摘要控件，保留资源脚本",images);
            Assert(noPrimary.Layout.Node.Child("regions").Child("primary_resource_region")==null&&noPrimary.Window.Node.Named(amount.Name)==null&&
                noPrimary.Rule.Read("","resources",noTitle.PrimaryId,"name_key")==noTitle.Rule.Read("","resources",noTitle.PrimaryId,"name_key"),
                "移除余额显示时未清理整组布局，或更改了资源规则。");
            var members=Binding(noPrimary,"regions.members_region.control");
            bool required=false;
            try{Editing.RemoveBinding(project,noPrimary,null,null,members,false);}catch(InvalidOperationException e){required=e.Message.Contains("需要该绑定");}
            Assert(required,"删除必需组时未引导改绑。");
            Editing.RebindControl(project,noPrimary,null,members,"members_rebound",true,false).Commit();
            var newMembers=Check(project,log,"同时改绑所有成员盾徽槽位",images);
            var changed=Binding(newMembers,members.Path);
            Assert(changed.Count>1&&changed.Name=="members_rebound"&&!changed.Missing(newMembers).Any()&&
                !members.Names.Any(old=>newMembers.Window.Node.Named(old)!=null),"改绑盾徽组时遗留旧控件或缺槽。");
            var dropSlot=new Plan(project,newMembers.Mount,"模拟删除成员组中的一个盾徽");
            var slot=newMembers.Window.Node.Named(changed.Names.Last());
            dropSlot.Text(newMembers.GuiPath,newMembers.Window.Doc.Replace(slot.Start,slot.End,""));dropSlot.Commit();
            var incomplete=BuildModel(project,images);
            Assert(incomplete.Problems.Any(e=>e.Binding==members.Path&&e.Message.Contains(slot.Get("name"))),"未指出成员组内准确缺失的盾徽。");
            Editing.RestoreControl(project,incomplete,newMembers,Binding(incomplete,members.Path),false).Commit();
            Check(project,log,"恢复缺失的成员盾徽槽位",images);
        }
        private static ControlBinding Binding(Model model,string field) { return ControlBindings.Read(model).Single(b=>b.Path==field); }
        private static void CheckCompactScores(Project project,StreamWriter log,string directory) {
            var model=BuildModel(project,null);
            var baseline=NativeValidation.Run(model,Program.Temporary);
            const string scoreName="elector_score_02",shieldName="office_flag_02",badgeName="elector_support_02";
            var original=project.Content[model.GuiPath].Data;
            var scoreStart=Geometry.Raw(model.Window.Node.Named(scoreName));
            var shieldStart=Geometry.Raw(model.Window.Node.Named(shieldName));
            var badgeStart=Geometry.Raw(model.Window.Node.Named(badgeName));
            var beforeScene=new Scene(model,new PreviewSettings());
            var beforeRange=Editing.ElectionPlacement(model,beforeScene.ByName(scoreName));
            Assert(beforeRange!=null&&beforeRange.Areas.Any(a=>a.Contains(new Point(shieldStart.X+35,shieldStart.Y+52))),
                "手动布局仍限制了评分和盾徽的重叠。");
            using(var form=new Workbench()) {
                form.Show();Application.DoEvents();form.DebugOpen(project,model);Application.DoEvents();
                Capture(form,Path.Combine(directory,"score-before-compact.png"));
                Exception failure=null;bool accepted=false;
                var action=AllControls(form).OfType<Button>().Single(b=>b.Text=="评分贴右下…");
                using(var timer=new System.Windows.Forms.Timer{Interval=80}) {
                    timer.Tick+=(s,e)=>{
                        var dialog=Application.OpenForms.Cast<Form>().LastOrDefault(f=>f.Visible&&f!=form);
                        if(dialog==null)return;
                        timer.Stop();
                        try {
                            Assert(dialog is DiffDialog,"评分布局改动没有显示差异。");
                            Assert(Files.Same(project.Content[model.GuiPath].Data,original),"预览改动时写入了项目。");
                            AllControls(dialog).OfType<Button>().Single(b=>b.Text=="确认应用").PerformClick();
                            accepted=true;
                        } catch(Exception problem) {failure=problem;dialog.Close();}
                    };
                    timer.Start();action.PerformClick();
                }
                if(failure!=null)throw failure;
                Assert(accepted&&!Files.Same(project.Content[model.GuiPath].Data,original),"没有应用评分排布。");
                var compact=BuildModel(project,model.Images);
                var window=compact.Window.Node;
                var score=window.Named(scoreName);var scoreAt=Geometry.Raw(score);
                var shield=window.Named(shieldName);var shieldAt=Geometry.Raw(shield);
                var badge=window.Named(badgeName);var badgeAt=Geometry.Raw(badge);
                var shieldSize=Geometry.SizeOf(shield,Size.Empty);
                Assert(score.Get("format")=="left"&&shieldAt==shieldStart&&badgeAt==badgeStart&&
                    scoreAt==new Point(shieldAt.X+(shieldSize.Width*50+46)/92,shieldAt.Y+(shieldSize.Height*75+46)/92),
                    "评分未叠在大盾徽右下角，或移动了两枚盾徽。");
                Assert(Enumerable.Range(0,8).All(i=>{
                    var suffix="_"+i.ToString("D2");
                    var flag=window.Named("office_flag"+suffix);
                    var value=window.Named("elector_score"+suffix);
                    return Geometry.Raw(value).X<Geometry.Raw(flag).X+Geometry.SizeOf(flag,Size.Empty).Width;
                }),"仍有评分被放在对应盾徽右边界之外。");
                var issues=NativeValidation.Run(compact,Program.Temporary);
                Assert(issues.All(p=>p.LayoutId!=compact.Layout.Node.Key&&baseline.Any(old=>old.File==p.File&&old.RawDetail==p.RawDetail)),
                    "收近评分后出现原生校验错误。"+String.Join("\n",issues.Select(p=>p.Message)));
                log.WriteLine("按真实按钮预览并确认。"+scoreName+" "+scoreStart+" → "+scoreAt+"，大盾徽及小盾徽横坐标仍为 "+shieldAt.X+"、"+badgeAt.X+"。八个评分槽全部收进大盾徽右边界以内。");
                Application.DoEvents();
                Capture(form,Path.Combine(directory,"score-compact.png"));

                var movedTo=form.CheckElectionDrag(log.WriteLine,scoreName,new Point(scoreAt.X-3,scoreAt.Y),false);
                var moved=BuildModel(project,model.Images);
                var preview=new Scene(moved,new PreviewSettings{Playing=true});
                var item=preview.ByName(scoreName);
                using(var canvas=new PreviewCanvas()) {
                    canvas.Use(preview,null);
                    Assert(item.Raw==movedTo&&movedTo.X<scoreAt.X&&movedTo.Y==scoreAt.Y&&
                        canvas.HitAt(Point.Add(movedTo,new Size(2,2)),false)==item&&!item.Click&&item.Tooltip.Length>0,
                        "向左拖动后的评分悬停无效或带有点击。");
                }
                Assert(Geometry.Raw(moved.Window.Node.Named(shieldName))==shieldAt&&Geometry.Raw(moved.Window.Node.Named(badgeName))==badgeAt&&
                    NativeValidation.Run(moved,Program.Temporary).All(p=>p.LayoutId!=moved.Layout.Node.Key),"独立左移改变了盾徽，或未通过原生检查。");
                log.WriteLine("评分向左单独拖动成功。新位置可悬停，盾徽不动，蒙古面板通过原生内容校验。");
                project.Undo(false);project.Undo(false);
                Assert(Files.Same(project.Content[model.GuiPath].Data,original),"撤销未恢复原 GUI。");
                project.Dirty=false;form.Close();
            }
        }

        private static void CheckIndependentScoreMove(Project project,StreamWriter log,string directory) {
            var model=BuildModel(project,null);
            var baseline=NativeValidation.Run(model,Program.Temporary);
            const string scoreName="elector_score_02";
            const string hitName="pf_editor_score_hit_02";
            var originalScore=model.Window.Node.Named(scoreName);
            var initial=Geometry.Raw(originalScore);
            var original=project.Content[model.GuiPath].Data;
            Point placed;
            using(var form=new Workbench()) {
                form.Show();Application.DoEvents();form.DebugOpen(project,model);Application.DoEvents();
                placed=form.CheckElectionDrag(log.WriteLine,scoreName,new Point(initial.X-19,initial.Y-12),false);
                Capture(form,Path.Combine(directory,"score-placement.png"));
                project.Dirty=false;form.Close();
            }
            var moved=BuildModel(project,model.Images);
            Assert(Geometry.Raw(moved.Window.Node.Named(scoreName))==placed&&placed!=initial,"评分没有单独移动。");
            foreach(var control in model.Window.Node.Nodes.Where(n=>n.Block&&n.Get("name")!=""&&n.Get("name")!=scoreName))
                Assert(model.Window.Doc.At(control)==moved.Window.Doc.At(moved.Window.Node.Named(control.Get("name"))),"移动评分改变了其它控件 "+control.Get("name"));
            var after=NativeValidation.Run(moved,Program.Temporary);
            Assert(after.All(issue=>baseline.Any(prior=>prior.File==issue.File&&prior.RawDetail==issue.RawDetail&&
                prior.ControlId==issue.ControlId&&prior.PositionDetail==issue.PositionDetail)),"独立移动评分造成了新的原生错误。");
            log.WriteLine("从蒙古自动备份验证 "+scoreName+"。实际左移只涉及评分文字。其它控件逐块比较一致。");
            project.Undo(false);
            Assert(Files.Same(project.Content[model.GuiPath].Data,original),"一次撤销未恢复评分旧位置。");

            var separate=new Plan(project,model.Mount,"验证独立的评分悬停控件");
            var scoreBinding=Binding(model,Editing.NominationScore);
            separate.Set(model.Layout.Path,model.Layout.Node.Key,new[]{"regions","special_members_region","nomination_tooltip_control"},"pf_editor_score_hit");
            for(int i=0;i<scoreBinding.Count;i++) {
                var doc=separate.Read(model.GuiPath);
                var win=doc.Root.Child("guiTypes").Named(model.Window.Node.Get("name"));
                var score=win.Named(scoreBinding.ControlName(scoreBinding.Name,i));
                var pos=Geometry.Raw(score);var size=Geometry.SizeOf(score,Size.Empty);
                string block="guiButtonType = { name = \"pf_editor_score_hit_"+i.ToString("D2")+"\" position = { x = "+pos.X+" y = "+pos.Y+
                    " } size = { x = "+size.Width+" y = "+size.Height+" } quadTextureSprite = \"GFX_pf075_transparent_hit\" no_clicksound = yes }";
                separate.Text(model.GuiPath,doc.Set(score,"alwaystransparent","yes"));
                doc=separate.Read(model.GuiPath);
                separate.Text(model.GuiPath,doc.Add(doc.Root.Child("guiTypes").Named(model.Window.Node.Get("name")),block));
            }
            separate.Commit();
            var paired=BuildModel(project,model.Images);
            var offices=Binding(paired,"regions.special_members_region.group_tooltip_control");
            Editing.RestoreControl(project,paired,paired,offices,false).Commit();
            paired=BuildModel(project,model.Images);
            var scene=new Scene(paired,new PreviewSettings());
            var hover=scene.ByName(hitName);
            var range=Editing.ElectionPlacement(paired,hover);
            Assert(range!=null&&range.MovingControls.Contains(scoreName)&&range.MovingControls.Contains(hitName),"选中悬停区时没有关联到评分文字。");
            var shifted=new Point(initial.X-14,initial.Y-12);
            Assert(range.Areas.Any(r=>r.Contains(shifted)),"配套悬停区没有单独可放的位置。");
            Editing.Move(project,paired,hover,shifted.X,shifted.Y,false).Commit();
            var linked=BuildModel(project,model.Images);
            Assert(Geometry.Raw(linked.Window.Node.Named(scoreName))==shifted&&Geometry.Raw(linked.Window.Node.Named(hitName))==shifted,
                "选择透明区移动时文字和悬停区没有同步。");
            Assert(NativeValidation.Run(linked,Program.Temporary).All(e=>e.LayoutId!=linked.Layout.Node.Key),"新评分命中位置未通过蒙古布局原生检查。");
            var preview=new Scene(linked,new PreviewSettings{Playing=true});
            using(var canvas=new PreviewCanvas()) {
                canvas.Use(preview,null);
                var target=preview.ByName(hitName);
                Assert(canvas.HitAt(Point.Add(shifted,new Size(2,2)),false)==target&&target.Tooltip.Length!=0&&!target.Click,
                    "评分悬停在新位置没有提示，或错误绑定了点击。");
            }
            log.WriteLine("验证独立悬停控件的布局。评分和透明区一起移到 "+shifted+"，新位置能悬停，蒙古布局通过原生校验。");
        }
        private static void CheckIndependentLeaderRemoval(Project project,StreamWriter log,string directory) {
            var model=BuildModel(project,null);
            var originalHeading=new Plan(project,model.Mount,"制造一个超出面板的职位标题");
            var input=model.Window.Doc;
            var window=model.Window.Node;
            var hit=window.Named("office_group_hit_00");
            var hitAt=Geometry.Raw(hit);
            originalHeading.Text(model.GuiPath,input.Position(hit,hitAt.X,-3));
            input=originalHeading.Read(model.GuiPath);
            originalHeading.Text(model.GuiPath,input.Position(input.Root.Child("guiTypes").Named(window.Get("name")).Named("office_group_title_00"),hitAt.X,-3));
            originalHeading.Commit();
            model=BuildModel(project,model.Images);
            var baseline=NativeValidation.Run(model,Program.Temporary);
            var old=baseline.SingleOrDefault(p=>p.ControlId=="office_group_hit_00"&&p.RawDetail!=null&&
                p.RawDetail.StartsWith(Editing.OfficePanelBounds,StringComparison.Ordinal));
            Assert(old!=null,"手动标题越界未定位到原控件。"+String.Join("\n",baseline.Select(p=>p.Message)));
            Assert(old.PositionDetail.Contains("(44, -3)")&&old.PositionDetail.Contains("面板大小"),"没有指出实际控件与面板尺寸。");
            log.WriteLine("在蒙古副本创建一个真正的画布越界。"+old.Message);
            var guiPath=model.GuiPath;
            byte[] original=project.Content[guiPath].Data;

            using(var form=new Workbench()) {
                form.Show();Application.DoEvents();form.DebugOpen(project,model);Application.DoEvents();
                var chooser=AllControls(form).OfType<ComboBox>().Single(c=>c.Items.OfType<VisualItem>().Any(i=>i.Name=="leader_name"));
                chooser.SelectedItem=chooser.Items.OfType<VisualItem>().Single(i=>i.Name=="leader_name");
                Exception failure=null;int stage=0;
                using(var timer=new System.Windows.Forms.Timer{Interval=80}) {
                    timer.Tick+=(s,e)=>{
                        var dialog=Application.OpenForms.Cast<Form>().LastOrDefault(f=>f.Visible&&f!=form);
                        if(dialog==null)return;
                        try {
                            if(stage==0&&dialog is BindingDialog) {
                                Assert(((BindingDialog)dialog).Binding.Path==ControlBindings.LeaderLabel,"删除按钮未选中领袖姓名绑定。");
                                stage++;
                                AllControls(dialog).OfType<Button>().Single(b=>b.Text=="移除显示…").PerformClick();
                            } else if(stage==1&&dialog is DiffDialog) {
                                timer.Stop();stage++;
                                var entries=AllControls(dialog).OfType<ListBox>().Single();
                                Assert(entries.Items.Contains("工程原有问题"),"差异窗口未区分已有问题。");
                                entries.SelectedItem="工程原有问题";
                                var details=AllControls(dialog).OfType<TextBox>().Single();
                                Assert(details.WordWrap&&details.Text.Contains("(44, -3)")&&details.Text.Contains("面板大小"),"已有问题缺少详细坐标或文字换行。");
                                Assert(Files.Same(project.Content[guiPath].Data,original),"确认前修改了草稿。");
                                Capture(dialog,Path.Combine(directory,"leader-remove-original-issue.png"));
                                AllControls(dialog).OfType<Button>().Single(b=>b.Text=="确认应用").PerformClick();
                            }
                        } catch(Exception problem) {failure=problem;timer.Stop();dialog.Close();}
                    };
                    timer.Start();AllControls(form).OfType<Button>().Single(b=>b.Text=="删除组件…").PerformClick();
                }
                if(failure!=null)throw failure;
                Assert(stage==2,"删除没有进入差异确认。");
                project.Dirty=false;form.Close();
            }
            var removed=BuildModel(project,model.Images);
            Assert(removed.Window.Node.Named("leader_name")==null&&removed.Window.Node.Named("leader_history_hover")==null&&
                removed.Window.Node.Named("leader_flag")!=null&&Editing.LeaderName(removed)=="","只删除姓名的绑定清理不正确。");
            var pending=NativeValidation.Run(removed,Program.Temporary);
            var same=pending.Single(p=>p.ControlId==old.ControlId);
            Assert(same.PositionDetail==old.PositionDetail&&same.PositionScope==old.PositionScope,"删除姓名更改了原有职位区问题。");
            log.WriteLine("真实按钮完成删除。姓名和专用悬停区已移除，领袖盾徽保留。原有问题显示于差异窗口。");
            log.WriteLine(same.Message);
            var fix=Editing.RestoreControl(project,removed,removed,Binding(removed,same.Binding),false);
            var check=VisualEditValidation.Check(removed,fix);
            Assert(check.Blocking.Count==0&&!check.Existing.Any(p=>p.LayoutId==removed.Layout.Node.Key),
                "单独恢复标题仍有新错误。"+String.Join("\n",check.Blocking.Concat(check.Existing).Select(p=>p.Message)));
            fix.Commit();
            var corrected=BuildModel(project,model.Images);
            Assert(Geometry.Raw(corrected.Window.Node.Named("office_group_hit_00"))==new Point(hitAt.X,0)&&
                Geometry.Raw(corrected.Window.Node.Named("office_group_title_00"))==new Point(hitAt.X,0)&&
                Geometry.Raw(corrected.Window.Node.Named("office_heading"))==Geometry.Raw(model.Window.Node.Named("office_heading")),
                "恢复位置时移动了总标题，或第一排未对齐。");
            var after=NativeValidation.Run(corrected,Program.Temporary);
            Assert(after.All(p=>p.LayoutId!=corrected.Layout.Node.Key),"蒙古布局修复后仍未通过原生检查。");
            log.WriteLine("恢复标题和悬停区到 Y=0，总标题保持原坐标。蒙古布局原生校验通过。");
            foreach(var other in after)log.WriteLine("其它问题 "+other.File+" "+other.Message);
            project.Undo(false);project.Undo(false);
            Assert(Files.Same(project.Content[guiPath].Data,original),"两次撤销没有恢复工程原字节。");
        }
        private static void CheckElectionMovement(Project project,StreamWriter log,string directory) {
            var model=BuildModel(project,null);
            var scene=new Scene(model,new PreviewSettings());
            var first=scene.ByName("office_flag_00");
            var before=Editing.ElectionPlacement(model,first);
            Assert(before!=null&&before.Areas.Any(box=>box.Contains(new Point(first.Raw.X,180))),"手动盾徽仍受排标题限制。");
            var baseline=NativeValidation.Run(model,Program.Temporary);
            Assert(baseline.All(p=>p.LayoutId!=model.Layout.Node.Key),"旧式标题顺序仍被当成错误。");
            var area=Editing.ReadElectionArea(model);
            var larger=new Rectangle(area.Current.X,area.Current.Y,area.Current.Width+30,area.Current.Height+24);
            log.WriteLine("已编辑蒙古稿的原坐标，允许把大盾徽移到标题上方，无需先重排。");
            var original=project.Content[model.GuiPath].Data;
            using(var form=new Workbench()) {
                form.Show();Application.DoEvents();form.DebugOpen(project,model);Application.DoEvents();
                Exception failure=null;int accepted=0;
                using(var timer=new System.Windows.Forms.Timer{Interval=80}) {
                    timer.Tick+=(s,e)=>{
                        var dialog=Application.OpenForms.Cast<Form>().LastOrDefault(f=>f.Visible&&f!=form);
                        if(dialog==null)return;
                        try {
                            if(accepted==0&&dialog is ElectionAreaDialog) {
                                var entries=AllControls(dialog).OfType<NumericUpDown>().ToArray();
                                var values=new[]{larger.X,larger.Y,larger.Width,larger.Height};
                                for(int i=0;i<values.Length;i++)entries[i].Value=values[i];
                                Assert(((ElectionAreaDialog)dialog).Area==larger,"排布窗口读入的坐标错误。");
                                Capture(dialog,Path.Combine(directory,"election-area.png"));
                                accepted++;
                                AllControls(dialog).OfType<Button>().Single(b=>b.Text=="预览改动…").PerformClick();
                            } else if(accepted==1&&dialog is DiffDialog) {
                                timer.Stop();accepted++;
                                Assert(Files.Same(project.Content[model.GuiPath].Data,original),"确认前已经改写了源草稿。");
                                AllControls(dialog).OfType<Button>().Single(b=>b.Text=="确认应用").PerformClick();
                            }
                        } catch(Exception problem) { failure=problem;timer.Stop();dialog.Close(); }
                    };
                    timer.Start();AllControls(form).OfType<Button>().Single(b=>b.Text=="排布选举区…").PerformClick();
                }
                if(failure!=null)throw failure;
                Assert(accepted==2&&!Files.Same(project.Content[model.GuiPath].Data,original),"排布向导没有应用预览。");
                var arranged=BuildModel(project,model.Images);
                var issues=NativeValidation.Run(arranged,Program.Temporary);
                Assert(issues.All(p=>p.LayoutId!=arranged.Layout.Node.Key&&baseline.Any(old=>old.File==p.File&&old.RawDetail==p.RawDetail)),
                    "新版排布留下了蒙古面板报错或引入了新的原生错误。"+String.Join("\n",issues.Select(p=>p.Message)));
                log.WriteLine("实际排布按钮将原 "+area.Current.Width+" × "+area.Current.Height+" 展至 "+larger.Width+" × "+larger.Height+"。蒙古布局原生校验通过。");
                foreach(var p in issues)log.WriteLine("已有且保留的其他文件问题 "+p.File+" "+p.Message);
                scene=new Scene(arranged,new PreviewSettings());
                for(int i=0;i<8;i++) {
                    var shield=scene.ByName("office_flag_0"+i);
                    var range=Editing.ElectionPlacement(arranged,shield);
                    Assert(range!=null&&range.Areas.Sum(r=>r.Width*r.Height)>1&&range.Areas.Any(r=>r.Contains(shield.Raw)),
                        "扩展后盾徽仍无法移动 "+shield.Name);
                }
                var point=form.CheckElectionDrag(log.WriteLine,"office_flag_00",new Point(first.Raw.X,180),false);
                Capture(form,Path.Combine(directory,"election-placement.png"));
                var moved=BuildModel(project,model.Images);
                Assert(Geometry.Raw(moved.Window.Node.Named("office_flag_00"))==point&&
                    NativeValidation.Run(moved,Program.Temporary).All(p=>p.LayoutId!=moved.Layout.Node.Key),"贴靠后的盾徽位置未通过原生验证。");
                var shared=arranged.Window.Doc.Root.Child("guiTypes").Named("pf075_hre_panel");
                Assert(arranged.Window.Doc.At(shared)==model.Window.Doc.At(model.Window.Doc.Root.Child("guiTypes").Named("pf075_hre_panel")),"排布意外修改了共用神罗窗口。");
                project.Undo(false);project.Undo(false);
                Assert(Files.Same(project.Content[model.GuiPath].Data,original),"两次撤销未恢复拖动和排布之前的 GUI。");
                project.Dirty=false;form.Close();
            }
            log.WriteLine("八枚盾徽可独立移动。验证跨排拖动、原生预检和两次撤销。");
        }
        private static void CheckMongolIcon(Project project,bool fromRecovery,StreamWriter log,string directory,Sprites images) {
            project.Data.Type="mongol_system";project.Data.Instance="";
            var model=BuildModel(project,images);
            images=model.Images;
            if(!fromRecovery) {
                var damage=new Plan(project,model.Mount,"复现旧版只移动选举盾徽");
                var target=Editing.EnsureVisual(damage,model,false,true);
                var gui=damage.Read(target.Gui);
                var window=gui.Root.Child("guiTypes").Named(target.Window);
                damage.Text(target.Gui,gui.Position(window.Named("office_flag_00"),108,319));
                damage.Commit();model=BuildModel(project,images);
            }
            var issues=NativeValidation.Run(model,Program.Temporary);
            Assert(issues.All(e=>e.LayoutId!=model.Layout.Node.Key),"有意叠放仍被当成了位置错误。"+String.Join("\n",issues.Select(p=>p.Message)));
            var source=project.Content[model.GuiPath].Data;
            var oldShield=Geometry.Raw(model.Window.Node.Named("office_flag_00"));
            var oldScore=Geometry.Raw(model.Window.Node.Named("elector_score_00"));
            log.WriteLine("保留手动叠放盾徽 "+oldShield+" 和评分 "+oldScore+"。开始实测添加组件向导。");
            var scene=new Scene(model,new PreviewSettings{Playing=true});
            string background=model.Layout.Node.Get("background_control"),hover=model.Layout.Node.Get("hover_control");
            var iconSize=model.Images.SizeOf("GFX_pf075_hre_icon",new Size(32,32));
            var blank=(from top in Enumerable.Range(365,120)
                       from left in Enumerable.Range(330,260)
                       let box=new Rectangle(new Point(left,top),iconSize)
                       where new Rectangle(Point.Empty,scene.Panel).Contains(box)&&
                             !scene.Items.Any(item=>!item.Hidden&&item.Name!=background&&item.Name!=hover&&item.Bounds.IntersectsWith(box))
                       select box.Location).First();
            Point placed=Point.Empty;
            using(var guide=new ComponentDialog(project,model,scene,null,false)) {
                Exception failure=null;
                guide.Shown+=(s,e)=>guide.BeginInvoke((Action)(()=>{
                    try {
                        guide.PrepareGuidedIcon("大汗行动");
                        ClickWizardDialog(guide,"在预览图上点位置…",form=>{
                            var canvas=form.Controls.OfType<PreviewCanvas>().Single();
                            canvas.Refresh();
                            var pixel=canvas.ClientAt(blank);
                            placed=canvas.Local(pixel);
                            typeof(Control).GetMethod("OnMouseClick",BindingFlags.Instance|BindingFlags.NonPublic)
                                .Invoke(canvas,new object[]{new MouseEventArgs(MouseButtons.Left,1,pixel.X,pixel.Y,0)});
                            Assert(form.DialogResult==DialogResult.OK,"选点窗口未接收面板空白处坐标 "+placed+"，当前提示 "+form.Text);
                        });
                        log.WriteLine("向导的选点预览已接受空白处坐标 "+placed+"。");
                        ClickWizardDialog(guide,"预览改动…",form=>{
                            var summary=AllControls(form).OfType<Label>().FirstOrDefault(l=>l.Text.Contains("同时修复选举区"));
                            Assert(form is DiffDialog&&summary==null,"添加组件改动了手动叠放的选举区域。");
                            Assert(Files.Same(project.Content[model.GuiPath].Data,source),"确认前就修改了草稿。");
                            Capture(form,Path.Combine(directory,"nomination-add-preview.png"));
                            AllControls(form).OfType<Button>().Single(b=>b.Text=="确认应用").PerformClick();
                        });
                    } catch(Exception error) { failure=error;guide.DialogResult=DialogResult.Cancel; }
                }));
                var result=guide.ShowDialog();
                if(failure!=null)throw failure;
                Assert(result==DialogResult.OK,"图标向导未应用改动。");
            }
            var edited=Check(project,log,"实际向导点空白、添加图标并确认后预检",images);
            Assert(Geometry.Raw(edited.Window.Node.Named("office_flag_00"))==oldShield&&
                Geometry.Raw(edited.Window.Node.Named("elector_score_00"))==oldScore,"向导移动了原有叠放控件。");
            var final=new Scene(edited,new PreviewSettings{Playing=true});
            var icon=final.ByName("pf_editor_icon");var tip=final.ByName("pf_editor_icon_pf_hint");
            Assert(icon!=null&&icon.Raw==placed&&tip!=null&&tip.Bounds==icon.Bounds&&tip.Tooltip.Contains("大汗行动"),
                "图标点击位置、悬停范围或本地化丢失。");
            Assert(edited.Window.Doc.At(edited.Window.Doc.Root.Child("guiTypes").Named("pf075_hre_panel"))==
                model.Window.Doc.At(model.Window.Doc.Root.Child("guiTypes").Named("pf075_hre_panel")),"改动影响了共用神罗面板。");
            var flag=final.ByName("office_flag_00");
            var move=Editing.Move(project,edited,flag,flag.Raw.X-2,flag.Raw.Y,false,true);
            var updated=move.Read(edited.GuiPath).Root.Child("guiTypes").Named(edited.Window.Node.Get("name"));
            Assert(Geometry.Raw(updated.Named("elector_score_00")).X==final.ByName("elector_score_00").Raw.X-2&&
                Geometry.Raw(updated.Named("elector_support_00")).X==final.ByName("elector_support_00").Raw.X-2,"盾徽移动没有带动评分和小盾徽。");
            var validMove=new Model(new Mount(project.Data.Roots,move.After,CancellationToken.None),project.Data.Type,
                project.Data.Instance,project.Data.Country,project.Data.Date,project.Data.Language,CancellationToken.None,images);
            Assert(!NativeValidation.Run(validMove,Program.Temporary).Any(e=>e.Error),"同步移动后的选举区原生校验失败。");
            bool rejected=false;
            try { Editing.Move(project,edited,flag,-18,319,false); } catch(InvalidOperationException e) { rejected=e.Message.Contains("绿线"); }
            Assert(rejected,"面板外的位置仍允许提交。");
            project.Undo(false);
            Assert(Files.Same(project.Content[model.GuiPath].Data,source),"一次撤销未还原新增图标。");
            log.WriteLine("图标提示命中，旧叠放坐标保留。整组移动、越过外边界的保护和撤销通过。");
        }
        private static void ClickWizardDialog(ComponentDialog guide,string buttonName,Action<Form> handle) {
            var button=AllControls(guide).OfType<Button>().Single(b=>b.Text==buttonName);
            Exception error=null;
            bool handled=false;
            var deadline=DateTime.UtcNow.AddSeconds(20);
            using(var timer=new System.Windows.Forms.Timer{Interval=60}) {
                timer.Tick+=(s,e)=>{
                    var child=Application.OpenForms.Cast<Form>().LastOrDefault(f=>f!=guide&&f.Visible);
                    if(child==null)return;
                    timer.Stop();handled=true;
                    try { handle(child); }
                    catch(Exception ex) { error=ex;child.Close(); }
                };
                timer.Start();button.PerformClick();
                while(!handled&&DateTime.UtcNow<deadline){Application.DoEvents();Thread.Sleep(15);}
                Application.DoEvents();
            }
            if(error!=null)throw error;
            Assert(handled,"向导未打开所需窗口 "+buttonName);
        }
        private static IEnumerable<Control> AllControls(Control parent) {
            foreach(Control child in parent.Controls) {
                yield return child;
                foreach(var control in AllControls(child))yield return control;
            }
        }
        private static Model BuildModel(Project p,Sprites images) {
            return new Model(new Mount(p.Data.Roots,p.Content,CancellationToken.None),p.Data.Type,p.Data.Instance,p.Data.Country,p.Data.Date,p.Data.Language,CancellationToken.None,images);
        }
        private static Model Check(Project p,StreamWriter log,string label,Sprites images) {
            var model=BuildModel(p,images);
            var problems=model.Problems.Concat(NativeValidation.Run(model,Program.Temporary)).ToArray();
            log.WriteLine(label+"  "+NativeValidation.LastSummary.Replace('\t',' '));
            foreach(var e in problems.Take(30))log.WriteLine("  "+e.File+" "+e);
            Assert(!problems.Any(e=>e.Error),label+"未通过");
            Assert(model.Type!=null&&model.Window!=null,label+"缺少预览");
            return model;
        }
        private static void Assert(bool good,string label) { if(!good)throw new InvalidOperationException(label); }
        private static byte[] Dds() {
            var data=new byte[128+24*24*4];
            Action<int,uint> put=(i,v)=>Array.Copy(BitConverter.GetBytes(v),0,data,i,4);
            put(0,0x20534444);put(4,124);put(8,0x100f);put(12,24);put(16,24);put(20,96);put(76,32);put(80,65);put(88,32);
            put(92,0xff0000);put(96,0xff00);put(100,0xff);put(104,0xff000000);put(108,0x1000);
            for(int y=0;y<24;++y)for(int x=0;x<24;++x) {
                int p=128+(y*24+x)*4;
                data[p]=(byte)(98+x*5);data[p+1]=(byte)(110+y*4);data[p+2]=35;
                int dx=x-12,dy=y-12;data[p+3]=(byte)(dx*dx+dy*dy<133?255:0);
            }
            return data;
        }
        private static void Capture(Form form,string target) {
            using(var bitmap=new Bitmap(form.Width,form.Height)) {
                using(var graphics=Graphics.FromImage(bitmap)) {
                    IntPtr dc=graphics.GetHdc();
                    bool result;
                    try{result=PrintWindow(form.Handle,dc,2);}finally{graphics.ReleaseHdc(dc);}
                    if(!result||bitmap.GetPixel(bitmap.Width/2,90).GetBrightness()<.02f)form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size));
                }
                bitmap.Save(target);
            }
        }
        [DllImport("user32.dll")]
        private static extern bool PrintWindow(IntPtr window,IntPtr target,uint flags);
    }
}
