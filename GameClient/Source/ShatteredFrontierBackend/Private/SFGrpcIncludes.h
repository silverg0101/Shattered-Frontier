#pragma once

// The one place gRPC/protobuf headers enter Unreal code: Windows types and UE's check/verify macros
// are fenced so neither side sees the other's definitions.
#include "CoreMinimal.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
// UE's Windows wrapper drops winnt.h's MemoryBarrier macro, which gRPC's atm_windows.h expects.
#if !defined(MemoryBarrier)
#include <intrin.h>
#define MemoryBarrier __faststorefence
#define SF_GRPC_DEFINED_MEMORY_BARRIER
#endif
#endif
THIRD_PARTY_INCLUDES_START
#pragma push_macro("check")
#pragma push_macro("verify")
#undef check
#undef verify
#include <grpcpp/grpcpp.h>
#include "sf/inventory/v1/ledger.grpc.pb.h"
#pragma pop_macro("verify")
#pragma pop_macro("check")
THIRD_PARTY_INCLUDES_END
#if PLATFORM_WINDOWS
#ifdef SF_GRPC_DEFINED_MEMORY_BARRIER
#undef MemoryBarrier
#undef SF_GRPC_DEFINED_MEMORY_BARRIER
#endif
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif
