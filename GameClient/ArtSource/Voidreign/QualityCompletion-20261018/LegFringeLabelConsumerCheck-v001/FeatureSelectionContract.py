"""Fail closed when a feature adapter cannot consume every semantic label."""
import numpy as np

def validate_feature_fields(data, feature_count, required_fields):
    if not isinstance(feature_count,int) or not 1<=feature_count<=128:
        raise ValueError('Declare a bounded feature profile before consuming selections')
    if 'labels' not in data:
        raise ValueError('Selection has no semantic labels')
    labels=data['labels']
    if labels.ndim!=1 or not np.issubdtype(labels.dtype,np.integer) or (labels<0).any():
        raise ValueError('Semantic labels must be a one-dimensional nonnegative integer array')
    unknown=set(np.unique(labels).tolist())-set(range(feature_count+1))
    if unknown:
        raise ValueError(f'Unsupported semantic labels {sorted(unknown)}; declare/rebuild their feature adapters, never silently leave them static')
    missing=set(required_fields)-set(data.files)
    if missing:
        raise ValueError(f'Missing derived feature fields {sorted(missing)}; label-only revisions require fresh roots/guides/weights, not copied caches')
