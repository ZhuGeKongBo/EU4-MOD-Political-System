using System;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;

namespace PoliticalEditor {
    internal static partial class Smoke {
        private static void CheckCustomOfficeLayout(RootMap roots,StreamWriter log,string directory) {
            var clean=roots.Copy();
            clean.Mod="";
            clean.Dependencies=new string[0];
            var project=new Project(clean);
            project.Data.Type="harbor_moot";
            project.Data.Instance="western_harbor";
            project.Data.Country="FRA";
            project.Data.Date="1444.11.11";
            project.Data.Title="Harbor Moot";
            Project.PutText(project.Content,"descriptor.mod","name = \"Harbor Moot\"\nsupported_version = \"1.37.*\"\n",null);
            Project.PutText(project.Content,Model.TypeDir+"harbor_moot.txt",@"
harbor_moot = {
    title = harbor_title
    leader = { title = Captain succession = {
        mode = elected event = ruler_changed electors_role = delegate
        tie_break = incumbent_then_stable_id cooldown_on_turnover = retain
    } }
    members = { source = explicit title = Ports }
    special_members = { delegate = { acquisition = appointed title_key = harbor_seats tooltip = harbor_tip max_holders = 3 require_member = yes } }
    interface = {
        layout = harbor_chart
        entry_button = { region = port_buttons icon = GFX_harbor_entry tooltip = harbor_tip }
    }
}",null);
            Project.PutText(project.Content,Model.HistoryDir+"western_harbor.txt",@"
western_harbor = {
    type = harbor_moot
    1444.1.1 = { leader = FRA members = { FRA SAV POR CAS } roles = { delegate = { SAV = yes POR = yes CAS = yes } } }
}",null);
            Project.PutText(project.Content,Model.LayoutDir+"harbor_chart.txt",@"
harbor_chart = {
    gui_file = ""interface/harbor_chart.gui"" window = western_chart
    background_control = chart_art hover_control = chart_hover close_control = leave_chart vote_control = council_vote
    regions = {
        leader_region = { kind = slot control = captain }
        members_region = { kind = list rows = 1 control = port caption = ports_caption scrollbar = port_rail }
        special_members_region = {
            kind = grid rows = 1 columns = 2 auto_place = no
            control = chair caption = seats_caption scrollbar = seat_rail
            group_heading = delegates_title group_tooltip_control = delegates_tip
            nomination_score = rating nomination_tooltip_control = rating_read support_shield = pledge
        }
    }
}
hud_regions = { port_buttons = { gui_file = ""interface/harbor_chart.gui"" gfx_file = ""interface/harbor_chart.gfx"" container = port_slot button_template = chart_icon capacity = 1 page_window = port_pages } }
",null);
            Project.PutText(project.Content,"interface/harbor_chart.gfx",@"
spriteTypes = {
    spriteType = { name = ""GFX_harbor_entry"" texturefile = ""gfx/interface/icon_hre.dds"" }
    spriteType = { name = ""GFX_harbor_hit"" texturefile = ""gfx/interface/resource_transparent.dds"" transparencecheck = no }
}",null);
            Project.PutText(project.Content,"localisation/harbor_chart_l_english.yml","l_english:\n harbor_title:0 \"Harbor Moot\"\n harbor_seats:0 \"Delegates\"\n harbor_tip:0 \"Review the delegates\"\n",null);
            Project.PutText(project.Content,"interface/harbor_chart.gui",@"
guiTypes = {
    windowType = {
        name = western_chart orientation = CENTER position = { x = -259 y = -219 } size = { x = 518 y = 438 }
        iconType = { name = chart_art spriteType = GFX_hre_bg position = { x = 0 y = 0 } }
        guiButtonType = { name = chart_hover position = { x = 0 y = 0 } size = { x = 518 y = 438 } quadTextureSprite = GFX_harbor_hit no_clicksound = yes }
        guiButtonType = { name = leave_chart position = { x = 476 y = 30 } }
        guiButtonType = { name = council_vote position = { x = 385 y = 30 } }
        guiButtonType = { name = captain position = { x = 26 y = 34 } size = { x = 56 y = 56 } quadTextureSprite = GFX_shield_medium }
        instantTextBoxType = { name = ports_caption position = { x = 391 y = 180 } maxWidth = 70 maxHeight = 20 }
        guiButtonType = { name = port_00 position = { x = 401 y = 221 } size = { x = 32 y = 32 } quadTextureSprite = GFX_shield_small }
        scrollbarType = { name = port_rail position = { x = 447 y = 218 } size = { x = 16 y = 70 } horizontal = 0 }
        instantTextBoxType = { name = seats_caption position = { x = 51 y = 115 } maxWidth = 90 maxHeight = 18 }
        scrollbarType = { name = seat_rail position = { x = 339 y = 140 } size = { x = 16 y = 160 } horizontal = 0 }
        guiButtonType = { name = delegates_tip_00 position = { x = 51 y = 140 } size = { x = 108 y = 14 } quadTextureSprite = GFX_harbor_hit no_clicksound = yes }
        instantTextBoxType = { name = delegates_title_00 position = { x = 51 y = 140 } maxWidth = 108 maxHeight = 14 alwaystransparent = yes }
        guiButtonType = { name = chair_00 position = { x = 68 y = 160 } size = { x = 56 y = 56 } quadTextureSprite = GFX_shield_medium }
        guiButtonType = { name = chair_01 position = { x = 254 y = 160 } size = { x = 56 y = 56 } quadTextureSprite = GFX_shield_medium }
        guiButtonType = { name = pledge_00 position = { x = 72 y = 202 } size = { x = 22 y = 22 } quadTextureSprite = GFX_shield_small }
        guiButtonType = { name = pledge_01 position = { x = 258 y = 202 } size = { x = 22 y = 22 } quadTextureSprite = GFX_shield_small }
        guiButtonType = { name = rating_read_00 position = { x = 132 y = 199 } size = { x = 36 y = 16 } quadTextureSprite = GFX_harbor_hit no_clicksound = yes }
        guiButtonType = { name = rating_read_01 position = { x = 319 y = 199 } size = { x = 36 y = 16 } quadTextureSprite = GFX_harbor_hit no_clicksound = yes }
        instantTextBoxType = { name = rating_00 position = { x = 132 y = 199 } maxWidth = 36 maxHeight = 16 alwaystransparent = yes format = left }
        instantTextBoxType = { name = rating_01 position = { x = 319 y = 199 } maxWidth = 36 maxHeight = 16 alwaystransparent = yes format = left }
    }
    windowType = { name = port_slot_00 guiButtonType = { name = chart_icon quadTextureSprite = GFX_harbor_entry } }
    windowType = { name = port_pages guiButtonType = { name = ps_page_next } }
}",null);
            project.Content["interface/ps_instance_selector.gui"]=new Entry(Files.Resource("starter.selector"),null,false);
            var model=Check(project,log,"T32 从空白建立独立类型、历史、布局和入口",null);
            Assert(model.Chain.Count==1&&model.EntrySprite=="GFX_harbor_entry","自定义工程没有使用自己的界面和入口。");
            var scene=new Scene(model,new PreviewSettings());
            var score=scene.ByName("rating_00");
            var target=new Point(99,201);
            Assert(Editing.ElectionPlacement(model,score).Areas.Any(area=>area.Contains(target)),"自定义评分不能覆盖盾徽。");
            Editing.Move(project,model,score,target.X,target.Y,false).Commit();
            var layered=BuildModel(project,model.Images);
            var play=new Scene(layered,new PreviewSettings{Playing=true});
            using(var canvas=new PreviewCanvas()) {
                canvas.Use(play,null);
                var textHit=canvas.HitAt(new Point(103,206),false);
                Assert(textHit==play.ByName("rating_read_00")&&!textHit.Click&&textHit.Tooltip.Length>0,
                    "叠放处的评分悬停未命中。实际控件 "+(textHit==null?"无":textHit.Name));
                Assert(canvas.HitAt(new Point(90,179),false)==play.ByName("chair_00")&&play.ByName("chair_00").Click,"大盾徽露出部分缺少外交点击。");
                Assert(canvas.HitAt(new Point(80,212),false)==play.ByName("pledge_00"),"小盾徽命中丢失。");
            }
            var edit=new Scene(layered,new PreviewSettings());
            Editing.Move(project,layered,edit.ByName("chair_00"),268,69,false).Commit();
            var moved=BuildModel(project,model.Images);
            Assert(Geometry.Raw(moved.Window.Node.Named("rating_00"))==target&&
                Geometry.Raw(moved.Window.Node.Named("pledge_00"))==new Point(72,202),"大盾徽单独移动拖走了其它控件。");
            var grouped=new Scene(moved,new PreviewSettings());
            Editing.Move(project,moved,grouped.ByName("chair_01"),404,307,false,true).Commit();
            var final=Check(project,log,"T32 叠放、跨排、跨竖条与整组移动后的原生预检",model.Images);
            Assert(Geometry.Raw(final.Window.Node.Named("rating_01"))==new Point(469,346)&&
                Geometry.Raw(final.Window.Node.Named("rating_read_01"))==new Point(469,346)&&
                Geometry.Raw(final.Window.Node.Named("pledge_01"))==new Point(408,349),"整组移动丢失配对组件。");
            var path=Path.Combine(directory,"Harbor-Moot.pfproject.json");
            project.Save(path);
            var reopened=Check(Project.Open(path),log,"T32 自定义工程存读",model.Images);
            Assert(Geometry.Raw(reopened.Window.Node.Named("chair_01"))==new Point(404,307),"项目重开重置了自定义坐标。");
            var exported=new ExportPlan(project,final,directory,"harbor_moot_qa",true,Program.Temporary);
            string output=exported.Write(Program.Temporary);
            var transferred=clean.Copy();
            transferred.Mod=Path.Combine(output,"mod","harbor_moot_qa");
            var exportedProject=Project.Import(transferred,CancellationToken.None);
            exportedProject.Data.Type=project.Data.Type;
            exportedProject.Data.Date=project.Data.Date;
            var imported=Check(exportedProject,log,"T32 完整导出并从独立目录重读",model.Images);
            Assert(Geometry.Raw(imported.Window.Node.Named("rating_read_01"))==new Point(469,346),"导出重置了悬停位置。");
        }
    }
}
