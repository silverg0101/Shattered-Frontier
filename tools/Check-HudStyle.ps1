# Fails when a HUD screen builds its own tab/button/slot/row state art or selection colour instead of the SFHudStyle helpers
# (Tab/SetTabState, ButtonBrush/ApplyButtonStyle, SlotBrush/SetSlotSelected, RowBrush/SetRowSelected). docs/blueprints/ui-hud.md "Controls".
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $root 'GameClient\Source\ShatteredFrontier'
$bad = Get-ChildItem $src -Recurse -Include *.cpp,*.h | Where-Object { $_.Name -ne 'SFHudStyle.h' } | Select-String -Pattern '"(tab_normal|tab_selected|button_hover|button_pressed|button_disabled|button_primary_brass|button_destructive|slot_[a-z]+_selected)"|2B3B2C|E6D08A|button_normal"\)\s*:|SetStyle\(.*Brush' |
    Where-Object { $_.Line -notmatch 'plate art only' }
if ($bad) { $bad | ForEach-Object { "{0}:{1}: {2}" -f $_.Path.Replace("$root\", ''), $_.LineNumber, $_.Line.Trim() }; Write-Error 'HUD state art or selection colour built outside SFHudStyle.h'; exit 1 }
'HUD style check: ok'
