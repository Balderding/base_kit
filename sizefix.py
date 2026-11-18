"""Loads gg.py and gg2.py so that their ground layouts (written for a 30 mm base) follow the real base size.

The layout code is full of numbers that mean "far enough to run past the rim" (boxes from -20 to 20, rows of paving
out to 20 mm...). On a bigger base those must reach further, while the SIZE of every piece (a brick, a curb stone, a
plank) must stay the same. This loader rewrites, inside the layout functions only:
  - far-away coordinates (17 mm or more from the centre) in R(...), box(...), np.arange(...), rng.uniform(-a, a)
        -> multiplied by the size factor K
  - constant loop counts of pattern rows (range(a, b), or range(n) with n >= 13)    -> multiplied by K
  - the number of random seed points of a cell pattern (inside MultiPoint([...]))    -> multiplied by K * K
Nothing changes at 30 mm (K = 1)."""
import ast, os, sys, types, math

HERE = os.path.dirname(os.path.abspath(__file__))
SKIP = {"build", "art", "render", "sk", "sk_circle", "build_cmds", "build_cmds_trim_last", "loops", "_arc_loop", "piece_group", "_surface_z", "_wall_r",
        "_rim", "_clip", "op_k", "clip_z", "op_clip", "_prism_groups", "_prune", "_coincide", "_separate", "_group_ops", "_base_mm", "polys_of", "inset",
        "wide_enough", "frame", "edges_survive", "fit_for_taper", "stone", "star", "wiggle", "shade", "T_zr", "T_flat", "T_free", "T_group", "T_init",
        "T_add", "tprism", "tstack", "tcut_flat", "pickF", "mk", "line_cut", "crate", "drill_holes", "manhole", "crater", "crater_t"}


def _k(): return max(1.0, sys.modules["gg"].K)
def KX(v): return v * _k()
def KN(v): return int(math.ceil(v * _k())) if v >= 0 else -int(math.ceil(-v * _k()))
def KA(v): return int(round(v * _k() ** 2))


def _num(n):
    if isinstance(n, ast.Constant) and isinstance(n.value, (int, float)) and not isinstance(n.value, bool): return float(n.value)
    if isinstance(n, ast.UnaryOp) and isinstance(n.op, ast.USub) and isinstance(n.operand, ast.Constant) and isinstance(n.operand.value, (int, float)): return -float(n.operand.value)
    return None


def _wrap(fn, node): return ast.copy_location(ast.Call(func=ast.Name(id=fn, ctx=ast.Load()), args=[node], keywords=[]), node)


class Far(ast.NodeTransformer):
    """inside one argument expression: wrap constants that are 'far' (|v| >= lim)"""
    def __init__(s, lim): s.lim = lim
    def visit_Call(s, node): return node            # do not reach into nested calls (rng.uniform(0, 2) stays)
    def visit_UnaryOp(s, node):
        v = _num(node)
        return _wrap("_KX", node) if v is not None and abs(v) >= s.lim else s.generic_visit(node)
    def visit_Constant(s, node):
        v = _num(node)
        return _wrap("_KX", node) if v is not None and abs(v) >= s.lim else node


class Fix(ast.NodeTransformer):
    def __init__(s): s.depth = 0; s.in_mp = 0
    def visit_ClassDef(s, node): return node
    def visit_FunctionDef(s, node):
        if s.depth == 0 and node.name in SKIP: return node
        s.depth += 1; s.generic_visit(node); s.depth -= 1; return node
    def visit_Call(s, node):
        if s.depth == 0: return s.generic_visit(node)
        f = node.func; name = f.id if isinstance(f, ast.Name) else f.attr if isinstance(f, ast.Attribute) else ""
        if name == "MultiPoint": s.in_mp += 1; s.generic_visit(node); s.in_mp -= 1; return node
        s.generic_visit(node)
        if name in ("R", "box"): node.args = [Far(17).visit(a) for a in node.args]
        elif name == "arange": node.args = [Far(14).visit(a) for a in node.args[:2]] + node.args[2:]
        elif name == "uniform" and len(node.args) == 2:
            a, b = _num(node.args[0]), _num(node.args[1])
            if a is not None and b is not None and abs(a) >= 14 and abs(b) >= 14: node.args = [_wrap("_KX", x) for x in node.args]
        elif name == "range":
            if s.in_mp: node.args = [_wrap("_KA", a) for a in node.args[:1]] if len(node.args) == 1 else node.args
            else:
                vals = [_num(a) for a in node.args]
                # rows of a pattern: range(-7, 8), range(1, 14)... or a long single count. Short single counts (range(6): the
                # corners of a hexagon) are shapes, not rows, and stay as they are.
                if all(v is not None for v in vals) and ((len(vals) >= 2 and any(abs(v) >= 5 for v in vals)) or (len(vals) == 1 and vals[0] >= 13)):
                    node.args = [_wrap("_KN", a) for a in node.args]
        return node


def load(name):
    if name in sys.modules and getattr(sys.modules[name], "_sizefixed", False): return sys.modules[name]
    path = os.path.join(HERE, name + ".py")
    with open(path, encoding="utf-8") as fh: tree = ast.parse(fh.read(), path)
    tree = ast.fix_missing_locations(Fix().visit(tree))
    mod = types.ModuleType(name); mod.__file__ = path; mod._sizefixed = True
    mod.__dict__.update({"_KX": KX, "_KN": KN, "_KA": KA})
    sys.modules[name] = mod
    exec(compile(tree, path, "exec"), mod.__dict__)
    return mod
