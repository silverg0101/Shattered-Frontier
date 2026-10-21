"""NAF's dilated neighborhood attention using ordinary PyTorch ROCm operations.

NAF nearest-exact upsamples keys/values by integer factors. Gather the same
shifted (not zero-padded) neighborhood from the original low-resolution grid,
avoiding the expanded high-resolution value tensor. Query chunks bound memory.
"""
import torch

def neighborhood_attention(q, k, v, kernel_size, scale, chunk_size=256):
    # q,k,v are BCHW; k/v share the low-resolution grid. NAF uses batch 1.
    batch, channels, hq, wq = q.shape
    _, heads, hk, wk, dk = k.shape
    assert batch == k.shape[0] == v.shape[0]
    assert hq % hk == 0 and wq % wk == 0
    kh, kw = kernel_size
    assert kh % 2 == kw % 2 == 1 and hk >= kh and wk >= kw
    dh, dw = hq // hk, wq // wk
    dq = channels // heads
    queries = q.reshape(batch,heads,dq,hq*wq).transpose(-1,-2)
    keys = k.reshape(batch,heads,hk*wk,dk)
    values = v.reshape(batch,heads,hk*wk,v.shape[-1])
    result = torch.empty((batch,heads,hq*wq,v.shape[-1]),device=q.device,dtype=v.dtype)
    offsets = (torch.arange(kh,device=q.device)[:,None]*wk + torch.arange(kw,device=q.device)[None,:]).flatten()
    for start in range(0,hq*wq,chunk_size):
        stop=min(start+chunk_size,hq*wq)
        index=torch.arange(start,stop,device=q.device)
        row=(index//wq//dh-kh//2).clamp(0,hk-kh)
        column=(index%wq//dw-kw//2).clamp(0,wk-kw)
        neighbors=(row*wk+column)[:,None]+offsets[None,:]
        kk=keys[:,:,neighbors,:]
        vv=values[:,:,neighbors,:]
        logits=torch.matmul(queries[:,:,start:stop,None,:],kk.transpose(-1,-2))*scale
        weights=logits.softmax(dim=-1)
        result[:,:,start:stop,:]=torch.matmul(weights,vv).squeeze(-2)
    return result.transpose(-1,-2).reshape(batch,heads*v.shape[-1],hq,wq)

def forward(self,q,k,v,image=None,return_weights=False,**kwargs):
    if return_weights: raise NotImplementedError('This inference adapter returns features only')
    batch,_,hk,wk=k.shape
    k=k.reshape(batch,self.num_heads,-1,hk,wk).permute(0,1,3,4,2).contiguous()
    v=v.reshape(batch,self.num_heads,-1,hk,wk).permute(0,1,3,4,2).contiguous()
    return neighborhood_attention(q,k,v,self.kernel_size,self.scale)
