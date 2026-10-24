"""Render MetaHuman preset faces / hair swaps to PNG via the full editor (needs RHI, not -nullrhi).

Run:  UnrealEditor.exe <uproject> -RenderOffscreen -unattended -ExecutePythonScript="<this file>"
Env:  MH_MODE=faces|hair   MH_OUT=<dir>   MH_ONLY=name1,name2 (faces)   MH_BASE=<preset name for hair mode>   MH_PRESETS=<asset folder>
Quits the editor when done. Nothing is saved; hair mode edits a scratch duplicate under /Game/Frontier/.
"""
import os, time, traceback
import unreal

MODE = os.environ.get("MH_MODE", "faces")
OUT = os.environ.get("MH_OUT", r"C:\Projects\SF-wt-ui\GameClient\ArtSource\Characters\Player\presets\raw")
ONLY = [s for s in os.environ.get("MH_ONLY", "").split(",") if s]
BASE = os.environ.get("MH_BASE", "Bruce")
SETTLE = int(os.environ.get("MH_SETTLE", "150"))  # ticks to let meshes/textures/grooms stream in
PRESETS = os.environ.get("MH_PRESETS", "/MetaHumanCharacter/Optional/Presets/")  # /Game/Frontier/Characters/Player/Faces/ + MH_ONLY=MHC_<id>,... renders built faces
HAIR = ["Hair_S_Messy", "Hair_M_Layered", "Hair_S_Casual", "Hair_S_SideSweptFringe", "Hair_S_RecedeMessy", "Hair_M_BobMessy"]
ALL = ["Ada", "Aera", "Aoi", "Asha", "Bo", "Bruce", "Cameron", "Celeste", "Dominic", "Etta", "Grace", "Isaiah", "Jelena",
       "Jorge", "Kelvin", "Lani", "Lorenzo", "Mateo", "Mikel", "Omari", "Orlando", "Sook-ja", "Sunita", "Trey", "Tuya",
       "Victor", "Vivian", "Walter", "Zuri"]
SCRATCH = "/Game/Frontier/Characters/Player/Scratch"
os.makedirs(OUT, exist_ok=True)
LOG = open(os.path.join(OUT, "_log.txt"), "a", buffering=1)


def log(*a):
    s = " ".join(str(x) for x in a)
    LOG.write(s + "\n")
    unreal.log("MHR " + s)


sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem) if hasattr(unreal, "LevelEditorSubsystem") else None
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world = unreal.EditorLevelLibrary.get_editor_world()
A = unreal.EditorLevelLibrary.spawn_actor_from_class


def make_backdrop_material():
    unreal.EditorAssetLibrary.make_directory(SCRATCH)
    path = SCRATCH + "/M_Backdrop"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Backdrop", SCRATCH, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    n = unreal.MaterialEditingLibrary.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
    n.set_editor_property("constant", unreal.LinearColor(0.30, 0.30, 0.32, 1))
    unreal.MaterialEditingLibrary.connect_material_property(n, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(m)
    return m


def set_up_stage():
    # character faces +Y here; camera sits at +Y, backdrop behind at -Y
    plane = A(unreal.StaticMeshActor, unreal.Vector(0, -150, 150), unreal.Rotator(0, 0, 0))
    plane.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))
    plane.set_actor_scale3d(unreal.Vector(12, 0.2, 8))
    plane.static_mesh_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    plane.static_mesh_component.set_material(0, make_backdrop_material())
    plane.static_mesh_component.set_cast_shadow(False)
    for pitch, yaw, inten in ((-25, -120, 3.0), (-10, -55, 2.2), (-60, -90, 0.8)):
        d = A(unreal.DirectionalLight, unreal.Vector(0, 0, 400), unreal.Rotator(0, pitch, yaw))
        c = d.get_component_by_class(unreal.DirectionalLightComponent)
        c.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        c.set_editor_property("intensity", inten)
        c.set_editor_property("light_source_angle", 6.0)
    sky = A(unreal.SkyLight, unreal.Vector(0, 0, 300), unreal.Rotator())
    sc = sky.get_component_by_class(unreal.SkyLightComponent)
    sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("intensity", 1.0)


rt = unreal.RenderingLibrary.create_render_target2d(world, 1024, 1280, unreal.TextureRenderTargetFormat.RTF_RGBA8)
cap_actor = A(unreal.SceneCapture2D, unreal.Vector(300, 0, 150), unreal.Rotator())
cap = cap_actor.capture_component2d
cap.texture_target = rt
cap.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
cap.set_editor_property("capture_every_frame", False)
cap.set_editor_property("capture_on_movement", False)
cap.fov_angle = 24.0
pp = cap.post_process_settings
pp.override_dynamic_global_illumination_method = True
pp.dynamic_global_illumination_method = unreal.DynamicGlobalIlluminationMethod.NONE
pp.override_auto_exposure_method = True
pp.auto_exposure_method = unreal.AutoExposureMethod.AEM_MANUAL
pp.override_auto_exposure_bias = True
pp.auto_exposure_bias = float(os.environ.get("MH_EV", "9.5"))
cap.post_process_settings = pp
cap.post_process_blend_weight = 1.0
set_up_stage()


def find_head(actor):
    """Return (head_pos, forward) from the face mesh's head bone; falls back to bounds."""
    for c in actor.get_components_by_class(unreal.SkeletalMeshComponent):
        nm = c.get_name()
        if "Face" in nm and c.skeletal_mesh is not None:
            for b in ("head", "FACIAL_C_FacialRoot", "headBase"):
                if c.get_bone_index(b) >= 0:
                    return c.get_socket_location(b), c, b
    return None, None, None


def shoot(actor, name, yaw):
    import math
    pos0, comp, bone = find_head(actor)
    if pos0 is None:
        origin, ext = actor.get_actor_bounds(False)
        pos0 = unreal.Vector(origin.x, origin.y, origin.z + ext.z - 14)
    fwd = actor.get_actor_forward_vector()
    log("head", name, pos0, "bone", bone, "fwd", fwd, "actor", actor.get_actor_location())
    tgt = unreal.Vector(pos0.x, pos0.y, pos0.z + float(os.environ.get("MH_HEADUP", "10")))
    base = math.atan2(fwd.y, fwd.x) + math.pi / 2  # face mesh looks along actor +Y
    r = base + math.radians(yaw) + (math.pi if os.environ.get("MH_FLIP") else 0)
    dist = float(os.environ.get("MH_DIST", "110"))
    pos = unreal.Vector(tgt.x + math.cos(r) * dist, tgt.y + math.sin(r) * dist, tgt.z + 2)
    rot = unreal.MathLibrary.find_look_at_rotation(pos, tgt)
    cap_actor.set_actor_location_and_rotation(pos, rot, False, False)
    cap.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, f"{name}.png")
    log("shot", name)


class Job:
    def __init__(self, name, build, shots):
        self.name, self.build, self.shots = name, build, shots


jobs = []
state = {"i": 0, "tick": 0, "actor": None, "phase": "start", "t0": time.time()}

if MODE == "faces":
    names = ONLY or ALL

    def mk(n):
        def build():
            ch = unreal.load_asset(PRESETS + n + "." + n)
            if not sub.try_add_object_to_edit(ch):
                raise RuntimeError("try_add_object_to_edit failed")
            sub.assemble_for_preview(character=ch)
            return ch, sub.spawn_meta_human_actor(character=ch)
        return build
    for n in names:
        jobs.append(Job(n, mk(n), [(n + "_front", 0)]))
else:
    # one fresh scratch copy of BASE per hair (re-selecting on an already-assembled character does not rebuild it)
    src = PRESETS + BASE + "." + BASE
    unreal.EditorAssetLibrary.make_directory(SCRATCH)
    base_ch = None

    def mk(h):
        def build():
            dst = f"{SCRATCH}/Base_{h}"
            ch = unreal.EditorAssetLibrary.duplicate_asset(src, dst)
            wi = unreal.load_asset(f"/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_{h}.WI_{h}")
            key = ch.internal_collection.try_add_item_from_wardrobe_item("Hair", wi)
            ch.internal_collection.default_instance.set_single_slot_selection(slot_name="Hair", item_key=key)
            if not sub.try_add_object_to_edit(ch):
                raise RuntimeError("try_add_object_to_edit failed")
            sub.assemble_for_preview(character=ch)
            return ch, sub.spawn_meta_human_actor(character=ch)
        return build
    for h in HAIR:
        jobs.append(Job(h, mk(h), [(h + "_front", 0), (h + "_34", 40)]))


def cleanup():
    a = state.get("actor")
    if a:
        try:
            a.destroy_actor()
        except Exception:
            pass
    ch = state.get("ch")
    if ch is not None:
        try:
            if sub.is_object_added_for_editing(ch):
                sub.remove_object_to_edit(ch)
        except Exception:
            pass
    state["actor"] = None
    state["ch"] = None


def finish():
    global handle
    log("finish called", state["i"], len(jobs))
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        if True:
            try:
                if sub.is_object_added_for_editing(base_ch):
                    sub.remove_object_to_edit(base_ch)
            except Exception:
                pass
            unreal.EditorAssetLibrary.delete_directory(SCRATCH)
    finally:
        log("done in", round(time.time() - state["t0"]), "s")
        unreal.SystemLibrary.quit_editor()


def tick(dt):
    if state.get("busy"):  # build() pumps the Slate loop, which re-enters this callback
        return
    state["busy"] = True
    try:
        _tick()
    finally:
        state["busy"] = False


def _tick():
    try:
        if state["i"] >= len(jobs):
            return finish()
        job = jobs[state["i"]]
        if state["phase"] == "start":
            log("build", job.name)
            global handle
            unreal.unregister_slate_post_tick_callback(handle)  # build() pumps Slate; re-entering Python crashes
            try:
                ch, actor = job.build()
            finally:
                handle = unreal.register_slate_post_tick_callback(tick)
            state["ch"], state["actor"] = ch, actor
            log("built", job.name, actor)
            state["tick"], state["phase"], state["t_built"] = 0, "settle", time.time()
        elif state["phase"] == "settle":
            state["tick"] += 1
            if state["tick"] >= SETTLE and time.time() - state["t_built"] >= float(os.environ.get("MH_WAIT", "0")):  # grooms stream in by wall time
                for nm, yaw in job.shots:
                    log("shooting", nm)
                    shoot(state["actor"], nm, yaw)
                cleanup()
                state["i"] += 1
                state["phase"] = "start"
    except Exception:
        log("ERROR", jobs[min(state["i"], len(jobs) - 1)].name, traceback.format_exc())
        try:
            cleanup()
        except Exception:
            pass
        state["i"] += 1
        state["phase"] = "start"


handle = unreal.register_slate_post_tick_callback(tick)
log("registered", MODE, len(jobs), "jobs")
