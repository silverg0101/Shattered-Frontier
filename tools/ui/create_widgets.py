"""Create the shared and HUD Widget Blueprints (WBP_*) under /Game/Frontier/UI/Widgets/.

Run in the open editor:   py -3 tools/Run-InEditor.py tools/ui/create_widgets.py [--force]

Each WBP gets a first-draft layout and the BindWidget names its C++ base expects (Public/UI/SFUiWidgets.h, SFHudWidgets.h;
the layouts are in Private/UI/SFWidgetAuthoring.cpp). Existing assets are skipped: the owner rearranges them in the UMG
Designer, and --force rebuilds them from the draft and DISCARDS those edits. Needs the C++ module built (SFWidgetAuthoring).
"""
import sys

import unreal

force = '--force' in sys.argv
report = unreal.SFWidgetAuthoring.build_all(force)
print(report)
if 'FAIL' in report:
    raise RuntimeError('create_widgets: at least one widget blueprint failed (see log above)')
