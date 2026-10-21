#pragma once

#include "Services/SFGameplayServicesClient.h"

// Pure wire conversion, called on the game thread before a Grant is queued.
bool SFSerializeGatherFollowUp(const FSFGameplayRequest& Receipt, TArray<uint8>& Payload, FString& Digest);
