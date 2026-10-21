"""Use PyTorch ROCm SDPA for the three sparse-attention calls in TRELLIS.2.

Only inference with zero dropout is supported. Sequence boundaries and the
default 1/sqrt(head_dim) attention scale match the FlashAttention API.
"""
import torch
import torch.nn.functional as F

def flash_attn_varlen_func(q,k,v,cu_seqlens_q,cu_seqlens_k,max_seqlen_q,max_seqlen_k,dropout_p=0.0,softmax_scale=None,causal=False,**kwargs):
    if dropout_p!=0 or kwargs:
        raise ValueError('Only TRELLIS inference attention is supported')
    qs=cu_seqlens_q.tolist(); ks=cu_seqlens_k.tolist(); results=[]
    for a,b,c,d in zip(qs[:-1],qs[1:],ks[:-1],ks[1:]):
        out=F.scaled_dot_product_attention(q[a:b].transpose(0,1).unsqueeze(0),k[c:d].transpose(0,1).unsqueeze(0),v[c:d].transpose(0,1).unsqueeze(0),dropout_p=0,is_causal=causal,scale=softmax_scale)
        results.append(out.squeeze(0).transpose(0,1))
    return torch.cat(results)

def flash_attn_varlen_qkvpacked_func(qkv,cu_seqlens,max_seqlen,**kwargs):
    q,k,v=qkv.unbind(1)
    return flash_attn_varlen_func(q,k,v,cu_seqlens,cu_seqlens,max_seqlen,max_seqlen,**kwargs)

def flash_attn_varlen_kvpacked_func(q,kv,cu_seqlens_q,cu_seqlens_k,max_seqlen_q,max_seqlen_k,**kwargs):
    k,v=kv.unbind(1)
    return flash_attn_varlen_func(q,k,v,cu_seqlens_q,cu_seqlens_k,max_seqlen_q,max_seqlen_k,**kwargs)

if __name__=='__main__':
    import json
    from torch.nn.attention import sdpa_kernel,SDPBackend
    torch.manual_seed(123)
    q=torch.randn(10,4,64,device='cuda',dtype=torch.float16)
    k=torch.randn(12,4,64,device='cuda',dtype=torch.float16)
    v=torch.randn_like(k)
    cq=torch.tensor([0,3,10],device='cuda',dtype=torch.int32)
    ck=torch.tensor([0,5,12],device='cuda',dtype=torch.int32)
    with sdpa_kernel(SDPBackend.FLASH_ATTENTION):
        actual=flash_attn_varlen_func(q,k,v,cq,ck,7,7)
    references=[]
    for a,b,c,d in [(0,3,0,5),(3,10,5,12)]:
        qq=q[a:b].float().transpose(0,1); kk=k[c:d].float().transpose(0,1); vv=v[c:d].float().transpose(0,1)
        references.append(((qq@kk.transpose(-1,-2)/8).softmax(-1)@vv).transpose(0,1))
    ref=torch.cat(references)
    error=(actual.float()-ref).abs().max().item()
    assert error<0.01,error
    print(json.dumps({'backend':'PyTorch ROCm SDPA forced FLASH_ATTENTION','max_abs_error_vs_fp32_math':error,'sequence_lengths_q':[3,7],'sequence_lengths_kv':[5,7]}))
