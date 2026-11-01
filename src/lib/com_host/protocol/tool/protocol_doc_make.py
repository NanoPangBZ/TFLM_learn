#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
协议说明文档生成工具。

用法:
    python protocol_doc_make.py <protocol目录路径> <文档输出路径>

扫描 protocol/cmd_data_def/*.h，校验格式并生成 HTML 文档。
文档中的源文件链接路径相对于 HTML 输出目录。
"""

from __future__ import annotations

import argparse
import html
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

_ASSETS_DIR = Path(__file__).resolve().parent / "assets"

_HEAD_THEME_BOOT = """<script>
(function () {
    var PRISM = {
        dark: "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/themes/prism-tomorrow.min.css",
        light: "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/themes/prism.min.css"
    };
    var saved = localStorage.getItem("protocol-doc-theme");
    var theme = saved === "light" || saved === "dark" ? saved : "dark";
    document.documentElement.setAttribute("data-theme", theme);
    var link = document.createElement("link");
    link.id = "prism-theme";
    link.rel = "stylesheet";
    link.href = PRISM[theme];
    document.head.appendChild(link);
})();
</script>"""


def _load_asset(name: str) -> str:
    return (_ASSETS_DIR / name).read_text(encoding="utf-8")




# ---------------------------------------------------------------------------
# 数据模型
# ---------------------------------------------------------------------------

@dataclass
class FormatIssue:
    file: Path
    line: int
    reason: str


@dataclass
class TypeRef:
    name: str
    line: int
    kind: str  # void | struct | ack
    members: List[str] = field(default_factory=list)


@dataclass
class MessageDoc:
    msg_id: str
    name: str
    brief: str
    data_source: Optional[str]
    ack_source: Optional[str]
    source_file: Path
    block_line: int


@dataclass
class EnumMemberDoc:
    name: str
    value: str
    comment: str = ""


@dataclass
class PrivateTypeDoc:
    name: str
    brief: str
    kind: str  # enum | struct
    source: str
    line: int
    enum_members: List[EnumMemberDoc] = field(default_factory=list)


@dataclass
class ModuleDoc:
    file: Path
    title: str
    messages: List[MessageDoc]
    enums: List[PrivateTypeDoc] = field(default_factory=list)
    structs: List[PrivateTypeDoc] = field(default_factory=list)


@dataclass
class IdRangeGroup:
    title: str
    range_text: str
    start: int
    end: int
    order: int
    capacity: int


@dataclass
class MacroIdEntry:
    name: str
    msg_id: str
    brief: str
    line: int


@dataclass
class ProtocolVersion:
    major: int
    minor: int
    patch: int
    release: int
    source_file: Path
    line: int
    release_line: int


# ---------------------------------------------------------------------------
# 正则
# ---------------------------------------------------------------------------

RE_MSG_ID_DEF = re.compile(
    r"^\s*(COM_HOST_MSG_ID_\w+)\s*=\s*(0x[0-9A-Fa-f]+)",
    re.MULTILINE,
)
RE_ID_RANGE_SECTION = re.compile(
    r"//\s*(.+?)\s*\((0x[0-9A-Fa-f]+)\s*~\s*(0x[0-9A-Fa-f]+)\)(?:\s*(\d+)个)?",
    re.IGNORECASE,
)
RE_MACRO_LINE = re.compile(r"^\s*(COM_HOST_MSG_ID_\w+)\s*=\s*(0x[0-9A-Fa-f]+)")
RE_MSG_SEPARATOR = re.compile(
    r"^/\*+msg id : (0x[0-9A-Fa-f]{4})\*+/?\*?$",
    re.IGNORECASE,
)
RE_SECTION_ENUM = re.compile(r"/\*+.*私有枚举.*\*+/")
RE_SECTION_STRUCT = re.compile(r"/\*+.*私有结构体.*\*+/")
RE_SECTION_DATA = re.compile(r"/\*+.*数据/应答结构体.*\*+/")
RE_SECTION_COMMON_ENUM = re.compile(r"/\*+.*通用枚举.*\*+/")
RE_SECTION_COMMON_STRUCT = re.compile(r"/\*+.*通用结构体.*\*+/")
RE_SECTION_PACK_CLOSE = re.compile(r"#pragma pack\(\)")
RE_MODULE_TITLE = re.compile(r"/\*+(.+?)(?:私有枚举|数据/应答结构体).*\*+/")
RE_DOXYGEN_NAME = re.compile(r"@name\s+(COM_HOST_MSG_ID_\w+)")
RE_DOXYGEN_BRIEF = re.compile(r"@brief\s+(.+)")
RE_DATA_COMMENT = re.compile(r"^//数据\s*$")
RE_ACK_COMMENT = re.compile(r"^//应答\s*$")
RE_TYPEDEF_VOID = re.compile(r"^typedef\s+void\*\s+(com_host_msg_\w+_data_t)\s*;")
RE_TYPEDEF_STRUCT = re.compile(
    r"^typedef\s+struct\s+(com_host_msg_\w+_(?:data|ack)_t)\s*\{",
)
RE_TYPEDEF_ENUM = re.compile(r"^typedef\s+enum\s+(\w+)\s*\{")
RE_PRIVATE_STRUCT = re.compile(r"^typedef\s+struct\s+(com_host_msg_\w+_t)\s*\{")
RE_TYPE_ALIAS = re.compile(r"^typedef\s+[\w]+\s+(\w+_t)\s*;", re.MULTILINE)
RE_RESULT_FIELD = re.compile(r"com_host_msg_ret_t\s+result\s*;")
RE_PROTOCOL_VERSION = re.compile(
    r"#define\s+COM_HOST_PROTOCOL_VERSION_(MAJOR|MINOR|PATCH|RELEASE)\s+(\d+)",
)


# ---------------------------------------------------------------------------
# 工具函数
# ---------------------------------------------------------------------------

def read_lines(path: Path) -> List[str]:
    return path.read_text(encoding="utf-8").splitlines()


def rel_link(target: Path, output_file: Path, line: int) -> str:
    rel = os.path.relpath(str(target.resolve()), str(output_file.parent.resolve()))
    return f"{Path(rel).as_posix()}#L{line}"


def macro_to_data_type(macro: str) -> str:
    suffix = macro.replace("COM_HOST_MSG_ID_", "").lower()
    return f"com_host_msg_{suffix}_data_t"


def macro_to_ack_type(macro: str) -> str:
    suffix = macro.replace("COM_HOST_MSG_ID_", "").lower()
    return f"com_host_msg_{suffix}_ack_data_t"


def normalize_msg_id(msg_id: str) -> str:
    return f"0x{int(msg_id, 16):04X}"


def parse_msg_id_defs(path: Path) -> Dict[str, str]:
    text = path.read_text(encoding="utf-8")
    return {macro: normalize_msg_id(value) for macro, value in RE_MSG_ID_DEF.findall(text)}


def parse_id_range_sections(path: Path) -> Tuple[List[IdRangeGroup], Dict[str, IdRangeGroup]]:
    """解析 com_msg_id_def.h 中的分段注释，并按枚举书写顺序归属宏。"""
    ranges: List[IdRangeGroup] = []
    macro_range: Dict[str, IdRangeGroup] = {}
    current: Optional[IdRangeGroup] = None

    for line in read_lines(path):
        m_range = RE_ID_RANGE_SECTION.search(line)
        if m_range:
            start = int(m_range.group(2), 16)
            end = int(m_range.group(3), 16)
            capacity = int(m_range.group(4)) if m_range.group(4) else end - start + 1
            current = IdRangeGroup(
                title=m_range.group(1).strip(),
                range_text=(
                    f"{normalize_msg_id(m_range.group(2))}"
                    f"~{normalize_msg_id(m_range.group(3))}"
                ),
                start=start,
                end=end,
                order=len(ranges),
                capacity=capacity,
            )
            ranges.append(current)
            continue
        m_macro = RE_MACRO_LINE.match(line)
        if m_macro and current is not None:
            macro_range[m_macro.group(1)] = current

    return ranges, macro_range


def parse_macro_entries(path: Path) -> Dict[str, MacroIdEntry]:
    entries: Dict[str, MacroIdEntry] = {}
    for i, line in enumerate(read_lines(path), 1):
        m = RE_MACRO_LINE.match(line)
        if not m:
            continue
        brief = line.split("//", 1)[1].strip() if "//" in line else m.group(1)
        entries[m.group(1)] = MacroIdEntry(
            name=m.group(1),
            msg_id=normalize_msg_id(m.group(2)),
            brief=brief,
            line=i,
        )
    return entries


def module_id_range_order(mod: ModuleDoc, macro_range: Dict[str, IdRangeGroup]) -> Optional[int]:
    for msg in mod.messages:
        rg = macro_range.get(msg.name)
        if rg is not None:
            return rg.order
    return None


RE_FIELD_TYPE_REF = re.compile(r"\b(com_host_msg_\w+_t)\b")
RE_ENUM_MEMBER = re.compile(
    r"^(COM_HOST_MSG_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*,?\s*$",
    re.IGNORECASE,
)


def build_type_registry(
    modules: List[ModuleDoc],
    extra_types: Optional[List[PrivateTypeDoc]] = None,
) -> Dict[str, PrivateTypeDoc]:
    registry: Dict[str, PrivateTypeDoc] = {}
    for mod in modules:
        for type_doc in mod.enums + mod.structs:
            registry.setdefault(type_doc.name, type_doc)
    for type_doc in extra_types or []:
        registry.setdefault(type_doc.name, type_doc)
    return registry


def msg_type_exclude(msg: MessageDoc) -> set[str]:
    excluded: set[str] = set()
    for src in (msg.data_source, msg.ack_source):
        if not src:
            continue
        name = extract_typedef_name(src)
        if name:
            excluded.add(name)
    return excluded


def collect_type_refs(
    sources: List[Optional[str]],
    registry: Dict[str, PrivateTypeDoc],
    kinds: Tuple[str, ...] = ("enum", "struct"),
    exclude: Optional[set[str]] = None,
) -> List[PrivateTypeDoc]:
    refs: List[PrivateTypeDoc] = []
    seen: set[str] = set()
    excluded = exclude or set()
    for src in sources:
        if not src:
            continue
        for match in RE_FIELD_TYPE_REF.finditer(src):
            name = match.group(1)
            if name in seen or name in excluded:
                continue
            type_doc = registry.get(name)
            if type_doc is None or type_doc.kind not in kinds:
                continue
            seen.add(name)
            refs.append(type_doc)
    return refs


def render_jump_buttons(types: List[PrivateTypeDoc]) -> str:
    if not types:
        return ""
    buttons = ""
    for type_doc in types:
        anchor = type_anchor(type_doc.name)
        btn_cls = "jump-btn--enum" if type_doc.kind == "enum" else "jump-btn--struct"
        buttons += (
            f'<a href="#{html.escape(anchor)}" class="jump-btn {btn_cls}" '
            f'title="{html.escape(type_doc.brief)}" data-jump-target="{html.escape(anchor)}">'
            f"{html.escape(type_doc.name)}</a>"
        )
    return (
        '<div class="card-jumps">'
        '<span class="card-jumps-label">跳转</span>'
        f"{buttons}"
        "</div>"
    )


def normalize_enum_value(value: str) -> str:
    if value.lower().startswith("0x"):
        num = int(value, 16)
        width = max(2, len(value) - 2)
        return f"0x{num:0{width}X}"
    return value


def parse_enum_members(block_lines: List[str]) -> List[EnumMemberDoc]:
    members: List[EnumMemberDoc] = []
    depth = 0
    for line in block_lines:
        depth += line.count("{") - line.count("}")
        if depth < 1:
            continue
        comment = ""
        code_part = line
        if "//" in line:
            code_part, _, tail = line.partition("//")
            comment = tail.strip()
        stripped = code_part.strip().rstrip(",")
        if not stripped or stripped in ("{", "}"):
            continue
        match = RE_ENUM_MEMBER.match(stripped)
        if not match:
            continue
        members.append(
            EnumMemberDoc(
                name=match.group(1),
                value=normalize_enum_value(match.group(2)),
                comment=comment,
            )
        )
    return members


def type_search_key(type_doc: PrivateTypeDoc) -> str:
    parts = [type_doc.name, type_doc.brief, type_doc.kind]
    parts.extend(member.name for member in type_doc.enum_members)
    return " ".join(parts).lower()


_TOC_TYPE_STYLE = {
    "enum": ("toc-leaf--enum", "toc-id--enum", "枚举"),
    "struct": ("toc-leaf--struct", "toc-id--struct", "结构"),
}


def render_toc_type_leaf(type_doc: PrivateTypeDoc) -> str:
    leaf_cls, id_cls, label = _TOC_TYPE_STYLE[type_doc.kind]
    sk = html.escape(type_search_key(type_doc), quote=True)
    title = html.escape(type_doc.brief)
    name = html.escape(type_doc.name)
    return (
        f'<li class="toc-leaf {leaf_cls}" data-search="{sk}"'
        f' data-toc-id="{html.escape(label)}" data-toc-title="{title}">'
        f'<a href="#type-{name}">'
        f'<span class="toc-id {id_cls}">{html.escape(label)}</span>'
        f'<span class="toc-title">{title}</span>'
        f"</a></li>\n"
    )


def render_toc_subgroup(title: str, items_html: str, subgroup_cls: str = "") -> str:
    if not items_html:
        return ""
    cls = f"toc-subgroup {subgroup_cls}".strip()
    return (
        f'<li class="{cls}">'
        f'<details class="toc-details toc-details--nested">'
        f'<summary class="toc-subgroup-title">{html.escape(title)}</summary>'
        f'<ul class="toc-sub toc-sub--nested">{items_html}</ul>'
        f"</details></li>\n"
    )


def build_toc_html(
    messages: List[MessageDoc],
    modules: List[ModuleDoc],
    id_ranges: List[IdRangeGroup],
    macro_range: Dict[str, IdRangeGroup],
    macro_entries: Dict[str, MacroIdEntry],
    msg_id_def_path: Path,
    output_file: Path,
) -> str:
    grouped: Dict[int, List[MessageDoc]] = defaultdict(list)
    enums_by_range: Dict[int, List[PrivateTypeDoc]] = defaultdict(list)
    structs_by_range: Dict[int, List[PrivateTypeDoc]] = defaultdict(list)
    documented_macros: set[str] = set()

    for mod in modules:
        order = module_id_range_order(mod, macro_range)
        if order is not None:
            enums_by_range[order].extend(mod.enums)
            structs_by_range[order].extend(mod.structs)

    for msg in messages:
        documented_macros.add(msg.name)
        rg = macro_range.get(msg.name)
        if rg is not None:
            grouped[rg.order].append(msg)

    toc_parts: List[str] = []
    for rg in id_ranges:
        msgs = sorted(grouped.get(rg.order, []), key=lambda m: int(m.msg_id, 16))
        range_enums = enums_by_range.get(rg.order, [])
        range_structs = structs_by_range.get(rg.order, [])
        id_only_macros = sorted(
            (
                entry
                for name, entry in macro_entries.items()
                if name not in documented_macros
                and macro_range.get(name) is not None
                and macro_range[name].order == rg.order
            ),
            key=lambda e: int(e.msg_id, 16),
        )
        if not msgs and not id_only_macros and not range_enums and not range_structs:
            continue

        used = sum(1 for rg2 in macro_range.values() if rg2.order == rg.order)
        usage_text = f"{used}/{rg.capacity}"

        sub_items = ""
        enum_items = "".join(render_toc_type_leaf(type_doc) for type_doc in range_enums)
        struct_items = "".join(render_toc_type_leaf(type_doc) for type_doc in range_structs)
        msg_items = ""
        for msg in msgs:
            sk = html.escape(msg_search_key(msg), quote=True)
            msg_items += (
                f'<li class="toc-leaf" data-search="{sk}"'
                f' data-toc-id="{html.escape(msg.msg_id)}"'
                f' data-toc-title="{html.escape(msg.brief)}">'
                f'<a href="#msg-{html.escape(msg.name)}">'
                f'<span class="toc-id">{html.escape(msg.msg_id)}</span>'
                f'<span class="toc-title">{html.escape(msg.brief)}</span>'
                f"</a></li>\n"
            )
        for entry in id_only_macros:
            sk = html.escape(
                f"{entry.msg_id} {entry.name} {entry.brief}".lower(),
                quote=True,
            )
            href = html.escape(rel_link(msg_id_def_path, output_file, entry.line))
            msg_items += (
                f'<li class="toc-leaf toc-leaf--id-only" data-search="{sk}"'
                f' data-toc-id="{html.escape(entry.msg_id)}"'
                f' data-toc-title="{html.escape(entry.brief)}">'
                f'<a href="{href}">'
                f'<span class="toc-id">{html.escape(entry.msg_id)}</span>'
                f'<span class="toc-title">{html.escape(entry.brief)}</span>'
                f'<span class="toc-badge">仅ID</span>'
                f"</a></li>\n"
            )

        if enum_items:
            sub_items += render_toc_subgroup("私有枚举", enum_items, "toc-subgroup--enum")
        if struct_items:
            sub_items += render_toc_subgroup("私有结构体", struct_items, "toc-subgroup--struct")
        sub_items += msg_items

        toc_parts.append(
            f'<li class="toc-group" data-search="{html.escape(rg.title.lower() + " " + rg.range_text.lower(), quote=True)}">'
            f'<details class="toc-details">'
            f'<summary class="toc-range">'
            f'<span class="toc-range-body">'
            f'<span class="toc-range-title">{html.escape(rg.title)}</span>'
            f'<span class="toc-range-foot">'
            f'<span class="toc-range-id">{html.escape(rg.range_text)}</span>'
            f'<span class="toc-range-usage" title="使用数/总数">{html.escape(usage_text)}</span>'
            f"</span>"
            f"</span></summary>"
            f'<ul class="toc-sub">{sub_items}</ul>'
            f"</details></li>\n"
        )

    return "".join(toc_parts)


def msg_search_key(msg: MessageDoc) -> str:
    return f"{msg.msg_id} {msg.name} {msg.brief}".lower()


def resolve_protocol_dir(protocol_dir: Path) -> Path:
    protocol_dir = protocol_dir.resolve()
    if not protocol_dir.is_dir():
        raise NotADirectoryError(f"协议目录不存在: {protocol_dir}")
    return protocol_dir


def resolve_source_files(protocol_dir: Path) -> List[Path]:
    cmd_dir = protocol_dir / "cmd_data_def"
    if not cmd_dir.is_dir():
        raise FileNotFoundError(f"未找到 cmd_data_def 目录: {cmd_dir}")
    files = sorted(cmd_dir.glob("*.h"))
    if not files:
        raise ValueError(f"{cmd_dir} 下没有 .h 头文件")
    return [f.resolve() for f in files]


def find_msg_id_def_path(protocol_dir: Path) -> Path:
    path = (protocol_dir / "com_msg_id_def.h").resolve()
    if not path.exists():
        raise FileNotFoundError(f"未找到 com_msg_id_def.h: {path}")
    return path


def find_protocol_def_path(protocol_dir: Path) -> Optional[Path]:
    for candidate in (
        protocol_dir.parent / "protocol_def.h",
        protocol_dir / "protocol_def.h",
    ):
        if candidate.exists():
            return candidate.resolve()
    return None


def parse_protocol_version(path: Path) -> Optional[ProtocolVersion]:
    lines = read_lines(path)
    values: Dict[str, Optional[int]] = {
        "MAJOR": None,
        "MINOR": None,
        "PATCH": None,
        "RELEASE": None,
    }
    first_line = 0
    release_line = 0
    for i, line in enumerate(lines, 1):
        m = RE_PROTOCOL_VERSION.search(line)
        if not m:
            continue
        key = m.group(1)
        if not first_line:
            first_line = i
        if key == "RELEASE":
            release_line = i
        values[key] = int(m.group(2))
    if values["MAJOR"] is None:
        return None
    return ProtocolVersion(
        major=values["MAJOR"] or 0,
        minor=values["MINOR"] or 0,
        patch=values["PATCH"] or 0,
        release=values["RELEASE"] or 0,
        source_file=path,
        line=first_line or 1,
        release_line=release_line if release_line else first_line or 1,
    )


def format_protocol_version(version: ProtocolVersion) -> str:
    return f"v{version.major}.{version.minor}.{version.patch}"


def format_protocol_channel(version: ProtocolVersion) -> Tuple[str, str]:
    if version.release == 1:
        return "Release", "version-channel--release"
    return "Debug", "version-channel--debug"


def extract_module_title(lines: List[str], filename: str) -> str:
    for line in lines:
        m = RE_MODULE_TITLE.search(line)
        if m:
            title = m.group(1).strip().strip("*").strip()
            if title:
                return title
    stem = Path(filename).stem.replace("_cmd_data_def", "").replace("_", " ")
    return stem


_CMD_SECTION_STOPS = [RE_SECTION_ENUM, RE_SECTION_STRUCT, RE_SECTION_DATA]


def section_span(
    lines: List[str],
    section_re: re.Pattern[str],
    stop_res: Optional[List[re.Pattern[str]]] = None,
) -> Optional[Tuple[int, int]]:
    stops = stop_res if stop_res is not None else _CMD_SECTION_STOPS
    start = next((i for i, line in enumerate(lines) if section_re.search(line)), None)
    if start is None:
        return None
    end = next((i for i in range(start + 1, len(lines)) if any(r.search(lines[i]) for r in stops)), len(lines))
    return start, end


def parse_common_type_docs(protocol_dir: Path) -> List[PrivateTypeDoc]:
    path = (protocol_dir / "com_msg_comm.h").resolve()
    if not path.exists():
        return []
    lines = read_lines(path)
    docs: List[PrivateTypeDoc] = []
    enum_span = section_span(lines, RE_SECTION_COMMON_ENUM, [RE_SECTION_COMMON_STRUCT, RE_SECTION_PACK_CLOSE])
    struct_span = section_span(lines, RE_SECTION_COMMON_STRUCT, [RE_SECTION_PACK_CLOSE])
    if enum_span:
        docs.extend(parse_private_types(path, lines, enum_span, "enum"))
    if struct_span:
        docs.extend(parse_private_types(path, lines, struct_span, "struct"))
    return docs


def extract_struct_members(block_lines: List[str], struct_start: int) -> List[str]:
    members: List[str] = []
    depth = 0
    for i in range(struct_start, len(block_lines)):
        line = block_lines[i]
        depth += line.count("{") - line.count("}")
        if i > struct_start and depth <= 0:
            break
        stripped = line.strip()
        if not stripped or stripped.startswith("//") or stripped in ("{", "}"):
            continue
        if stripped.startswith("typedef") or stripped.startswith("struct"):
            continue
        members.append(stripped)
    return members


def parse_typedef_after(
    lines: List[str], start_idx: int, end_idx: int
) -> Optional[Tuple[TypeRef, int]]:
    for i in range(start_idx, end_idx):
        stripped = lines[i].strip()
        m_void = RE_TYPEDEF_VOID.match(stripped)
        if m_void:
            return TypeRef(m_void.group(1), 0, "void"), i
        m_struct = RE_TYPEDEF_STRUCT.match(stripped)
        if m_struct:
            members = extract_struct_members(lines, i)
            kind = "ack" if "_ack_data_t" in m_struct.group(1) else "struct"
            return TypeRef(m_struct.group(1), 0, kind, members), i
    return None


def abs_line(block_abs_start: int, cursor: int, rel_idx: int) -> int:
    return block_abs_start + cursor + rel_idx + 1


def find_typedef_end(lines: List[str], start: int) -> int:
    if start >= len(lines):
        return start
    stripped = lines[start].strip()
    if RE_TYPEDEF_VOID.match(stripped):
        return start
    depth = 0
    for i in range(start, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if i > start and depth <= 0 and "}" in lines[i]:
            return i
    return start


def extract_type_source(
    rest: List[str], comment_idx: Optional[int], typedef_idx: int
) -> str:
    end = find_typedef_end(rest, typedef_idx)
    start = comment_idx if comment_idx is not None else typedef_idx
    chunk = rest[start : end + 1]
    while chunk and not chunk[-1].strip():
        chunk.pop()
    return "\n".join(chunk)


def extract_typedef_name(src: str) -> Optional[str]:
    """从 typedef 源码中提取结构体类型名。"""
    m = re.search(r"\}\s*(\w+)\s*;", src)
    if m:
        return m.group(1)
    m = re.search(r"typedef\s+struct\s+(\w+)\s*\{", src)
    if m:
        return m.group(1)
    return None


def is_msg_data_type(name: str) -> bool:
    return name.endswith("_data_t") or name.endswith("_ack_data_t")


def find_private_decl_end(lines: List[str], start: int) -> int:
    depth = 0
    for i in range(start, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if i > start and depth <= 0 and "}" in lines[i]:
            end = i
            j = i + 1
            while j < len(lines) and not lines[j].strip():
                j += 1
            if j < len(lines) and RE_TYPE_ALIAS.match(lines[j].strip()):
                return j
            return end
    return start


def extract_private_type_name(block_lines: List[str]) -> str:
    text = "\n".join(block_lines)
    alias_m = RE_TYPE_ALIAS.search(text)
    if alias_m:
        return alias_m.group(1)
    close_m = re.search(r"\}\s*(\w+)\s*;", text)
    if close_m:
        return close_m.group(1)
    first = block_lines[0].strip()
    enum_m = RE_TYPEDEF_ENUM.match(first)
    if enum_m:
        return enum_m.group(1)
    struct_m = RE_PRIVATE_STRUCT.match(first)
    if struct_m:
        return struct_m.group(1)
    return ""


def parse_line_brief(line: str) -> Optional[str]:
    stripped = line.strip()
    if not stripped.startswith("//") or stripped == "//空":
        return None
    return stripped[2:].strip()


def parse_private_types(
    path: Path,
    lines: List[str],
    section_span: Tuple[int, int],
    kind: str,
) -> List[PrivateTypeDoc]:
    start, end = section_span
    body = lines[start + 1 : end]
    docs: List[PrivateTypeDoc] = []
    pending_brief: Optional[str] = None
    comment_line_idx: Optional[int] = None
    i = 0
    while i < len(body):
        stripped = body[i].strip()
        if not stripped or stripped == "//空":
            i += 1
            continue

        brief = parse_line_brief(body[i])
        if brief is not None and not RE_TYPEDEF_ENUM.match(stripped) and not RE_PRIVATE_STRUCT.match(stripped):
            pending_brief = brief
            comment_line_idx = i
            i += 1
            continue

        is_enum = RE_TYPEDEF_ENUM.match(stripped) is not None
        struct_m = RE_PRIVATE_STRUCT.match(stripped)
        is_struct = struct_m is not None and not is_msg_data_type(struct_m.group(1))
        if not is_enum and not is_struct:
            i += 1
            continue
        if kind == "enum" and not is_enum:
            i += 1
            continue
        if kind == "struct" and not is_struct:
            i += 1
            continue

        block_end = find_private_decl_end(body, i)
        block = body[i : block_end + 1]
        type_name = extract_private_type_name(block)
        src_start = comment_line_idx if comment_line_idx is not None and comment_line_idx < i else i
        source = "\n".join(ln.rstrip() for ln in body[src_start : block_end + 1])
        line_no = start + 1 + i + 1
        enum_members = parse_enum_members(block) if is_enum else []
        docs.append(
            PrivateTypeDoc(
                name=type_name,
                brief=pending_brief or type_name,
                kind=kind,
                source=source,
                line=line_no,
                enum_members=enum_members,
            )
        )
        pending_brief = None
        comment_line_idx = None
        i = block_end + 1
    return docs


# ---------------------------------------------------------------------------
# 校验与解析
# ---------------------------------------------------------------------------

def validate_file_structure(path: Path, lines: List[str], issues: List[FormatIssue]) -> None:
    def add(line: int, reason: str) -> None:
        issues.append(FormatIssue(path, line, reason))

    if not lines or lines[0].strip() != "#pragma once":
        add(1, "文件首行必须为 #pragma once")
    if not any('#include "../com_msg_comm.h"' in ln for ln in lines):
        add(1, '必须包含 #include "../com_msg_comm.h"')

    pack_open = [i + 1 for i, ln in enumerate(lines) if ln.strip() == "#pragma pack(1)"]
    pack_close = [i + 1 for i, ln in enumerate(lines) if ln.strip() == "#pragma pack()"]
    if not pack_open:
        add(1, "缺少 #pragma pack(1)")
    if not pack_close:
        add(len(lines), "缺少 #pragma pack()")
    if pack_open and pack_close and pack_open[0] >= pack_close[-1]:
        add(pack_open[0], "#pragma pack(1) 必须出现在 #pragma pack() 之前")

    enum_span = section_span(lines, RE_SECTION_ENUM)
    struct_span = section_span(lines, RE_SECTION_STRUCT)
    data_span = section_span(lines, RE_SECTION_DATA)
    if enum_span is None:
        add(1, "缺少「私有枚举」分段注释")
    if struct_span is None:
        add(1, "缺少「私有结构体」分段注释")
    if data_span is None:
        add(1, "缺少「数据/应答结构体」分段注释")
    if enum_span and struct_span and data_span and not (enum_span[0] < struct_span[0] < data_span[0]):
        add(enum_span[0] + 1, "分段顺序必须为: 私有枚举 → 私有结构体 → 数据/应答结构体")

    if enum_span:
        enum_body = lines[enum_span[0] + 1 : enum_span[1]]
        has_enum = any("typedef enum" in ln for ln in enum_body)
        has_empty = any(ln.strip() == "//空" for ln in enum_body)
        if has_enum:
            for j, ln in enumerate(enum_body):
                if RE_TYPEDEF_ENUM.match(ln.strip()):
                    prev = enum_body[j - 1].strip() if j > 0 else ""
                    if not prev.startswith("//") or prev == "//空":
                        add(enum_span[0] + 1 + j + 1, "私有枚举 typedef 前必须有描述性 // 注释")
        elif not has_empty:
            add(enum_span[0] + 1, "私有枚举段无定义时必须包含 //空")

    if struct_span:
        struct_body = lines[struct_span[0] + 1 : struct_span[1]]
        has_struct = any("typedef struct" in ln for ln in struct_body)
        has_empty = any(ln.strip() == "//空" for ln in struct_body)
        if has_struct:
            for j, ln in enumerate(struct_body):
                if RE_TYPEDEF_STRUCT.match(ln.strip()):
                    prev = struct_body[j - 1].strip() if j > 0 else ""
                    if not prev.startswith("//") or prev == "//空":
                        add(struct_span[0] + 1 + j + 1, "私有结构体 typedef 前必须有描述性 // 注释")
        elif not has_empty:
            add(struct_span[0] + 1, "私有结构体段无定义时必须包含 //空")


def find_message_blocks(
    path: Path, lines: List[str], msg_id_defs: Dict[str, str], issues: List[FormatIssue]
) -> List[MessageDoc]:
    data_span = section_span(lines, RE_SECTION_DATA)
    if data_span is None:
        return []

    data_start, data_end = data_span
    data_lines = lines[data_start:data_end]
    separator_indices = [
        i for i, ln in enumerate(data_lines) if RE_MSG_SEPARATOR.match(ln.strip())
    ]
    messages: List[MessageDoc] = []

    for idx, sep_rel in enumerate(separator_indices):
        sep_abs = data_start + sep_rel + 1
        block_end_rel = separator_indices[idx + 1] if idx + 1 < len(separator_indices) else len(data_lines)
        block = data_lines[sep_rel:block_end_rel]
        block_abs_start = data_start + sep_rel

        def add(line_offset: int, reason: str) -> None:
            issues.append(FormatIssue(path, block_abs_start + line_offset + 1, reason))

        sep_m = RE_MSG_SEPARATOR.match(block[0].strip())
        if not sep_m:
            add(0, "消息分隔行格式错误")
            continue

        block_msg_id = normalize_msg_id(sep_m.group(1))
        cursor = 1
        while cursor < len(block) and not block[cursor].strip().startswith("/**"):
            if block[cursor].strip():
                add(cursor, "分隔行与 Doxygen 注释块之间不允许有非空内容")
            cursor += 1
        if cursor >= len(block) or block[cursor].strip() != "/**":
            add(max(cursor, 1), "缺少 Doxygen 注释块 (/** ... */)")
            continue

        doxy_lines: List[str] = []
        while cursor < len(block):
            doxy_lines.append(block[cursor])
            if block[cursor].strip().endswith("*/"):
                cursor += 1
                break
            cursor += 1
        else:
            add(cursor, "Doxygen 注释块未闭合")
            continue

        doxy_text = "\n".join(doxy_lines)
        name_m = RE_DOXYGEN_NAME.search(doxy_text)
        brief_m = RE_DOXYGEN_BRIEF.search(doxy_text)
        if not name_m:
            add(1, "Doxygen 注释块缺少 @name COM_HOST_MSG_ID_xxx")
            continue
        if not brief_m:
            add(1, "Doxygen 注释块缺少 @brief 功能描述")
            continue

        macro = name_m.group(1)
        brief = brief_m.group(1).strip()
        if macro not in msg_id_defs:
            add(1, f"@name {macro} 未在 com_msg_id_def.h 中定义")
            continue
        if block_msg_id != msg_id_defs[macro]:
            add(0, f"分隔行 msg id {block_msg_id} 与 @name 在 com_msg_id_def.h 中的值 {msg_id_defs[macro]} 不一致")

        rest = block[cursor:]
        data_ref: Optional[TypeRef] = None
        ack_ref: Optional[TypeRef] = None
        data_source: Optional[str] = None
        ack_source: Optional[str] = None
        data_typedef_idx: Optional[int] = None

        data_idx = next((i for i, ln in enumerate(rest) if RE_DATA_COMMENT.match(ln.strip())), None)
        if data_idx is not None:
            parsed = parse_typedef_after(rest, data_idx + 1, len(rest))
            if parsed is None:
                add(cursor + data_idx + 1, "//数据 后缺少有效的 typedef 定义")
            else:
                data_ref, rel = parsed
                data_ref.line = abs_line(block_abs_start, cursor, rel)
                data_typedef_idx = rel
                data_source = extract_type_source(rest, data_idx, rel)
        else:
            direct = parse_typedef_after(rest, 0, len(rest))
            if direct is not None:
                type_ref, rel = direct
                if type_ref.kind != "ack" and "_ack_data_t" not in type_ref.name:
                    add(cursor, "数据 typedef 前必须有 //数据 注释")
                    type_ref.line = abs_line(block_abs_start, cursor, rel)
                    data_ref = type_ref
                    data_typedef_idx = rel
                    data_source = extract_type_source(rest, None, rel)

        ack_search_from = (find_typedef_end(rest, data_typedef_idx) + 1) if data_typedef_idx is not None else 0
        ack_idx = next(
            (i for i, ln in enumerate(rest[ack_search_from:], ack_search_from)
             if RE_ACK_COMMENT.match(ln.strip())),
            None,
        )
        if ack_idx is not None:
            parsed = parse_typedef_after(rest, ack_idx + 1, len(rest))
            if parsed is None:
                add(cursor + ack_idx + 1, "//应答 后缺少有效的 typedef 定义")
            else:
                ack_ref, rel = parsed
                ack_ref.line = abs_line(block_abs_start, cursor, rel)
                ack_source = extract_type_source(rest, ack_idx, rel)

        if data_ref and data_ref.name != macro_to_data_type(macro):
            add(data_ref.line - block_abs_start - 1, f"数据类型命名应为 {macro_to_data_type(macro)}，实际为 {data_ref.name}")
        if ack_ref and ack_ref.name != macro_to_ack_type(macro):
            add(ack_ref.line - block_abs_start - 1, f"应答类型命名应为 {macro_to_ack_type(macro)}，实际为 {ack_ref.name}")
        if ack_ref and ack_ref.kind == "ack" and macro != "COM_HOST_MSG_ID_PING":
            if not RE_RESULT_FIELD.search("\n".join(ack_ref.members)):
                add(ack_ref.line - block_abs_start - 1, "应答结构体首个字段必须为 com_host_msg_ret_t result")

        if data_ref is None and ack_ref is None:
            add(0, "消息块未定义数据或应答类型")
        else:
            messages.append(
                MessageDoc(
                    block_msg_id, macro, brief,
                    data_source, ack_source, path, sep_abs,
                )
            )
    return messages


# ---------------------------------------------------------------------------
# HTML 生成
# ---------------------------------------------------------------------------

def build_type_anchor_map(
    modules: List[ModuleDoc],
    messages: List[MessageDoc],
    extra_types: Optional[List[PrivateTypeDoc]] = None,
) -> Dict[str, str]:
    anchors: Dict[str, str] = {}

    def add_private_type(type_doc: PrivateTypeDoc) -> None:
        anchor = type_anchor(type_doc.name)
        anchors[type_doc.name] = anchor
        if type_doc.kind == "enum" and type_doc.name.endswith("_t"):
            anchors[type_doc.name[:-2] + "_e"] = anchor
        if type_doc.kind == "enum":
            for member in type_doc.enum_members:
                anchors[member.name] = anchor

    for mod in modules:
        for type_doc in mod.enums + mod.structs:
            add_private_type(type_doc)
    for type_doc in extra_types or []:
        if type_doc.name not in anchors:
            add_private_type(type_doc)
    for msg in messages:
        msg_anchor = f"msg-{msg.name}"
        for src in (msg.data_source, msg.ack_source):
            if not src:
                continue
            name = extract_typedef_name(src)
            if name and name not in anchors:
                anchors[name] = msg_anchor
    return anchors


def linkify_source(src: str, anchor_map: Dict[str, str]) -> str:
    if not anchor_map:
        return html.escape(src)
    names = sorted(anchor_map.keys(), key=len, reverse=True)
    pattern = re.compile(r"\b(" + "|".join(re.escape(name) for name in names) + r")\b")

    def replace_match(match: re.Match[str]) -> str:
        name = match.group(1)
        anchor = anchor_map[name]
        cls = "type-ref type-ref--enum" if name.startswith("COM_HOST_MSG_") else "type-ref"
        return f'<a href="#{anchor}" class="{cls}">{html.escape(name)}</a>'

    parts: List[str] = []
    last = 0
    for match in pattern.finditer(src):
        parts.append(html.escape(src[last : match.start()]))
        parts.append(replace_match(match))
        last = match.end()
    parts.append(html.escape(src[last:]))
    return "".join(parts)


def render_enum_values_list(type_doc: PrivateTypeDoc) -> str:
    if not type_doc.enum_members:
        return '<p class="enum-empty">无枚举项</p>'
    items = ""
    for member in type_doc.enum_members:
        title_attr = ""
        if member.comment:
            title_attr = f' title="{html.escape(member.comment, quote=True)}"'
        comment_html = (
            f'<span class="enum-item-comment">{html.escape(member.comment)}</span>'
            if member.comment
            else ""
        )
        items += (
            f'<div class="enum-item"{title_attr}>'
            f'<div class="enum-item-main">'
            f'<code class="enum-item-name">{html.escape(member.name)}</code>'
            f'<span class="enum-item-code">{html.escape(member.value)}</span>'
            f"</div>"
            f"{comment_html}"
            f"</div>"
        )
    return f'<div class="enum-list">{items}</div>'


def render_enum_card(type_doc: PrivateTypeDoc, mod: ModuleDoc, link) -> str:
    sk = html.escape(type_search_key(type_doc), quote=True)
    jumps_html = ""
    return f"""
            <article class="type-card type-card--enum" id="type-{html.escape(type_doc.name)}" data-search="{sk}">
                <div class="enum-card">
                    <div class="enum-card-head">
                        <div class="enum-caption-head">
                            <span class="type-kind">枚举</span>
                            <span class="type-name">{html.escape(type_doc.name)}</span>
                        </div>
                        <p class="type-brief">{html.escape(type_doc.brief)}</p>
                        {jumps_html}
                        <div class="type-src">{link(mod.file, type_doc.line, f"{mod.file.name}:{type_doc.line}", "src-link")}</div>
                    </div>
                    {render_enum_values_list(type_doc)}
                </div>
            </article>
            """


def render_type_card(
    type_doc: PrivateTypeDoc,
    mod: ModuleDoc,
    link,
    anchor_map: Dict[str, str],
    type_registry: Dict[str, PrivateTypeDoc],
) -> str:
    if type_doc.kind == "enum":
        return render_enum_card(type_doc, mod, link)
    jump_targets = collect_type_refs(
        [type_doc.source],
        type_registry,
        kinds=("enum", "struct"),
        exclude={type_doc.name},
    )
    jumps_html = render_jump_buttons(jump_targets)
    sk = html.escape(type_search_key(type_doc), quote=True)
    return f"""
            <article class="type-card type-card--struct" id="type-{html.escape(type_doc.name)}" data-search="{sk}">
                <div class="type-layout">
                    <div class="type-meta">
                        <div class="type-head">
                            <span class="type-kind">结构体</span>
                            <span class="type-name">{html.escape(type_doc.name)}</span>
                        </div>
                        <p class="type-brief">{html.escape(type_doc.brief)}</p>
                        {jumps_html}
                        <div class="type-src">{link(mod.file, type_doc.line, f"{mod.file.name}:{type_doc.line}", "src-link")}</div>
                    </div>
                    <div class="type-code">
                        {render_code_panel(type_doc.source, None, anchor_map)}
                    </div>
                </div>
            </article>
            """


def render_private_types_section(
    title: str,
    types: List[PrivateTypeDoc],
    mod: ModuleDoc,
    link,
    anchor_map: Dict[str, str],
    type_registry: Dict[str, PrivateTypeDoc],
) -> str:
    if not types:
        return ""
    cards = "".join(render_type_card(t, mod, link, anchor_map, type_registry) for t in types)
    return f"""
            <div class="type-section">
                <h3 class="type-section-title">{html.escape(title)}</h3>
                <div class="type-grid">{cards}</div>
            </div>
            """


def render_code_panel(
    data_src: Optional[str],
    ack_src: Optional[str],
    anchor_map: Optional[Dict[str, str]] = None,
) -> str:
    parts: List[str] = []
    for src in (data_src, ack_src):
        if not src:
            continue
        struct_name = extract_typedef_name(src) or ""
        name_btn = ""
        if struct_name:
            name_btn = (
                f'<button type="button" class="code-copy-name" '
                f'title="复制结构体名" data-struct-name="{html.escape(struct_name, quote=True)}">'
                f"复制名</button>"
            )
        code_html = linkify_source(src, anchor_map) if anchor_map else html.escape(src)
        linked_cls = " code-linked" if anchor_map else ""
        parts.append(
            '<div class="code-block-wrap">'
            '<div class="code-actions">'
            '<button type="button" class="code-copy" title="复制代码">复制</button>'
            f"{name_btn}"
            "</div>"
            f'<pre class="language-c{linked_cls}"><code class="language-c">{code_html}</code></pre>'
            "</div>"
        )
    if not parts:
        return '<p class="code-empty">无</p>'
    return "\n".join(parts)


def type_anchor(type_name: str) -> str:
    return f"type-{type_name}"


def render_msg_card(
    msg: MessageDoc,
    mod: ModuleDoc,
    link,
    type_registry: Dict[str, PrivateTypeDoc],
    anchor_map: Dict[str, str],
) -> str:
    sk = html.escape(msg_search_key(msg), quote=True)
    jumps_html = render_jump_buttons(
        collect_type_refs(
            [msg.data_source, msg.ack_source],
            type_registry,
            kinds=("enum", "struct"),
            exclude=msg_type_exclude(msg),
        )
    )
    loc = f"{mod.file.name}:{msg.block_line}"
    return f"""
            <article class="msg-card" id="msg-{html.escape(msg.name)}" data-search="{sk}">
                <div class="msg-layout">
                    <div class="msg-meta">
                        <div class="msg-head">
                            <span class="msg-id">{html.escape(msg.msg_id)}</span>
                            <span class="msg-name">{html.escape(msg.name)}</span>
                        </div>
                        <p class="msg-brief">{html.escape(msg.brief)}</p>
                        {jumps_html}
                        <div class="msg-src">{link(msg.source_file, msg.block_line, loc, "src-link")}</div>
                    </div>
                    <div class="msg-code">
                        {render_code_panel(msg.data_source, msg.ack_source, anchor_map)}
                    </div>
                </div>
            </article>
            """


def render_issues_section(issues: List[FormatIssue], link) -> str:
    if not issues:
        return ""
    by_file: Dict[str, List[FormatIssue]] = defaultdict(list)
    for issue in issues:
        by_file[issue.file.name].append(issue)
    groups = ""
    for fname, flist in sorted(by_file.items()):
        rows = "".join(
            f'<tr><td class="col-loc">{link(i.file, i.line, f"L{i.line}")}</td>'
            f"<td>{html.escape(i.reason)}</td></tr>"
            for i in sorted(flist, key=lambda x: x.line)
        )
        groups += f"""
            <div class="issue-group">
                <h3>{html.escape(fname)}</h3>
                <table class="issue-table"><tbody>{rows}</tbody></table>
            </div>
            """
    return f"""
        <section class="panel issues-panel" id="issues">
            <h2>格式问题清单</h2>
            <p class="panel-desc">共 {len(issues)} 项，参照 cmd_data_def 模板（分隔行、@name/@brief、//数据、//应答 等）。</p>
            {groups}
        </section>
        """


def render_hero_version(protocol_version: ProtocolVersion, link) -> str:
    ver_text = format_protocol_version(protocol_version)
    channel_label, channel_cls = format_protocol_channel(protocol_version)
    ver_link = link(protocol_version.source_file, protocol_version.line, ver_text, "version-tag")
    channel_link = link(
        protocol_version.source_file,
        protocol_version.release_line,
        channel_label,
        f"version-channel {channel_cls}",
    )
    return f'<p class="hero-version">协议版本 {ver_link} {channel_link}</p>'


def build_html(
    protocol_dir: Path,
    output_file: Path,
    modules: List[ModuleDoc],
    issues: List[FormatIssue],
    messages: List[MessageDoc],
    msg_id_def_path: Path,
    id_ranges: List[IdRangeGroup],
    macro_range: Dict[str, IdRangeGroup],
    protocol_version: Optional[ProtocolVersion] = None,
) -> str:
    total_msgs = len(messages)

    def link(file: Path, line: int, text: str, cls: str = "") -> str:
        href = rel_link(file, output_file, line)
        c = f' class="{cls}"' if cls else ""
        return f'<a href="{html.escape(href)}"{c}>{html.escape(text)}</a>'

    def module_sort_key(mod: ModuleDoc) -> int:
        if mod.messages:
            return min(int(m.msg_id, 16) for m in mod.messages)
        if mod.enums or mod.structs:
            return 0xFFFE
        return 0xFFFF

    sorted_modules = sorted(modules, key=module_sort_key)
    common_types = parse_common_type_docs(protocol_dir)
    common_enums = [t for t in common_types if t.kind == "enum"]
    common_structs = [t for t in common_types if t.kind == "struct"]
    total_enums = sum(len(m.enums) for m in modules) + len(common_enums)
    total_structs = sum(len(m.structs) for m in modules) + len(common_structs)
    type_registry = build_type_registry(modules, common_types)
    type_anchor_map = build_type_anchor_map(modules, messages, common_types)
    toc_items = build_toc_html(
        messages,
        modules,
        id_ranges,
        macro_range,
        parse_macro_entries(msg_id_def_path),
        msg_id_def_path,
        output_file,
    )
    module_sections = ""
    if common_types:
        com_comm_path = (protocol_dir / "com_msg_comm.h").resolve()
        rel_comm = os.path.relpath(str(com_comm_path), str(output_file.parent))
        common_mod = ModuleDoc(com_comm_path, "公共类型", [], common_enums, common_structs)
        common_type_sections = (
            render_private_types_section("通用枚举", common_enums, common_mod, link, type_anchor_map, type_registry)
            + render_private_types_section("通用结构体", common_structs, common_mod, link, type_anchor_map, type_registry)
        )
        module_sections += f"""
        <section class="module" id="mod-common-types">
            <header class="module-head">
                <div>
                    <h2>公共类型</h2>
                    <p class="module-file">{link(com_comm_path, 1, Path(rel_comm).as_posix(), "file-link")}</p>
                </div>
                <div class="module-stat">
                    <span class="pill">{len(common_enums)} 枚举</span>
                    <span class="pill">{len(common_structs)} 结构体</span>
                </div>
            </header>
            {common_type_sections}
        </section>
        """
    for mod in sorted_modules:
        anchor = mod.file.stem
        rel_file = os.path.relpath(str(mod.file), str(output_file.parent))
        msgs_html = "".join(
            render_msg_card(msg, mod, link, type_registry, type_anchor_map)
            for msg in sorted(mod.messages, key=lambda m: int(m.msg_id, 16))
        )

        mod_file_issues = [i for i in issues if i.file == mod.file]
        mod_issue_html = ""
        if mod_file_issues:
            items = "".join(
                f'<li>{link(i.file, i.line, f"{i.file.name}:{i.line}")} — {html.escape(i.reason)}</li>'
                for i in sorted(mod_file_issues, key=lambda x: x.line)
            )
            mod_issue_html = f'<ul class="mod-issues">{items}</ul>'

        type_sections = ""
        type_sections += render_private_types_section("私有枚举", mod.enums, mod, link, type_anchor_map, type_registry)
        type_sections += render_private_types_section("私有结构体", mod.structs, mod, link, type_anchor_map, type_registry)

        stat_pills = [f'<span class="pill">{len(mod.messages)} 条消息</span>']
        if mod.enums:
            stat_pills.append(f'<span class="pill">{len(mod.enums)} 枚举</span>')
        if mod.structs:
            stat_pills.append(f'<span class="pill">{len(mod.structs)} 结构体</span>')
        if mod_file_issues:
            stat_pills.append(f"<span class='pill warn'>{len(mod_file_issues)} 项问题</span>")

        empty = '<p class="empty">本组未定义消息。</p>' if not mod.messages else ""
        module_sections += f"""
        <section class="module" id="mod-{html.escape(anchor)}">
            <header class="module-head">
                <div>
                    <h2>{html.escape(mod.title)}</h2>
                    <p class="module-file">{link(mod.file, 1, Path(rel_file).as_posix(), "file-link")}</p>
                </div>
                <div class="module-stat">
                    {"".join(stat_pills)}
                </div>
            </header>
            {mod_issue_html}
            {type_sections}
            <div class="msg-grid">{msgs_html}</div>
            {empty}
        </section>
        """

    issue_section = render_issues_section(issues, link)
    hero_version_section = (
        render_hero_version(protocol_version, link) if protocol_version else ""
    )

    issue_stat_color = "var(--error)" if issues else "var(--accent2)"
    css = _load_asset("protocol_doc.css")
    js = _load_asset("protocol_doc.js")

    return f"""<!DOCTYPE html>
<html lang="zh-CN" data-theme="dark">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>协议说明文档</title>
{_HEAD_THEME_BOOT}
<style>
{css}
</style>
</head>
<body>
<div class="theme-switch" id="theme-switch" role="group" aria-label="主题切换">
    <span class="theme-slider" aria-hidden="true"></span>
    <button type="button" class="theme-btn" data-theme="light" id="theme-light" title="浅色主题">
        <svg class="theme-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41"/></svg>
        <span>明</span>
    </button>
    <button type="button" class="theme-btn is-active" data-theme="dark" id="theme-dark" title="深色主题">
        <svg class="theme-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/></svg>
        <span>暗</span>
    </button>
</div>
<div class="layout">
    <aside class="sidebar">
        <h1>协议说明</h1>
        <div class="toc-search-wrap">
            <input type="search" id="toc-search" class="toc-search"
                   placeholder="搜索 ID / 宏名 / 功能..." autocomplete="off" />
            <span id="toc-search-hint" class="toc-search-hint"></span>
        </div>
        <ul class="toc" id="toc">
            {toc_items}
        </ul>
    </aside>
    <main class="main">
        <div class="hero">
            <h2>ComHost 消息协议</h2>
            {hero_version_section}
            <p class="hero-meta">
                <span>目录: {html.escape(str(protocol_dir))}</span>
                <span>ID 定义: {html.escape(msg_id_def_path.name)}</span>
            </p>
            <div class="stats">
                <div class="stat-card"><div class="num">{len(modules)}</div><div class="lbl">组</div></div>
                <div class="stat-card"><div class="num">{total_msgs}</div><div class="lbl">消息</div></div>
                <div class="stat-card"><div class="num">{total_enums}</div><div class="lbl">枚举</div></div>
                <div class="stat-card"><div class="num">{total_structs}</div><div class="lbl">结构体</div></div>
                <div class="stat-card"><div class="num" style="color:{issue_stat_color}">{len(issues)}</div><div class="lbl">格式问题</div></div>
            </div>
        </div>
        {issue_section}
        {module_sections}
    </main>
</div>
<button type="button" id="back-top" title="回到顶部">↑</button>
<aside id="type-preview" class="type-preview" aria-hidden="true">
    <div class="type-preview-head">类型预览</div>
    <div class="type-preview-inner"></div>
</aside>
<script src="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/prism.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/prismjs@1.29.0/components/prism-c.min.js"></script>
<script>
{js}
</script>
</body>
</html>"""


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

def analyze(protocol_dir: Path) -> Tuple[List[ModuleDoc], List[FormatIssue], List[MessageDoc]]:
    protocol_dir = resolve_protocol_dir(protocol_dir)
    source_files = resolve_source_files(protocol_dir)
    msg_id_def_path = find_msg_id_def_path(protocol_dir)
    msg_id_defs = parse_msg_id_defs(msg_id_def_path)

    modules: List[ModuleDoc] = []
    all_issues: List[FormatIssue] = []
    all_messages: List[MessageDoc] = []

    for file_path in source_files:
        file_issues: List[FormatIssue] = []
        lines = read_lines(file_path)
        validate_file_structure(file_path, lines, file_issues)
        enum_span = section_span(lines, RE_SECTION_ENUM)
        struct_span = section_span(lines, RE_SECTION_STRUCT)
        enums = parse_private_types(file_path, lines, enum_span, "enum") if enum_span else []
        structs = parse_private_types(file_path, lines, struct_span, "struct") if struct_span else []
        msgs = find_message_blocks(file_path, lines, msg_id_defs, file_issues)
        title = extract_module_title(lines, file_path.name)
        modules.append(ModuleDoc(file_path, title, msgs, enums, structs))
        all_issues.extend(file_issues)
        all_messages.extend(msgs)

    return modules, all_issues, all_messages


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="扫描 protocol 目录，校验 cmd_data_def 头文件格式并生成 HTML 文档。"
    )
    parser.add_argument("protocol_dir", type=Path, help="protocol 目录路径")
    parser.add_argument("output", type=Path, help="HTML 文档输出路径")
    args = parser.parse_args(argv)

    protocol_dir = args.protocol_dir.resolve()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    try:
        protocol_dir = resolve_protocol_dir(protocol_dir)
        msg_id_def_path = find_msg_id_def_path(protocol_dir)
        id_ranges, macro_range = parse_id_range_sections(msg_id_def_path)
        protocol_def_path = find_protocol_def_path(protocol_dir)
        protocol_version = (
            parse_protocol_version(protocol_def_path)
            if protocol_def_path
            else None
        )
        modules, issues, messages = analyze(protocol_dir)
    except (FileNotFoundError, NotADirectoryError, ValueError) as exc:
        print(f"错误: {exc}", file=sys.stderr)
        return 2

    html_text = build_html(
        protocol_dir, output, modules, issues, messages,
        msg_id_def_path, id_ranges, macro_range, protocol_version,
    )
    output.write_text(html_text, encoding="utf-8")

    print(f"文档已生成: {output}")
    print(f"扫描组: {len(modules)} 个, 消息: {len(messages)} 条")
    if issues:
        print(f"格式校验未通过，共 {len(issues)} 项问题（详见文档）", file=sys.stderr)
        for issue in sorted(issues, key=lambda x: (str(x.file), x.line)):
            print(f"  {issue.file.name}:{issue.line} - {issue.reason}", file=sys.stderr)
        return 1
    print("格式校验通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
