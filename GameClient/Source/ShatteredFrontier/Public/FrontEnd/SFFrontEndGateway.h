#pragma once

#include "FrontEnd/SFFrontEnd.h"

struct FSFIdentityHttpReply
{
    int32 Status = 0;
    FString Body;
};

// Injectable transport keeps the gateway protocol testable without a running HTTP service.
class SHATTEREDFRONTIER_API ISFIdentityHttp
{
public:
    virtual ~ISFIdentityHttp() = default;
    virtual void Send(const FString& Verb, const FString& Path, const FString& Body, const FString& Bearer,
        TFunction<void(FSFIdentityHttpReply)> Done, const FString& IdempotencyKey = {}) = 0;
};

SHATTEREDFRONTIER_API TSharedRef<ISFIdentityHttp> SFMakeIdentityHttp(const FString& BaseUrl);
SHATTEREDFRONTIER_API TSharedRef<ISFFrontEndIdentity> SFMakeGatewayIdentity(TSharedRef<ISFIdentityHttp> Http);
