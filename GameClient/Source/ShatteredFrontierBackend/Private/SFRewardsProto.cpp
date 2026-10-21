// Compiles the generated rewards.proto stubs inside the same fences; one translation unit per proto (file-static names collide).
#include "SFGrpcGameplayIncludes.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
#endif
THIRD_PARTY_INCLUDES_START
#pragma push_macro("check")
#pragma push_macro("verify")
#undef check
#undef verify
#include "sf/rewards/v1/rewards.pb.cc.inc"
#include "sf/rewards/v1/rewards.grpc.pb.cc.inc"
#pragma pop_macro("verify")
#pragma pop_macro("check")
THIRD_PARTY_INCLUDES_END
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif
