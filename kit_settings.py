"""Loads settings.json from the kit folder. Every path and limit lives there, nothing is hard-coded in the scripts."""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
_DEFAULTS = {
    "solidcraft_cli": "solidcraft-cli",
    "out_folder": os.path.join(HERE, "out"),
    "live": False,
    "step_limit_secs": 12,
    "step_limit_per_command_secs": 1.5,
    "base_limit_secs": 120,
    "batch_limit_mins": 45,
    "workers": "auto",
    "stop_if_fail_fraction": 0.25,
    "max_dropped_pieces": 4,
    "sketch_tolerance_mm": 0.03,
    "touching_tolerance_mm": 0.005,
    "true_circle_rim": True,
    "allow_supports": False,
}

def load():
    cfg = dict(_DEFAULTS)
    p = os.path.join(HERE, "settings.json")
    if os.path.exists(p):
        with open(p, encoding="utf-8") as f: cfg.update(json.load(f))
    if not os.path.isabs(cfg["out_folder"]): cfg["out_folder"] = os.path.join(HERE, cfg["out_folder"])
    os.makedirs(cfg["out_folder"], exist_ok=True)
    if str(cfg.get("workers", "auto")).strip().lower() in ("auto", "0", ""):          # "auto": a third of the cores, 2 to 12 (each worker builds one base at a time)
        cfg["workers"] = max(2, min(12, (os.cpu_count() or 6) // 3))
    else: cfg["workers"] = max(1, int(cfg["workers"]))
    if os.environ.get("KIT_SCENES") in ("0", "1"): cfg["scenes"] = os.environ["KIT_SCENES"] == "1"      # set by --scenes on|off
    if os.environ.get("KIT_OBJECTS"): cfg["objects_amount"] = float(os.environ["KIT_OBJECTS"])      # set by --objects
    if os.environ.get("KIT_SUPPORTS") in ("0", "1"): cfg["allow_supports"] = os.environ["KIT_SUPPORTS"] == "1"      # set by --supports on|off
    return cfg

CFG = load()

def figure_scale(size_mm, scale):
    """the scenery scale a base of this size really gets: a figure scale (32, 28 or 15 mm figures) only from
    "figure_scale_from_mm" (default 50) up. A smaller base keeps the classic look (scale 1), the whole small scene with
    its detail on top: at figure scale a 30 mm base showed only a bare part of a scene. size_mm: the diameter, or an
    oval's long side."""
    return 1.0 if float(size_mm) < float(CFG.get("figure_scale_from_mm", 50)) - 1e-6 else float(scale)
