using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;

namespace PoliticalEditor {
    internal sealed class Problem {
        internal readonly string File, Message;
        internal readonly int Offset;
        internal readonly bool Error;
        internal readonly string Binding;
        internal readonly string LayoutId;
        internal readonly string RawDetail;
        internal readonly string ControlId, PositionDetail, PositionScope;
        internal Problem(string file, int offset, string message, bool error = true, string binding = null, string layoutId = null, string rawDetail = null,
            string controlId = null, string positionDetail = null, string positionScope = null) {
            File = file; Offset = offset; Message = message; Error = error; Binding=binding; LayoutId=layoutId; RawDetail=rawDetail;
            ControlId=controlId;PositionDetail=positionDetail;PositionScope=positionScope;
        }
        public override string ToString() { return (Error ? L.T("错误  ") : L.T("提示  ")) + Message; }
    }

    internal sealed class Node {
        internal string Key, Value, Operator;
        internal int Start, KeyEnd, ValueStart, End, Close;
        internal List<Node> Children;
        internal bool Block { get { return Children != null; } }
        internal Node Child(string key) { return Children == null ? null : Children.LastOrDefault(n => String.Equals(n.Key, key, StringComparison.OrdinalIgnoreCase)); }
        internal string Get(string key, string fallback = "") { var n = Child(key); return n == null || n.Value == null ? fallback : n.Value; }
        internal Node Named(string name) { return Children == null ? null : Children.FirstOrDefault(n => n.Block && n.Get("name") == name); }
        internal IEnumerable<Node> Nodes { get { return Children ?? Enumerable.Empty<Node>(); } }
    }

    internal sealed class Document {
        private struct Token {
            internal readonly int Start, End;
            internal readonly string Text;
            internal readonly char Kind;
            internal Token(int start, int end, string text, char kind) { Start = start; End = end; Text = text; Kind = kind; }
        }
        internal readonly string File, Text;
        internal readonly List<Problem> Problems = new List<Problem>();
        internal readonly Node Root;
        private readonly List<Token> tokens = new List<Token>();
        private int next;

        internal Document(string file, string text) {
            File = file; Text = text;
            Tokenize();
            Root = new Node { Key = "", Children = ReadBlock(false, 0), Close = text.Length, End = text.Length };
        }

        private void Error(int offset, string message) {
            if (Problems.Count < 40) Problems.Add(new Problem(File, offset, message));
        }

        private void Tokenize() {
            for (int i = 0; i < Text.Length;) {
                int start = i;
                char ch = Text[i++];
                if (char.IsWhiteSpace(ch) || ch == '\uFEFF') continue;
                if (ch == '#') { while (i < Text.Length && Text[i] != '\r' && Text[i] != '\n') ++i; continue; }
                if (ch == '{' || ch == '}') { tokens.Add(new Token(start, i, ch.ToString(), ch)); continue; }
                if (ch == '=' || ch == '<' || ch == '>' || ((ch == '!' || ch == '?') && i < Text.Length && Text[i] == '=')) {
                    if (ch != '=' && i < Text.Length && Text[i] == '=') ++i;
                    tokens.Add(new Token(start, i, Text.Substring(start, i - start), '='));
                    continue;
                }
                if (ch == '"') {
                    var value = new StringBuilder(); bool closed = false;
                    while (i < Text.Length) {
                        ch = Text[i++];
                        if (ch == '"') { closed = true; break; }
                        if (ch == '\\' && i < Text.Length && (Text[i] == '"' || Text[i] == '\\')) ch = Text[i++];
                        value.Append(ch);
                    }
                    if (!closed) Error(start, L.T("字符串缺少结束引号。"));
                    tokens.Add(new Token(start, i, value.ToString(), 'w'));
                    continue;
                }
                while (i < Text.Length && !char.IsWhiteSpace(Text[i]) && "#{}=<>\"".IndexOf(Text[i]) < 0) ++i;
                tokens.Add(new Token(start, i, Text.Substring(start, i - start), 'w'));
            }
        }

        private List<Node> ReadBlock(bool nested, int depth) {
            var nodes = new List<Node>();
            if (depth > 128) { Error(tokens[Math.Min(next, tokens.Count - 1)].Start, L.T("嵌套超过可编辑深度。")); next = tokens.Count; return nodes; }
            while (next < tokens.Count) {
                Token token = tokens[next++];
                if (token.Kind == '}') {
                    if (nested) { --next; return nodes; }
                    Error(token.Start, L.T("此右大括号没有匹配的左大括号。")); continue;
                }
                if (token.Kind != 'w' && token.Kind != '{') { Error(token.Start, L.T("此处应为字段或值。")); continue; }
                var node = new Node { Key = token.Text, Value = token.Text, Start = token.Start, KeyEnd = token.End, ValueStart = token.Start, End = token.End };
                if (token.Kind == '{') { node.Key = ""; --next; }
                else if (next < tokens.Count && tokens[next].Kind == '=') {
                    node.Operator = tokens[next++].Text;
                    if (next >= tokens.Count || (tokens[next].Kind != 'w' && tokens[next].Kind != '{')) {
                        Error(node.Start, L.T("字段 ") + node.Key + L.T(" 缺少值。")); continue;
                    }
                } else { nodes.Add(node); continue; }
                Token valueToken = tokens[next++];
                node.ValueStart = valueToken.Start;
                if (valueToken.Kind == '{') {
                    node.Value = null;
                    node.Children = ReadBlock(true, depth + 1);
                    if (next < tokens.Count && tokens[next].Kind == '}') {
                        node.Close = tokens[next].Start;
                        node.End = tokens[next++].End;
                    } else {
                        node.Close = node.End = Text.Length;
                        Error(valueToken.Start, L.T("字段 ") + node.Key + L.T(" 缺少右大括号。"));
                    }
                } else { node.Value = valueToken.Text; node.End = valueToken.End; }
                nodes.Add(node);
            }
            return nodes;
        }

        internal string At(Node n) { return Text.Substring(n.Start, n.End - n.Start); }
        internal int Line(int offset) { int line = 1; for (int i = 0; i < Math.Min(offset, Text.Length); ++i) if (Text[i] == '\n') ++line; return line; }
        internal string Newline { get { return Text.Contains("\r\n") ? "\r\n" : "\n"; } }
        internal string Indent(int position) {
            int start = position;
            while (start > 0 && Text[start - 1] != '\n' && Text[start - 1] != '\r') --start;
            int stop = start;
            while (stop < position && (Text[stop] == ' ' || Text[stop] == '\t')) ++stop;
            return Text.Substring(start, stop - start);
        }
        internal string Replace(int start, int end, string value) { return Text.Substring(0, start) + value + Text.Substring(end); }
        internal string Add(Node parent, string block) {
            string nl = Newline;
            block = ViewText(block);
            if (parent == Root) return Text + (Text.Length == 0 || Text.EndsWith("\n") ? "" : nl) + block.Replace("\n", nl) + nl;
            string indent = parent.Nodes.Any() ? Indent(parent.Nodes.First().Start) : Indent(parent.Start) + "    ";
            if (indent.Length <= Indent(parent.Start).Length) indent = Indent(parent.Start) + "    ";
            int insert = parent.Close;
            while (insert > parent.ValueStart + 1 && (Text[insert - 1] == ' ' || Text[insert - 1] == '\t')) --insert;
            string addition = (insert == 0 || Text[insert - 1] == '\n' ? "" : nl) + indent + block.Replace("\n", nl + indent) + nl;
            return Replace(insert, insert, addition);
        }
        internal string Set(Node parent, string key, string text) {
            Node current = parent.Child(key);
            return current == null ? Add(parent, key + " = " + text) : Replace(current.ValueStart, current.End, text);
        }
        internal string SetPath(Node parent, string[] keys, int index, string value) {
            if (index == keys.Length - 1) return Set(parent, keys[index], value);
            Node child = parent.Child(keys[index]);
            if (child != null && child.Block) return SetPath(child, keys, index + 1, value);
            for (int j = keys.Length - 1; j > index; --j) value = "{ " + keys[j] + " = " + value + " }";
            return Set(parent, keys[index], value);
        }
        internal string Position(Node control, int x, int y) {
            var pos = control.Child("position");
            var px = pos == null ? null : pos.Child("x"); var py = pos == null ? null : pos.Child("y");
            if (px == null || py == null || px.Block || py.Block)
                return Set(control, "position", "{ x = " + x + " y = " + y + " }");
            var result = new StringBuilder(Text);
            var first = px.ValueStart > py.ValueStart ? px : py;
            var second = first == px ? py : px;
            result.Remove(first.ValueStart, first.End - first.ValueStart).Insert(first.ValueStart, (first == px ? x : y).ToString(CultureInfo.InvariantCulture));
            result.Remove(second.ValueStart, second.End - second.ValueStart).Insert(second.ValueStart, (second == px ? x : y).ToString(CultureInfo.InvariantCulture));
            return result.ToString();
        }

        internal static string Quote(string value) { return "\"" + value.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\r", " ").Replace("\n", " ") + "\""; }
        internal static int Int(string value, int fallback = 0) { int number; return int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out number) ? number : fallback; }
        internal static string ViewText(string raw) { return raw.Replace("\r\n", "\n").Replace('\r', '\n'); }
        internal static int RawOffset(string raw, int visual) {
            int offset = 0;
            for (int i = 0; i < visual && offset < raw.Length; ++i) {
                if (raw[offset++] == '\r' && offset < raw.Length && raw[offset] == '\n') ++offset;
            }
            return offset;
        }
        internal static string MergeEditorText(string raw, string edit) {
            string old = ViewText(raw), next = ViewText(edit);
            if (old == next) return raw;
            int prefix = 0, suffix = 0;
            while (prefix < Math.Min(old.Length, next.Length) && old[prefix] == next[prefix]) ++prefix;
            while (suffix < Math.Min(old.Length, next.Length) - prefix && old[old.Length - 1 - suffix] == next[next.Length - 1 - suffix]) ++suffix;
            int start = RawOffset(raw, prefix), end = RawOffset(raw, old.Length - suffix);
            string newline = raw.Contains("\r\n") ? "\r\n" : "\n";
            return raw.Substring(0, start) + next.Substring(prefix, next.Length - prefix - suffix).Replace("\n", newline) + raw.Substring(end);
        }
    }

    internal sealed class TextFile {
        internal readonly string Text;
        internal readonly Encoding Encoding;
        internal readonly int Preamble;
        private TextFile(string text, Encoding encoding, int preamble) { Text = text; Encoding = encoding; Preamble = preamble; }
        internal static TextFile Decode(byte[] bytes) {
            Encoding enc = new UTF8Encoding(false, true); int offset = 0;
            if (bytes.Length >= 3 && bytes[0] == 239 && bytes[1] == 187 && bytes[2] == 191) { enc = new UTF8Encoding(true, true); offset = 3; }
            else if (bytes.Length >= 2 && bytes[0] == 255 && bytes[1] == 254) { enc = new UnicodeEncoding(false, true, true); offset = 2; }
            else if (bytes.Length >= 2 && bytes[0] == 254 && bytes[1] == 255) { enc = new UnicodeEncoding(true, true, true); offset = 2; }
            try { return new TextFile(enc.GetString(bytes, offset, bytes.Length - offset), enc, offset); }
            catch (DecoderFallbackException) {
                if (offset != 0) throw;
                enc = Encoding.GetEncoding(1252, EncoderFallback.ExceptionFallback, DecoderFallback.ExceptionFallback);
                return new TextFile(enc.GetString(bytes), enc, 0);
            }
        }
        internal byte[] Encode(string text) {
            try { return WithPreamble(Encoding, text, Preamble != 0); }
            catch (EncoderFallbackException) { throw new InvalidOperationException(L.T("当前编码无法表达新增文字。请先将该文件明确转换为 UTF-8。")); }
        }
        private static byte[] WithPreamble(Encoding encoding, string text, bool include) {
            byte[] body = encoding.GetBytes(text);
            return include ? encoding.GetPreamble().Concat(body).ToArray() : body;
        }
        internal static byte[] Utf8(string text, bool bom = false) { return WithPreamble(new UTF8Encoding(bom, true), text, bom); }
    }
}
