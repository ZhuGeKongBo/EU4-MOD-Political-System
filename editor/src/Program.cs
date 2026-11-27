using System;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

[assembly: AssemblyTitle("EU4 Political Editor")]
[assembly: AssemblyDescription("Offline Windows editor for EU4 Political Framework content")]
[assembly: AssemblyProduct("EU4 Political Editor")]
[assembly: AssemblyVersion("1.0.0.0")]
[assembly: AssemblyFileVersion("1.0.11.0")]

namespace PoliticalEditor {
    internal static class Program {
        internal const string Version = "1.0.11";
        private static string home=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"EU4PoliticalEditor");
        internal static string Temporary { get { return Path.Combine(home,"cache"); } }
        internal static string Recovery { get { return Path.Combine(home,"recovery"); } }
        internal static string LanguagePreference { get { return Path.Combine(home,"interface-language.txt"); } }
        internal static void Log(Exception error) {
            try {
                Directory.CreateDirectory(home);
                string log=Path.Combine(home,"editor-errors.log");
                if(new FileInfo(log).Exists&&new FileInfo(log).Length>1500000)File.Delete(log);
                File.AppendAllText(log,DateTime.UtcNow.ToString("o")+"\r\n"+error+"\r\n",Encoding.UTF8);
            }catch(IOException){}catch(UnauthorizedAccessException){}
        }
        [STAThread]
        private static int Main(string[] args) {
            bool verify=args.Length==2&&args[0]=="--self-test";
            if(verify) {
                string directory=Files.Normalize(args[1]);
                var roots=RootMap.Detect();
                if(Files.Under(directory,roots.Game)||Files.Under(directory,roots.User))return 2;
                try { Files.NoLinks(directory); } catch(IOException) { return 2; }
                if(File.Exists(Path.Combine(directory,"results.txt")))return 2;
                home=Path.Combine(directory,"editor-state");
            }
            try {
                Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
                Application.SetUnhandledExceptionMode(verify?UnhandledExceptionMode.ThrowException:UnhandledExceptionMode.CatchException);
                if(verify)return Smoke.Run(args[1]);
                L.Use(L.Read(LanguagePreference));
                Application.ThreadException+=(s,e)=>{Log(e.Exception);MessageBox.Show(e.Exception.Message,L.T("EU4 政治体系编辑器"),MessageBoxButtons.OK,MessageBoxIcon.Warning);};
                WorkbenchSession session=null;
                do {
                    var resume=session;
                    using(var window=new Workbench()) {
                        if(resume!=null) {
                            window.StartPosition=FormStartPosition.Manual;
                            window.Bounds=resume.Bounds;window.WindowState=resume.WindowState;
                            window.Shown+=async(s,e)=>await window.RestoreWindow(resume);
                        } else if(args.Length==1&&args[0].EndsWith(".pfproject.json",StringComparison.OrdinalIgnoreCase))
                            window.Shown+=async(s,e)=>await window.OpenPath(args[0]);
                        Application.Run(window);
                        session=window.NextWindow;
                    }
                    if(session!=null)L.Use(session.Language);
                } while(session!=null);
                return 0;
            } catch(Exception e) {
                Log(e);
                if(verify) {
                    try{File.AppendAllText(Path.Combine(args[1],"results.txt"),"\r\nFAIL "+e+"\r\n",Encoding.UTF8);}catch(IOException){}
                } else MessageBox.Show(e.Message,L.T("启动编辑器时遇到错误"),MessageBoxButtons.OK,MessageBoxIcon.Error);
                return 1;
            }
        }
    }
}
