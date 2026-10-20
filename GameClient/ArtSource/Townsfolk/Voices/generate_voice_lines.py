"""Generate townsfolk voice lines from townsfolk-lines.json (AI Townsfolk doc, section 5.7).

    python generate_voice_lines.py --check     # any Python 3: validates the catalog, prints LINES_OK
    %LOCALAPPDATA%\\ShatteredFrontier\\tts-venv\\Scripts\\python generate_voice_lines.py
Environment: py -3.12 -m venv, then pip install chatterbox-tts kokoro soundfile, then torch/torchaudio 2.6.0 from
the cu124 index with --force-reinstall --no-deps (GPU), "numpy<2", and "setuptools<81" (the Perth watermarker
still imports pkg_resources).

Each line is rendered for every profile that shares one of its roles, once per combination of slot values. The
line's tone picks Chatterbox settings from 'emotions' (plus the profile's exaggeration offset) and the lip-sync
mood. Each profile clones a synthetic Kokoro reference clip. A clip is re-rendered only when its inputs change
(content hash). Clips that fail the length or clipping checks are retried once with another seed and then
reported. Writes Lines/<profile>/<clip>.wav, Lines/manifest.json and Lines/index.html.
"""
import hashlib, html, itertools, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'Lines'
WORDS_PER_S = 2.6  # typical speaking rate for the length check


def validate(C):
    problems = []
    roles = {r for p in C['profiles'] for r in p['roles']}
    for line in C['lines']:
        if line['tone'] not in C['emotions']:
            problems.append(f"{line['id']}: tone '{line['tone']}' has no emotion settings")
        names = set(re.findall(r'\{(\w+)\}', line['text']))
        slots = line.get('slots', {})
        for n in names - set(slots):
            problems.append(f"{line['id']}: slot {{{n}}} has no values")
        for n in names & set(slots):
            if not slots[n]:
                problems.append(f"{line['id']}: slot {{{n}}} has an empty value list")
        if re.search(r'\{(player|name|character)\w*\}', line['text'], re.I):
            problems.append(f"{line['id']}: lines may not name a player")
        if not set(line['roles']) & roles:
            problems.append(f"{line['id']}: no profile speaks roles {line['roles']}")
    ids = [l['id'] for l in C['lines']]
    if len(ids) != len(set(ids)):
        problems.append('duplicate line ids')
    return problems


def clips(C):
    """Every clip to render: (profile, line, slot values, text, clip name)."""
    for prof in C['profiles']:
        for line in C['lines']:
            if not set(line['roles']) & set(prof['roles']):
                continue
            names = re.findall(r'\{(\w+)\}', line['text'])
            for combo in itertools.product(*[line['slots'][n] for n in names]):
                text = line['text']
                for n, v in zip(names, combo):
                    text = text.replace('{' + n + '}', v)
                slug = '__'.join(re.sub(r'\W+', '_', v.lower()).strip('_') for v in combo)
                yield prof, line, dict(zip(names, combo)), text, line['id'] + (f'__{slug}' if slug else '')


def settings(C, prof, line):
    e = C['emotions'][line['tone']]
    return dict(exaggeration=round(min(2.0, max(0.25, e['exaggeration'] + prof.get('exaggeration_offset', 0))), 3),
                cfg_weight=e['cfg_weight'], mood=e['mood'], mood_intensity=e.get('mood_intensity', 1.0))


def check_clip(wav, sr, text):
    """Length within reason for the words spoken, and no clipping. Returns a list of problems."""
    import numpy as np
    dur = len(wav) / sr
    want = len(text.split()) / WORDS_PER_S
    out = []
    if not 0.4 * want <= dur <= 2.2 * want + 1.0:
        out.append(f'duration {dur:.1f}s for ~{want:.1f}s of words')
    if float(np.max(np.abs(wav))) >= 0.999:
        out.append('clipping')
    return out


def trim(wav, sr, floor=0.01, pad=0.08):
    import numpy as np
    loud = np.nonzero(np.abs(wav) > floor)[0]
    if not len(loud):
        return wav
    p = int(pad * sr)
    return wav[max(0, loud[0] - p):loud[-1] + p]


def render(C):
    import importlib.metadata as md
    import numpy as np, soundfile as sf, torch
    from kokoro import KPipeline
    from chatterbox.tts import ChatterboxTTS
    OUT.mkdir(exist_ok=True)
    old = OUT / 'manifest.json'
    done = {c['file']: c for c in json.loads(old.read_text(encoding='utf-8'))['clips']} if old.exists() else {}
    versions = dict(chatterbox=md.version('chatterbox-tts'), kokoro=md.version('kokoro'))
    kok = KPipeline(lang_code=C['references']['lang_code'], repo_id=C['references']['repo'])
    tts = ChatterboxTTS.from_pretrained(device='cuda' if torch.cuda.is_available() else 'cpu')
    refs, manifest, rendered, flagged = {}, [], 0, []
    for prof, line, slots, text, name in clips(C):
        voice = prof['reference_voice']
        if voice not in refs:
            refs[voice] = OUT / f'reference__{voice}.wav'
            if not refs[voice].exists():
                audio = np.concatenate([np.asarray(a) for _, _, a in kok(C['references']['text'], voice=voice)])
                sf.write(refs[voice], audio, 24000)
        s = settings(C, prof, line)
        key = json.dumps([text, voice, C['references']['text'], s['exaggeration'], s['cfg_weight'], versions], sort_keys=True)
        h = hashlib.sha1(key.encode()).hexdigest()[:12]
        rel = f"{prof['id']}/{name}.wav"
        path = OUT / rel
        if rel in done and done[rel]['hash'] == h and path.exists():
            manifest.append(done[rel])
            continue
        path.parent.mkdir(exist_ok=True)
        issues = []
        for attempt in range(2):
            torch.manual_seed(int(h, 16) + attempt)
            wav = tts.generate(text, audio_prompt_path=str(refs[voice]), exaggeration=s['exaggeration'], cfg_weight=s['cfg_weight'])
            wav = trim(wav.squeeze(0).cpu().numpy(), tts.sr)
            issues = check_clip(wav, tts.sr, text)
            if not issues:
                break
        sf.write(path, wav, tts.sr)
        rendered += 1
        if issues:
            flagged.append(f'{rel}: {issues}')
        manifest.append(dict(file=rel, hash=h, profile=prof['id'], line=line['id'], slots=slots, tone=line['tone'], text=text,
                             duration_s=round(len(wav) / tts.sr, 2), issues=issues, **s,
                             model=C['model']['repo'], model_version=versions['chatterbox'], licence=C['model']['licence'],
                             reference=f"{C['references']['repo']} {voice} (Apache-2.0), synthetic"))
        print('rendered', rel, issues or '')
    (OUT / 'manifest.json').write_text(json.dumps(dict(_about='Generated by generate_voice_lines.py; one entry per clip.',
                                                       clips=manifest), indent=1) + '\n', encoding='utf-8', newline='\n')
    rows = ''.join(f"<tr><td>{html.escape(c['profile'])}</td><td>{html.escape(c['tone'])} / {c['exaggeration']}</td>"
                   f"<td>{html.escape(c['text'])}</td><td><audio controls preload='none' src='{c['file']}'></audio></td>"
                   f"<td>{html.escape(', '.join(c['issues']))}</td></tr>" for c in manifest)
    descs = ''.join(f"<li><b>{p['id']}</b>: {html.escape(p['desc'])} (reference {p['reference_voice']})</li>" for p in C['profiles'])
    (OUT / 'index.html').write_text(
        "<!doctype html><meta charset='utf-8'><title>Townsfolk voice lines</title>"
        "<style>body{font:15px system-ui;margin:16px}td{padding:4px 8px;border-bottom:1px solid #ccc}</style>"
        f"<h1>Townsfolk voice lines</h1><ul>{descs}</ul><table><tr><th>Profile</th><th>Tone / exaggeration</th><th>Line</th>"
        f"<th>Listen</th><th>Issues</th></tr>{rows}</table>", encoding='utf-8', newline='\n')
    print(f'VOICE_LINES_OK {len(manifest)} clips, {rendered} rendered, {len(flagged)} flagged')
    for f in flagged:
        print('FLAG', f)


if __name__ == '__main__':
    C = json.loads((ROOT / 'townsfolk-lines.json').read_text(encoding='utf-8'))
    problems = validate(C)
    bad = json.loads(json.dumps(C)); bad['lines'][0]['tone'] = 'nope'; bad['lines'][1]['text'] += ' {player_name}'
    assert len(validate(bad)) >= 2, 'self-test: an unknown tone and a player-name slot must fail'
    for p in problems:
        print('FAIL', p)
    if problems:
        sys.exit(1)
    if '--check' in sys.argv:
        print(f'LINES_OK {sum(1 for _ in clips(C))} clips from {len(C["lines"])} lines x {len(C["profiles"])} profiles')
    else:
        render(C)
