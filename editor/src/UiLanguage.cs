using System;
using System.Collections.Generic;
using System.IO;
using System.Security;

namespace PoliticalEditor {
    internal static class L {
        internal const string Chinese="zh-CN", English="en";
        private static readonly Lazy<Dictionary<string,string>> EnglishText=new Lazy<Dictionary<string,string>>(()=>{
            var bytes=Files.Resource("ui.english");
            return bytes==null?new Dictionary<string,string>():Files.Json().Deserialize<Dictionary<string,string>>(Files.Utf8.GetString(bytes));
        });
        private static volatile string current=Chinese;
        internal static string Current { get { return current; } }
        internal static string T(string text) {
            string value;
            return current==English&&text!=null&&EnglishText.Value.TryGetValue(text,out value)?value:text;
        }
        internal static bool HasTranslation(string text) { return EnglishText.Value.ContainsKey(text); }
        internal static void Use(string language) { current=language==English?English:Chinese; }
        internal static string Read(string file) {
            try {
                if(File.Exists(file)&&Files.Utf8.GetString(Files.Read(file,64)).Trim()==English)return English;
            } catch(IOException) { } catch(UnauthorizedAccessException) { } catch(SecurityException) { } catch(System.Text.DecoderFallbackException) { }
            return Chinese;
        }
        internal static void Save(string file,string language) { Files.Atomic(file,Files.Utf8.GetBytes(language==English?English:Chinese)); }
    }
}
