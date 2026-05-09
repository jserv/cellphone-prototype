#!/usr/bin/env python3
"""ttf-to-vecfont -- convert TTF to LVGL vector font outline data.

Derived from mado twin-ttf (Keith Packard, MIT). Emits an
lv_font_vec_data_t with LV_FONT_VEC_TYPE_TTF.

    ttf-to-vecfont.py [--range 0xLO-0xHI] <font.ttf> > output.c

Requires libfreetype to be discoverable through ctypes.util.find_library.
"""

import argparse
import ctypes
import ctypes.util
import sys

# ---- libfreetype binding ----------------------------------------------------

_lib_path = ctypes.util.find_library("freetype")
if _lib_path is None:
    sys.exit("error: libfreetype not found")
ft = ctypes.CDLL(_lib_path)

FT_Pos = ctypes.c_long
FT_Long = ctypes.c_long
FT_ULong = ctypes.c_ulong
FT_Int = ctypes.c_int
FT_UInt = ctypes.c_uint
FT_Short = ctypes.c_short
FT_UShort = ctypes.c_ushort
FT_Fixed = ctypes.c_long
FT_Error = ctypes.c_int
FT_Encoding = ctypes.c_uint

FT_ENCODING_UNICODE = (ord("u") << 24) | (ord("n") << 16) | (ord("i") << 8) | ord("c")
FT_LOAD_NO_SCALE = 0x1
FT_LOAD_LINEAR_DESIGN = 0x2000


class FT_Vector(ctypes.Structure):
    _fields_ = [("x", FT_Pos), ("y", FT_Pos)]


class FT_BBox(ctypes.Structure):
    _fields_ = [("xMin", FT_Pos), ("yMin", FT_Pos),
                ("xMax", FT_Pos), ("yMax", FT_Pos)]


class FT_Generic(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("finalizer", ctypes.c_void_p)]


class FT_Glyph_Metrics(ctypes.Structure):
    _fields_ = [("width", FT_Pos), ("height", FT_Pos),
                ("horiBearingX", FT_Pos), ("horiBearingY", FT_Pos),
                ("horiAdvance", FT_Pos),
                ("vertBearingX", FT_Pos), ("vertBearingY", FT_Pos),
                ("vertAdvance", FT_Pos)]


class FT_Bitmap(ctypes.Structure):
    _fields_ = [("rows", ctypes.c_uint), ("width", ctypes.c_uint),
                ("pitch", ctypes.c_int),
                ("buffer", ctypes.POINTER(ctypes.c_ubyte)),
                ("num_grays", ctypes.c_ushort),
                ("pixel_mode", ctypes.c_ubyte),
                ("palette_mode", ctypes.c_ubyte),
                ("palette", ctypes.c_void_p)]


class FT_Outline(ctypes.Structure):
    _fields_ = [("n_contours", ctypes.c_short),
                ("n_points", ctypes.c_short),
                ("points", ctypes.POINTER(FT_Vector)),
                ("tags", ctypes.c_char_p),
                ("contours", ctypes.POINTER(ctypes.c_short)),
                ("flags", ctypes.c_int)]


class FT_GlyphSlotRec(ctypes.Structure):
    """Layout matches FreeType >= 2.10 up to and including .outline."""
    _fields_ = [
        ("library", ctypes.c_void_p),
        ("face", ctypes.c_void_p),
        ("next", ctypes.c_void_p),
        ("glyph_index", FT_UInt),
        ("generic", FT_Generic),
        ("metrics", FT_Glyph_Metrics),
        ("linearHoriAdvance", FT_Fixed),
        ("linearVertAdvance", FT_Fixed),
        ("advance", FT_Vector),
        ("format", ctypes.c_uint),
        ("bitmap", FT_Bitmap),
        ("bitmap_left", FT_Int),
        ("bitmap_top", FT_Int),
        ("outline", FT_Outline),
    ]


class FT_FaceRec(ctypes.Structure):
    """Layout matches FreeType up to and including .glyph."""
    _fields_ = [
        ("num_faces", FT_Long),
        ("face_index", FT_Long),
        ("face_flags", FT_Long),
        ("style_flags", FT_Long),
        ("num_glyphs", FT_Long),
        ("family_name", ctypes.c_char_p),
        ("style_name", ctypes.c_char_p),
        ("num_fixed_sizes", FT_Int),
        ("available_sizes", ctypes.c_void_p),
        ("num_charmaps", FT_Int),
        ("charmaps", ctypes.c_void_p),
        ("generic", FT_Generic),
        ("bbox", FT_BBox),
        ("units_per_EM", FT_UShort),
        ("ascender", FT_Short),
        ("descender", FT_Short),
        ("height", FT_Short),
        ("max_advance_width", FT_Short),
        ("max_advance_height", FT_Short),
        ("underline_position", FT_Short),
        ("underline_thickness", FT_Short),
        ("glyph", ctypes.POINTER(FT_GlyphSlotRec)),
    ]


MoveToFn = ctypes.CFUNCTYPE(
    ctypes.c_int, ctypes.POINTER(FT_Vector), ctypes.c_void_p)
LineToFn = ctypes.CFUNCTYPE(
    ctypes.c_int, ctypes.POINTER(FT_Vector), ctypes.c_void_p)
ConicToFn = ctypes.CFUNCTYPE(
    ctypes.c_int, ctypes.POINTER(FT_Vector), ctypes.POINTER(FT_Vector),
    ctypes.c_void_p)
CubicToFn = ctypes.CFUNCTYPE(
    ctypes.c_int, ctypes.POINTER(FT_Vector), ctypes.POINTER(FT_Vector),
    ctypes.POINTER(FT_Vector), ctypes.c_void_p)


class FT_Outline_Funcs(ctypes.Structure):
    _fields_ = [("move_to", MoveToFn), ("line_to", LineToFn),
                ("conic_to", ConicToFn), ("cubic_to", CubicToFn),
                ("shift", ctypes.c_int), ("delta", FT_Pos)]


_SIGS = (
    ("FT_Init_FreeType",
     [ctypes.POINTER(ctypes.c_void_p)], FT_Error),
    ("FT_Done_FreeType",
     [ctypes.c_void_p], FT_Error),
    ("FT_New_Face",
     [ctypes.c_void_p, ctypes.c_char_p, FT_Long,
      ctypes.POINTER(ctypes.POINTER(FT_FaceRec))], FT_Error),
    ("FT_Done_Face",
     [ctypes.POINTER(FT_FaceRec)], FT_Error),
    ("FT_Select_Charmap",
     [ctypes.POINTER(FT_FaceRec), FT_Encoding], FT_Error),
    ("FT_Get_First_Char",
     [ctypes.POINTER(FT_FaceRec), ctypes.POINTER(FT_UInt)], FT_ULong),
    ("FT_Get_Next_Char",
     [ctypes.POINTER(FT_FaceRec), FT_ULong, ctypes.POINTER(FT_UInt)],
     FT_ULong),
    ("FT_Get_Char_Index",
     [ctypes.POINTER(FT_FaceRec), FT_ULong], FT_UInt),
    ("FT_Load_Glyph",
     [ctypes.POINTER(FT_FaceRec), FT_UInt, ctypes.c_int], FT_Error),
    ("FT_Outline_Decompose",
     [ctypes.POINTER(FT_Outline), ctypes.POINTER(FT_Outline_Funcs),
      ctypes.c_void_p], FT_Error),
)
for _name, _args, _ret in _SIGS:
    _f = getattr(ft, _name)
    _f.argtypes = _args
    _f.restype = _ret


# ---- helpers ----------------------------------------------------------------

_IDENT = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_")
_DIGIT = set("0123456789")


def sanitize(name):
    """Map an arbitrary string to a C identifier (matches the C twin-ttf)."""
    out = []
    for i, c in enumerate(name):
        if c in _IDENT:
            out.append(c)
        elif c in _DIGIT:
            out.append(("_" + c) if i == 0 else c)
        else:
            out.append("_")
    return "".join(out)


def parse_range(text):
    parts = text.split("-")
    if len(parts) != 2:
        sys.exit(f"error: invalid range '{text}'")
    try:
        return int(parts[0], 0), int(parts[1], 0)
    except ValueError:
        sys.exit(f"error: invalid range '{text}'")


# ---- conversion -------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(
        description="Convert TTF to LVGL vector-font outline data.")
    p.add_argument("--range", default="0x0-0x10FFFF",
                   help="Unicode range, e.g. 0x20-0x7F")
    p.add_argument("font", help="Input TTF file")
    args = p.parse_args()
    lo, hi = parse_range(args.range)

    lib = ctypes.c_void_p()
    if ft.FT_Init_FreeType(ctypes.byref(lib)):
        sys.exit("error: FT_Init_FreeType failed")

    face_p = ctypes.POINTER(FT_FaceRec)()
    if ft.FT_New_Face(lib, args.font.encode("utf-8"), 0, ctypes.byref(face_p)):
        sys.exit(f"error: cannot open '{args.font}'")
    if ft.FT_Select_Charmap(face_p, FT_ENCODING_UNICODE):
        sys.exit("error: no Unicode charmap")

    face = face_p.contents
    units = face.units_per_EM
    if units == 0:
        sys.exit("error: face reports units_per_EM == 0")

    def quantize_q1_6(x):
        scaled = int(x) * 64
        if scaled >= 0:
            q = (scaled + units // 2) // units
        else:
            q = -(((-scaled) + units // 2) // units)

        if q < -128:
            state["clamped"] += 1
            return -128
        if q > 127:
            state["clamped"] += 1
            return 127
        return q

    def quantize_pair(vec):
        return quantize_q1_6(vec.x), quantize_q1_6(vec.y)

    out = sys.stdout.write
    state = {"offset": 1, "current": None, "clamped": 0}

    def emit_cmd(ch):
        out(f"\t'{ch}', ")
        state["offset"] += 1

    def emit_q(val):
        out(f"{val}, ")
        state["offset"] += 1

    def emit_pair(pt):
        emit_q(pt[0]); emit_q(pt[1])

    @MoveToFn
    def mv(to, _):
        qto = quantize_pair(to[0])
        emit_cmd("m"); emit_pair(qto); out("\n")
        state["current"] = qto
        return 0

    @LineToFn
    def ln(to, _):
        qto = quantize_pair(to[0])
        if qto == state["current"]:
            return 0
        emit_cmd("l"); emit_pair(qto); out("\n")
        state["current"] = qto
        return 0

    @ConicToFn
    def cn(ct, to, _):
        qct = quantize_pair(ct[0])
        qto = quantize_pair(to[0])
        if qct == state["current"] and qto == state["current"]:
            return 0
        emit_cmd("2")
        emit_pair(qct)
        emit_pair(qto)
        out("\n")
        state["current"] = qto
        return 0

    @CubicToFn
    def cb(c1, c2, to, _):
        qc1 = quantize_pair(c1[0])
        qc2 = quantize_pair(c2[0])
        qto = quantize_pair(to[0])
        if qc1 == state["current"] and qc2 == state["current"] and qto == state["current"]:
            return 0
        emit_cmd("c")
        emit_pair(qc1)
        emit_pair(qc2)
        emit_pair(qto)
        out("\n")
        state["current"] = qto
        return 0

    funcs = FT_Outline_Funcs(mv, ln, cn, cb, 0, 0)
    in_range = lambda u: lo <= u <= hi

    offs = [0] * (face.num_glyphs + 1)
    family = (face.family_name or b"font").decode("utf-8", "replace")
    sane = sanitize(family)

    out(f"/* Generated from {args.font} -- SIL OFL 1.1 */\n")
    out('#include "lv_font_vec.h"\n\n#if LV_USE_FONT_VEC\n\n')
    out("/* clang-format off */\nstatic const int8_t outlines[] = {\n")
    out("    0, /* index 0 = glyph not present */\n")

    gi = FT_UInt()
    u = ft.FT_Get_First_Char(face_p, ctypes.byref(gi))
    while gi.value and u < 0x1000000:
        load_flags = FT_LOAD_NO_SCALE | FT_LOAD_LINEAR_DESIGN
        if in_range(u) and ft.FT_Load_Glyph(face_p, gi.value, load_flags) == 0:
            offs[gi.value] = state["offset"]
            slot = face.glyph.contents
            state["current"] = None
            out(f"    /* U+{u:04X} */ ")
            advance = slot.metrics.horiAdvance or slot.linearHoriAdvance
            emit_q(quantize_q1_6(advance))
            out("\n")
            ft.FT_Outline_Decompose(
                ctypes.byref(slot.outline), ctypes.byref(funcs), None)
            emit_cmd("e"); out("\n")
        u = ft.FT_Get_Next_Char(face_p, u, ctypes.byref(gi))

    out("};\n/* clang-format on */\n\n")

    out("static const lv_font_vec_charmap_t charmap[] = {\n")
    n_charmap = 0
    gi = FT_UInt()
    u = ft.FT_Get_First_Char(face_p, ctypes.byref(gi))
    while gi.value and u < 0x1000000:
        if in_range(u):
            page = u & ~0x7F
            out(f"    {{0x{page >> 7:04x}, {{\n")
            for o in range(128):
                cp = page + o
                g = ft.FT_Get_Char_Index(face_p, cp) if in_range(cp) else 0
                if (o & 7) == 0:
                    out("\t")
                out(f"0x{offs[g]:08x}, ")
                if (o & 7) == 7:
                    out("\n")
            out("    }},\n")
            u = page + 127
            n_charmap += 1
        u = ft.FT_Get_Next_Char(face_p, u, ctypes.byref(gi))
    out("};\n\n")

    out(f"const lv_font_vec_data_t lv_font_vec_{sane}_data = {{\n")
    out(f"    .charmap = charmap,\n    .n_charmap = {n_charmap},\n")
    out("    .outlines = outlines,\n    .outlines_size = sizeof(outlines),\n")
    out("    .type = LV_FONT_VEC_TYPE_TTF,\n")
    out("    .ascender = "); emit_q(quantize_q1_6(face.ascender))
    out("\n    .descender = "); emit_q(quantize_q1_6(face.descender))
    out("\n    .height = "); emit_q(quantize_q1_6(face.height))
    out("\n};\n\n#endif\n")

    if state["clamped"]:
        print(f"warning: clamped {state['clamped']} Q1.6 coordinates while converting {args.font}",
              file=sys.stderr)

    ft.FT_Done_Face(face_p)
    ft.FT_Done_FreeType(lib)


if __name__ == "__main__":
    main()
