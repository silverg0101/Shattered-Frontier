"""Export MetaHuman hair wardrobe thumbnails to PNG. Run: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended"""
import unreal
OUT = r"C:\Projects\SF-wt-ui\GameClient\ArtSource\Characters\Player\presets\raw"
for h in ["Hair_S_Messy", "Hair_M_Layered", "Hair_S_Casual", "Hair_S_SideSweptFringe", "Hair_S_RecedeMessy", "Hair_M_BobMessy"]:
    t = unreal.load_asset(f"/MetaHumanCharacter/Optional/Grooms/Thumbnails/Hair/{h}_Thumb.{h}_Thumb")
    task = unreal.AssetExportTask()
    task.object, task.filename, task.automated, task.prompt, task.replace_identical = t, f"{OUT}\{h}_thumb.png", True, False, True
    unreal.log(f"THUMB {h} {unreal.Exporter.run_asset_export_task(task)}")
