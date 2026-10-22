# HUD vitals v6 source parts

Owner reference: `reference-owner.png`. These are fresh image-generation redraws, not crops. Source sprites are RGBA PNG at 2× their intended 1080p display size. Values and the level number are supplied as runtime text. `bar_fill_mask.png` is derived from the navy interior of `bar_frame.png`; use its white alpha to clip either rectangular fill into the pointed interior. Black/transparent pixels exclude the brass border. No Unreal import is included.

| Part | Pixels | Use |
| --- | ---: | --- |
| `badge_frame.png` | 128×168 | Empty hex badge and number plate |
| `badge_stag.png` | 80×88 | Center within upper badge |
| `bar_frame.png` | 600×44 | Shared health/stamina track |
| `bar_fill_health.png` | 512×32 | Horizontal red fill strip |
| `bar_fill_stamina.png` | 512×32 | Horizontal amber fill strip |
| `bar_fill_mask.png` | 600×44 | White permitted fill area, black elsewhere |
| `icon_heart.png` | 44×44 | Health icon |
| `icon_stamina_bolt.png` | 44×44 | Stamina icon |

Horizontal 9-slice margins for `bar_frame.png` at source resolution: **left 16, top 8, right 34, bottom 8 px**. Stretch the center horizontally; keep the pointed right cap intact. Position the fill in the inner track and apply the corresponding resized mask after the frame's 9-slice geometry. Do not stretch the fill's texture vertically.

## Generation prompts

Built-in imagegen was given `reference-owner.png` as a style/color reference for each generated part. Outputs were isolated from their generated canvases by the primary alpha component, resized with Lanczos to the sizes above, and placed on transparent RGBA canvases. The mask was scripted from the resized frame's connected navy interior. Prompts below are the exact prompts used; the second heart prompt was a style check, and the first result was retained because the second added a glow absent from the reference.

### badge_frame.png

> Use case: style-transfer. Asset type: standalone 2D game HUD sprite, badge_frame.png. Use the reference image strictly for brass/navy color, line weight, and heraldic visual style; REDRAW, do not crop or extract. Produce only the hexagonal level badge frame, front-on orthographic, symmetric, centered: thin beveled antique brass hexagon border, dark deep-navy interior, and integral small lower brass-outlined number plate protruding down. The number plate is EMPTY; badge interior is EMPTY. No stag, no icon, no text, no digits. Flat even lighting, crisp precise edges, no external shadow, no scene or background. Fully transparent RGBA outside the object, including all canvas corners. Target aspect approximately 128x168 pixels; leave a narrow transparent padding. A single clean isolated asset.

### badge_stag.png

> Use case: style-transfer. Asset type: standalone 2D game HUD sprite, badge_stag.png. Use reference solely for the emblem style and palette; REDRAW, never crop or extract. Produce only the pale ivory stylized stag head silhouette from the badge, elegant symmetrical branched antlers, narrow face, minimal dark detail, front-on. Clean icon matching the owner's restrained heraldic style, no enclosing badge or border, no text or numbers. Flat lighting, crisp edges, no outside shadow. Fully transparent RGBA everywhere outside the isolated stag, canvas corners alpha zero. Target about 80x88 px, suitable to overlay centered in 128x168 badge frame.

### bar_frame.png

> Use case: style-transfer. Asset type: standalone 2D game HUD sprite, bar_frame.png. REDRAW one empty horizontal vitals bar frame using reference only for style and brass/navy palette, never crop it. Very wide shallow silhouette, target 600x44 pixels: square-ish left end, long exactly straight parallel horizontal top/bottom lines, sharp pointed chevron right end. Thin antique brass border with restrained subtle bevel; interior is continuous very dark translucent navy track, empty. No red or amber fill, no heart or bolt, no text or numbers. Middle 70 percent must be visually uniform, seamlessly stretchable horizontally by 9-slice. Orthographic front-on, flat lighting, crisp edges, no external shadow. Isolated on fully transparent RGBA; every outside corner alpha zero. Entire bar framed within canvas with narrow transparent padding.

### bar_fill_health.png

> Use case: style-transfer. Asset type: standalone 2D game HUD sprite, bar_fill_health.png. REDRAW only a long rectangular HEALTH fill strip, reference supplies deep crimson red color and subtle horizontal texture/gradient, never crop/extract. Thin very wide flat strip with perfectly square ends, constant height, uniform seamless middle horizontally stretchable/tileable. No brass border, no pointed end, no icon, no letters, no numbers. Front-on orthographic and flat lighting, no outside shadow. Fully transparent RGBA outside isolated red strip; canvas corners alpha zero. Target eventual size roughly 512x32 px for compositing into 600x44 frame.

### bar_fill_stamina.png

> Use case: style-transfer. Asset type: standalone 2D game HUD sprite, bar_fill_stamina.png. REDRAW only a long rectangular STAMINA fill strip, reference supplies warm amber/gold color and subtle horizontal texture/gradient, never crop/extract. Thin very wide flat strip with perfectly square ends, constant height, uniform seamless middle horizontally stretchable/tileable. No brass border, no pointed end, no icon, no letters, no numbers. Front-on orthographic and flat lighting, no outside shadow. Fully transparent RGBA outside isolated amber strip; canvas corners alpha zero. Target eventual size roughly 512x32 px for compositing into 600x44 frame.

### icon_heart.png

> Use case: style-transfer. Asset type: standalone 2D game HUD sprite, icon_heart.png. REDRAW only the small red heart icon from the reference's style; use it for muted crimson hue and simple silhouette, never crop/extract. One centered symmetrical heart with subtly modeled red surface and crisp silhouette. No outline frame, no bar, no text, no number. Front-on, flat even lighting, no external shadow. Fully transparent RGBA outside the isolated heart, all corners alpha zero. Intended at 44x44 pixels (2x display).

Style check (not retained):

> Use case: style-transfer. Asset type: icon_heart.png, a tiny game HUD icon. REDRAW the heart from the owner's reference style, never crop. This is FLAT 2D vector-like UI art, not a 3D object: solid muted crimson silhouette, only a barely perceptible top-to-bottom value shift, no specular highlight, no bevel, no gloss, no outline, no external shadow. Exact simple heart shape seen in reference. One centered isolated heart on true transparent RGBA background. No text, no digits, no extra marks. Crisp clean antialiasing, front-on.

### icon_stamina_bolt.png

> Use case: style-transfer. Asset type: standalone 2D game HUD sprite, icon_stamina_bolt.png. REDRAW only the small amber golden lightning bolt icon from the reference style; use reference for angular silhouette and golden hue, never crop/extract. One centered simple angular bolt, nearly solid gold with subtle restrained surface variation, crisp edges. No outline frame, no bar, no text, no number. Front-on, flat even lighting, no external shadow. Fully transparent RGBA outside isolated bolt, all canvas corners alpha zero. Intended at 44x44 pixels (2x display).
