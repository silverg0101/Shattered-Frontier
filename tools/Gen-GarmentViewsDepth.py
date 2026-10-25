"""Body-conditioned garment views: one Qwen-Image-2.1 + Fun-ControlNet-Union depth generation per view on a RA-SIEMS ComfyUI.
Depth maps come from tools/Render-BodyDepthViews.py. Prompt rules are wrapped around the garment text (ghost mannequin, white
background, dark lining in openings). Saves <out>/<name>-{front,left,right}.png and prints seconds per view.
  py -3 tools/Gen-GarmentViewsDepth.py --slot head --name GEAR02-hood --prompt-file hood.txt --gpu 1 [--seed 12345 --cfg 3]
"""
import argparse, json, os, subprocess, sys, time, urllib.request, uuid

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SSH = ['ssh', '-i', os.path.expanduser('~/.ssh/id_ed25519_ra_siems'), '-o', 'IdentitiesOnly=yes', 'raadmin@192.168.0.107']
VIEWS = ('front', 'left', 'right')
WRAP = ("{garment} Ghost-mannequin product photo of ONE single garment on a plain pure white background, seen exactly {view}, "
        "orthographic, no person, no mannequin, no arms, no hands, no body parts, no other objects, no turnaround sheet. "
        "The garment follows the supplied depth map exactly: it is worn by an invisible body of that shape, so it hugs that "
        "silhouette and size. Every opening (neck, face, wrists, hem) stays open and visible, and through each opening you see "
        "only the dark inner lining of the garment, never the white background or skin.")
VIEW_TEXT = {'front': 'from the front', 'left': 'from its left side, in profile facing image-left', 'right': 'from its right side, in profile facing image-right'}
NEG = "person, skin, face, ears, nose, eyes, head, arms, sleeves, hands, torso, chest, tunic, shirt, body, mannequin, text, watermark, white fill inside openings, several garments, collage, floor shadow"


def call(base, path, data=None, ctype='application/json'):
    req = urllib.request.Request(base + path, data=data, headers={'Content-Type': ctype} if data else {})
    return json.load(urllib.request.urlopen(req, timeout=60))


def upload(base, png_path):
    b, name = uuid.uuid4().hex, f'depth_{uuid.uuid4().hex[:8]}.png'
    body = (f'--{b}\r\nContent-Disposition: form-data; name="image"; filename="{name}"\r\nContent-Type: image/png\r\n\r\n').encode() \
        + open(png_path, 'rb').read() + f'\r\n--{b}--\r\n'.encode()
    return call(base, '/upload/image', body, f'multipart/form-data; boundary={b}')['name']


def generate(base, depth_png, prompt, seed, cfg, steps, strength, workflow=None):
    g = json.load(open(workflow or os.path.join(ROOT, 'tools/comfy-workflows/qwen21-depth-garment.json')))
    g['5']['inputs']['image'] = upload(base, depth_png)
    g['6']['inputs'].update(prompt=prompt, negative_prompt=NEG)
    g['7']['inputs']['strength'] = strength
    g['9']['inputs'].update(seed=seed, cfg=cfg, steps=steps)
    pid = call(base, '/prompt', json.dumps({'prompt': g, 'client_id': 'garment'}).encode())['prompt_id']
    while True:
        h = call(base, f'/history/{pid}').get(pid)
        if h and h.get('status', {}).get('completed') is not None:
            if not h['status']['completed']:
                raise RuntimeError(json.dumps(h['status'])[:800])
            im = h['outputs']['11']['images'][0]
            q = f"/view?filename={im['filename']}&subfolder={im['subfolder']}&type={im['type']}"
            return urllib.request.urlopen(base + q, timeout=60).read()
        time.sleep(1)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--slot', default='head'); ap.add_argument('--name', required=True)
    ap.add_argument('--prompt', help='garment text'); ap.add_argument('--prompt-file')
    ap.add_argument('--depth-dir', default=os.path.join(ROOT, 'Artifacts/GEAR02/depth'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'Artifacts/GEAR02/views'))
    ap.add_argument('--gpu', type=int, default=1, choices=(1, 2)); ap.add_argument('--seed', type=int, default=12345)
    ap.add_argument('--cfg', type=float, default=3.0); ap.add_argument('--steps', type=int, default=25)
    ap.add_argument('--strength', type=float, default=1.0)
    a = ap.parse_args()
    garment = a.prompt or open(a.prompt_file, encoding='utf-8').read().strip()
    os.makedirs(a.out, exist_ok=True)
    port = 18187 + a.gpu
    tun = subprocess.Popen(SSH[:-1] + ['-N', '-L', f'{port}:127.0.0.1:{8187 + a.gpu}', SSH[-1]])
    try:
        time.sleep(3)
        base = f'http://127.0.0.1:{port}'
        for v in VIEWS:
            t = time.time()
            png = generate(base, os.path.join(a.depth_dir, f'depth_{a.slot}_{v}.png'),
                           WRAP.format(garment=garment, view=VIEW_TEXT[v]), a.seed, a.cfg, a.steps, a.strength)
            open(os.path.join(a.out, f'{a.name}-{v}.png'), 'wb').write(png)
            print(f'{v}: {time.time() - t:.1f}s', flush=True)
    finally:
        tun.terminate()
