import json
from pathlib import Path
from huggingface_hub import HfApi, snapshot_download

root=Path('/home/grego/sf-asset-lab')
api=HfApi()
requests=[
 ('microsoft/TRELLIS.2-4B',['pipeline.json','ckpts/*dec*','ckpts/*flow*']),
 ('microsoft/TRELLIS-image-large',['ckpts/ss_dec_conv3d_16l8_fp16.*']),
 ('PIA-SPACE-LAB/dinov3-vitl-pretrain-lvd1689m',['*.json','model.safetensors','README.md']),
]
manifest=[]
for repo,patterns in requests:
    info=api.model_info(repo)
    print('Downloading',repo,info.sha,flush=True)
    location=snapshot_download(repo,revision=info.sha,allow_patterns=patterns,max_workers=4)
    manifest.append(dict(repo=repo,revision=info.sha,path=location,patterns=patterns))
def model_hash(repo):
    return next(f.lfs.sha256 for f in api.model_info(repo,files_metadata=True).siblings if f.rfilename=='model.safetensors')
assert model_hash('facebook/dinov3-vitl16-pretrain-lvd1689m')==model_hash('PIA-SPACE-LAB/dinov3-vitl-pretrain-lvd1689m')
manifest[-1]['official_weight_sha256']=model_hash('facebook/dinov3-vitl16-pretrain-lvd1689m')
(root/'model-downloads.json').write_text(json.dumps(manifest,indent=2))
print('MODEL_DOWNLOADS_COMPLETE',flush=True)
