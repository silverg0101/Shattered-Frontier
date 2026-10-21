#pragma once

// Client-only map-travel loading screen (MoviePlayer). Register() from the game module; it does nothing on dedicated
// servers, commandlets, the editor process, -NullRHI or other non-rendering runs. Look: HUD-v1 kit frames, Cinzel
// headings, splash background; values come from USFLoadingScreenSettings.
namespace SFLoadingScreen
{
SHATTEREDFRONTIER_API void Register();
SHATTEREDFRONTIER_API void Unregister();
}
