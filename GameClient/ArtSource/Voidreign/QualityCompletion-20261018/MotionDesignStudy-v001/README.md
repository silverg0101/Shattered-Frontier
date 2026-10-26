# Motion design study — finite single-variable comparisons

These are trajectory diagnostics, not a fourth delivered model bake. Historical topology/rig/secondary-motion attempts stay3. Frozen solver,24guides, colliders, timing and limits are identical. User renewed authorization to continue hair fixes.

| Case | Peak guide step | Node warnings / frames | Settled RMS style deviation | Turn max deviation | Stop max deviation |
| --- | --- | --- | --- | --- | --- |
| Baseline | 155.0mm | 118 / 33 | 87.0mm | 748.5mm | 325.5mm |
| Drag12 | 95.6mm | 52 / 27 | 87.0mm | 781.7mm | 533.3mm |
| Spring30 | 94.2mm | 65 / 13 | 171.8mm | 790.3mm | 698.6mm |

No combined profile or delivered rig. Both peak steps still exceed80mm; higher damping increases turn/stop style lag, weaker spring nearly doubles settled deviation. Prioritize reviewed guide/skin coverage and coupled contact/velocity fixtures.

All settle/startup/root/segment/required-event checks remain bounded, but no speed-only selection is accepted. See study-results.json for fixed-envelope conflict, free-guide residuals and all exact report paths. Guide envelope clearance does not establish skin clearance.

Reproduce each case with the frozen Simulation-v003/tool-source.py and guides.json, its <Case>-profile.json and a fresh output directory. Then run Check-FeatureTrajectory.py and Check-FeatureGuideClearance.py with the existing pinned profiles. The original study-plan.json remains the immutable prepared plan; study-results.json records execution and decision.
