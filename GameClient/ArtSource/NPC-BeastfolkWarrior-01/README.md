# BeastfolkWarrior art source

`PixalMV-v002/source/BeastfolkWarrior.glb` is the original WSL Pixal export. `PixalMV-v002/BeastfolkWarrior.glb` and `.blend` are the existing normalized 1.85 m source. See `PixalMV-v002/review.md` and `review-sheet.png` for the review and limitations. `AutoRig-v002/SK_BeastfolkWarrior.fbx` is the mannequin-compatible import candidate; the report and motion previews require visual signoff before gameplay use.

Editor slot import command (run only when the Unreal editor is available): `& "<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "GameClient\ShatteredFrontier.uproject" -run=pythonscript -nullrhi -unattended -nosplash '-script=tools/Import-NpcSkeletal.py --rig GameClient/ArtSource/NPC-BeastfolkWarrior-01/AutoRig-v002 --name BeastfolkWarrior --version V2'`
