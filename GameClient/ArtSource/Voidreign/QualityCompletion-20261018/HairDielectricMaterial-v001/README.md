# Hair dielectric material prototype

Material attempt2/3, not an accepted whole mount. Source is the existing WalkPilot-v003 diagnostic. The original slot stays intact; a copied material sets Metallic0 only on124288 whole-selection Mane/Tail faces.2713mixed faces remain original. Albedo/roughness and image pixels retained; source/UV/geometry/weights/binds unchanged.

Replay the copied producer tool-source.py/profile.json in CPU background Blender into a new folder. Verify with tools/Build-MountSemanticMaterial.py using --verify-build <build folder>, the same profile and a new check folder; freeze the exact checker snapshot before replay. Existing candidate hash is bound by material-change.json.

Raw HairDielectricRetention-v001 correctly reports material/face-assignment differences. retention-summary.json scopes those expected changes and independently verified exact assignment; all other raw checks pass. Do not call the raw exact-material test a pass.

Head/saddle matched previews: HairDielectricReview-v001. Both sides/rear: HairDielectricBodyReview-v001. Current Blender GUI remains the old diagnostic walk to preserve unsaved state. No full animation replay, exchange or engine acceptance was performed for this material prototype.
