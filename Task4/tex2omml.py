# -*- coding: utf-8 -*-
"""
极简 LaTeX 子集 -> OMML (Office Math Markup Language) 转换器
纯离线实现，不依赖任何外部工具，供生成含"原生 Word 公式对象"的 .docx 使用。
"""
import re
from xml.sax.saxutils import escape

M_NS = "http://schemas.openxmlformats.org/officeDocument/2006/math"

GREEK = {
    "alpha": "α", "beta": "β", "gamma": "γ", "delta": "δ", "epsilon": "ε",
    "varepsilon": "ε", "zeta": "ζ", "eta": "η", "theta": "θ", "vartheta": "ϑ",
    "iota": "ι", "kappa": "κ", "lambda": "λ", "mu": "μ", "nu": "ν", "xi": "ξ",
    "pi": "π", "rho": "ρ", "sigma": "σ", "tau": "τ", "upsilon": "υ", "phi": "φ",
    "varphi": "φ", "chi": "χ", "psi": "ψ", "omega": "ω",
    "Gamma": "Γ", "Delta": "Δ", "Theta": "Θ", "Lambda": "Λ", "Xi": "Ξ",
    "Pi": "Π", "Sigma": "Σ", "Upsilon": "Υ", "Phi": "Φ", "Psi": "Ψ", "Omega": "Ω",
}

SYM = {
    "times": "×", "cdot": "⋅", "pm": "±", "mp": "∓", "le": "≤", "leq": "≤",
    "ge": "≥", "geq": "≥", "ne": "≠", "neq": "≠", "approx": "≈", "equiv": "≡",
    "to": "→", "rightarrow": "→", "Rightarrow": "⇒", "Longrightarrow": "⟹",
    "Leftrightarrow": "⇔", "in": "∈", "notin": "∉", "cup": "∪", "cap": "∩",
    "infty": "∞", "circ": "∘", "angle": "∠", "triangle": "△", "partial": "∂",
    "nabla": "∇", "sum": "∑", "prod": "∏", "int": "∫", "ldots": "…",
    "cdots": "⋯", "dots": "…", "perp": "⊥", "parallel": "∥", "because": "∵",
    "therefore": "∴", "prime": "′", "vert": "|", "Vert": "‖", "degree": "°",
    "quad": "\u2003", "qquad": "\u2003\u2003", "colon": ":",
    "sim": "∼", "simeq": "≃", "cong": "≅", "propto": "∝", "ast": "∗",
    "star": "⋆", "bullet": "∙", "oplus": "⊕", "otimes": "⊗", "over": "/",
    "not": "̸", "forall": "∀", "exists": "∃", "leftarrow": "←",
    "Leftarrow": "⇐", "uparrow": "↑", "downarrow": "↓", "mapsto": "↦",
}

UPRIGHT = {"cos", "sin", "tan", "cot", "sec", "csc", "arccos", "arcsin",
           "arctan", "arccot", "sinh", "cosh", "tanh", "exp", "ln", "log",
           "lg", "max", "min", "lim", "arg", "det", "dim", "ker", "deg",
           "gcd", "lcm", "sup", "inf", "sgn", "mod", "bmod", "e", "d"}

# 这些是 LaTeX 的"间距微调"命令。键必须是【去掉反斜杠后】的命令名 ——
# 扫描器 peek_cmd()/consume_cmd() 返回的就是命令名本身（如 "\!" -> "!"），
# 所以键写成 "\\!" 永远匹配不上，会被当成字面字符原样输出。
SPACE_CMDS = {",": "\u2009", ";": "\u2005", ":": "\u2005", "!": "",
              " ": " ", "quad": "\u2003", "qquad": "\u2003\u2003"}

MATRIX_ENVS = {"bmatrix", "pmatrix", "matrix", "Bmatrix", "vmatrix", "Vmatrix"}

# 各类矩阵环境对应的外围括号
MATRIX_DELIM = {
    "bmatrix": ("[", "]"), "pmatrix": ("(", ")"), "Bmatrix": ("{", "}"),
    "vmatrix": ("|", "|"), "Vmatrix": ("‖", "‖"), "matrix": (".", "."),
}


class Node:
    __slots__ = ("xml", "kind", "base", "sub", "sup")

    def __init__(self, xml, kind="el", base=None, sub=None, sup=None):
        self.xml, self.kind, self.base, self.sub, self.sup = xml, kind, base, sub, sup


def _t(txt, sty=None):
    rpr = '<m:rPr><m:sty m:val="%s"/></m:rPr>' % sty if sty else ""
    return '<m:r>%s<m:t xml:space="preserve">%s</m:t></m:r>' % (rpr, escape(txt))


def N(txt, sty=None):
    return Node(_t(txt, sty))


def join(nodes):
    return "".join(n.xml for n in nodes)


def n_frac(num, den):
    return Node('<m:f><m:fPr><m:type m:val="bar"/></m:fPr>'
                '<m:num>%s</m:num><m:den>%s</m:den></m:f>' % (join(num), join(den)))


def n_rad(e):
    return Node('<m:rad><m:radPr><m:degHide m:val="1"/></m:radPr>'
                '<m:deg/><m:e>%s</m:e></m:rad>' % join(e))


def n_sup(base, sup):
    return Node('<m:sSup><m:sSupPr/><m:e>%s</m:e><m:sup>%s</m:sup></m:sSup>'
                % (join(base), join(sup)), "sup", base, None, sup)


def n_sub(base, sub):
    return Node('<m:sSub><m:sSubPr/><m:e>%s</m:e><m:sub>%s</m:sub></m:sSub>'
                % (join(base), join(sub)), "sub", base, sub, None)


def n_subsup(base, sub, sup):
    return Node('<m:sSubSup><m:sSubSupPr/><m:e>%s</m:e><m:sub>%s</m:sub>'
                '<m:sup>%s</m:sup></m:sSubSup>'
                % (join(base), join(sub), join(sup)), "subsup", base, sub, sup)


def n_delim(inner, beg="(", end=")"):
    b = "" if beg == "." else beg
    e = "" if end == "." else end
    return Node('<m:d><m:dPr><m:begChr m:val="%s"/><m:endChr m:val="%s"/>'
                '<m:grow m:val="1"/></m:dPr><m:e>%s</m:e></m:d>'
                % (escape(b), escape(e), join(inner)))


def n_matrix(rows):
    ncols = max((len(r) for r in rows), default=1)
    mc = ('<m:mc><m:mcPr><m:count m:val="%d"/>'
          '<m:mcJc m:val="center"/></m:mcPr></m:mc>' % ncols)
    body = "".join("<m:mr>" + "".join('<m:e>%s</m:e>' % join(c) for c in row)
                   + "</m:mr>" for row in rows)
    return Node('<m:m><m:mPr><m:mcs>%s</m:mcs></m:mPr>%s</m:m>' % (mc, body))


class Tex:
    def __init__(self, s):
        self.s = s
        self.i = 0
        self.n = len(s)

    def peek(self):
        return self.s[self.i] if self.i < self.n else ""

    def eof(self):
        return self.i >= self.n

    # ---------- 命令扫描 ----------
    def peek_cmd(self):
        if self.peek() != "\\":
            return None
        j = self.i + 1
        if j >= self.n:
            return ""
        if self.s[j] == "\\":
            return "\\\\"
        k = j
        while k < self.n and self.s[k].isalpha():
            k += 1
        if k == j:
            return self.s[j]
        return self.s[j:k]

    def consume_cmd(self):
        if self.peek() != "\\":
            return ""
        j = self.i + 1
        if j >= self.n:
            self.i = self.n
            return ""
        if self.s[j] == "\\":
            self.i = j + 1
            return "\\\\"
        k = j
        while k < self.n and self.s[k].isalpha():
            k += 1
        if k == j:
            self.i = j + 1
            return self.s[j]
        self.i = k
        return self.s[j:k]

    def read_raw_group(self):
        """读取 {..} 内的原始文本（用于 \\mathrm{...}），支持嵌套花括号。"""
        if self.peek() != "{":
            return ""
        self.i += 1
        depth = 1
        out = []
        while not self.eof():
            c = self.s[self.i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    self.i += 1
                    break
            out.append(c)
            self.i += 1
        return "".join(out)

    def read_delim(self):
        c = self.peek()
        if c == "\\":
            cmd = self.peek_cmd()
            self.consume_cmd()
            return {"{": "{", "}": "}", "vert": "|", "Vert": "‖",
                    "langle": "⟨", "rangle": "⟩"}.get(cmd, "")
        if c == "":
            return ""
        self.i += 1
        return c

    def parse_arg(self):
        c = self.peek()
        if c == "{":
            self.i += 1
            grp = self.parse_seq("}")
            if self.peek() == "}":
                self.i += 1
            return grp
        if c == "\\":
            cmd = self.peek_cmd()
            self.consume_cmd()
            return self.cmd_node(cmd)
        if c == "":
            return []
        self.i += 1
        return [N(c)]

    def cmd_node(self, cmd):
        if cmd in GREEK:
            return [N(GREEK[cmd])]
        if cmd in SYM:
            return [N(SYM[cmd])]
        if cmd in UPRIGHT:
            return [N(cmd, "p")]
        if cmd in SPACE_CMDS:
            return [N(SPACE_CMDS[cmd])] if SPACE_CMDS[cmd] else []
        if cmd == "" :
            return []
        return [N(cmd)]

    def attach(self, nodes, kind, arg):
        base = nodes.pop() if nodes else N("")
        if kind == "^":
            if base.kind == "sub":
                nodes.append(n_subsup(base.base, base.sub, arg))
            else:
                nodes.append(n_sup([base], arg))
        else:
            if base.kind == "sup":
                nodes.append(n_subsup(base.base, arg, base.sup))
            else:
                nodes.append(n_sub([base], arg))

    def parse_matrix(self):
        """解析矩阵环境内容，返回行的列表；每行是单元格（节点列表）的列表。

        关键：单元格必须在遇到行分隔符 `\\\\` 时【停下来】。
        之前 stop_cmds 里只有 "end"，`\\\\` 被 parse_seq 当成普通命令消费掉，
        结果整块矩阵退化成一行带字面 "\\\\" 的纯文本 —— 这是第一版的渲染事故。
        """
        rows, cells = [], []
        while True:
            cell = self.parse_seq(stop_chars="&", stop_cmds=("end", "\\\\"))
            cells.append(cell)
            if self.peek() == "&":
                self.i += 1
                continue
            cmd = self.peek_cmd()
            if cmd == "\\\\":
                self.consume_cmd()
                if self.peek() == "[":
                    while not self.eof() and self.peek() != "]":
                        self.i += 1
                    if self.peek() == "]":
                        self.i += 1
                rows.append(cells)
                cells = []
                continue
            if cmd == "end":
                self.consume_cmd()
                self.read_raw_group()
                rows.append(cells)
                break
            rows.append(cells)
            break
        return [r for r in rows if any(c for c in r)]

    def apply_command(self, cmd, nodes):
        if cmd in ("frac", "dfrac", "tfrac"):
            a = self.parse_arg()
            b = self.parse_arg()
            nodes.append(n_frac(a, b))
            return
        if cmd == "sqrt":
            if self.peek() == "[":
                while not self.eof() and self.peek() != "]":
                    self.i += 1
                if self.peek() == "]":
                    self.i += 1
            nodes.append(n_rad(self.parse_arg()))
            return
        if cmd in ("mathrm", "text", "operatorname", "mathit", "mathbf",
                   "mathsf", "mathtt"):
            raw = self.read_raw_group()
            if cmd == "mathbf":
                nodes.append(N(raw, "bi"))
            elif cmd == "mathit":
                nodes.append(N(raw, "i"))
            else:
                nodes.append(N(raw, "p"))
            return
        if cmd == "boldsymbol":
            nodes.append(N(self.read_raw_group(), "bi"))
            return
        if cmd == "boxed":
            inner = self.parse_arg()
            nodes.append(Node(
                '<m:borderBox><m:borderBoxPr><m:ctrlPr/></m:borderBoxPr>'
                '<m:e>%s</m:e></m:borderBox>' % join(inner)))
            return
        if cmd == "left":
            beg = self.read_delim()
            inner = self.parse_seq(stop_cmds=("right",))
            if self.peek_cmd() == "right":
                self.consume_cmd()
                end = self.read_delim()
            else:
                end = "."
            nodes.append(n_delim(inner, beg, end))
            return
        if cmd == "begin":
            env = self.read_raw_group()
            if env in MATRIX_ENVS:
                rows = self.parse_matrix()
                beg, end = MATRIX_DELIM.get(env, (".", "."))
                mat = n_matrix(rows)
                # 带括号的矩阵环境：OMML 里矩阵本身不含括号，需再包一层 m:d
                nodes.append(n_delim([mat], beg, end) if beg != "." else mat)
            return
        if cmd in SPACE_CMDS:
            txt = SPACE_CMDS[cmd]
            if txt:
                nodes.append(N(txt))
            return
        nodes.extend(self.cmd_node(cmd))

    def parse_seq(self, stop_chars="", stop_cmds=()):
        nodes = []
        while not self.eof():
            c = self.peek()
            if c in stop_chars:
                break
            if c == "\\":
                cmd = self.peek_cmd()
                if cmd in stop_cmds:
                    break
                self.consume_cmd()
                self.apply_command(cmd, nodes)
                continue
            if c == "{":
                self.i += 1
                grp = self.parse_seq("}")
                if self.peek() == "}":
                    self.i += 1
                nodes.append(Node(join(grp), "grp", base=grp))
                continue
            if c == "}":
                break
            if c in "^_":
                self.i += 1
                self.attach(nodes, c, self.parse_arg())
                continue
            if c == "'":
                self.i += 1
                nodes.append(N("′"))
                continue
            if c.isspace():
                self.i += 1
                continue
            # 连续普通字符累积成一个 run
            j = self.i
            chunk = []
            while (j < self.n and self.s[j] not in "\\{}^_'"
                   and not self.s[j].isspace() and self.s[j] not in stop_chars):
                chunk.append(self.s[j])
                j += 1
            self.i = j
            nodes.append(N("".join(chunk)))
        return nodes


def tex_to_omml(tex):
    """把一段 LaTeX 子集转换成 <m:oMath> XML 字符串。"""
    p = Tex(tex)
    inner = join(p.parse_seq())
    return '<m:oMath xmlns:m="%s">%s</m:oMath>' % (M_NS, inner)
