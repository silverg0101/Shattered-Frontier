// Compiles the generated ledger.proto stubs (tools/Build-GrpcThirdParty.ps1 -StubsOnly) inside the same fences.
#include "SFGrpcIncludes.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
#endif
THIRD_PARTY_INCLUDES_START
#pragma push_macro("check")
#pragma push_macro("verify")
#undef check
#undef verify
#include "sf/inventory/v1/ledger.pb.cc.inc"
#include "sf/inventory/v1/ledger.grpc.pb.cc.inc"
#pragma pop_macro("verify")
#pragma pop_macro("check")
THIRD_PARTY_INCLUDES_END
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif
