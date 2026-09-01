// Copyright (c) 2026. MIT License.
//
// RelayIds.h
//
// Single source of truth for the identifiers the relay plugin registers under
// and listens on. Distinct from the probe's identifiers (see
// ../../probe/common/ProbeIds.h): the probe was a throwaway proof and was
// unregistered after its two-machine test passed; this is the PRODUCTION relay
// plugin with its own CLSID, AddIn name, and DVC channel.
//
// The DVC channel name here MUST match the one the host-side Windhawk mod opens
// (taskbar-integration/host, WTSVirtualChannelOpenEx). The target window class
// here MUST match the client-side mod's message-only relay window
// (taskbar-integration/client, RELAY_CLASS). See DECISIONS.md D-10.

#pragma once

// The production Dynamic Virtual Channel endpoint name. The relay plugin calls
// CreateListener(kChannelName); the host mod calls
// WTSVirtualChannelOpenEx(..., kChannelName, WTS_CHANNEL_OPTION_DYNAMIC).
// Distinct from the probe's "dvc::taskbar::probe"; same namespaced style.
#define RELAY_CHANNEL_NAME "dvc::taskbar::relay"

// COM CLSID of the relay plugin's COM class, in string form (used for registry).
// Fresh GUID minted for this component; distinct from the probe's CLSID.
#define RELAY_CLSID_STRING L"{6FC96481-9467-496E-BA33-A202ED052F39}"

// Unique name under ...\Terminal Server Client\Default\AddIns\<name>.
// Distinct from the probe's "HideRdpBarTaskbarDvcProbe".
#define RELAY_ADDIN_NAME L"RdpSessionToolkitDvcRelay"

// Window class of the client-side mod's message-only relay receiver. The relay
// plugin discovers it with FindWindowExW(HWND_MESSAGE, ...), exactly as the
// client mod's own local self-test path does, and forwards the DVC payload's
// first byte to it via WM_COPYDATA.
#define RELAY_TARGET_WINDOW_CLASS L"CitadelRdpTaskbarRelay"

// Diagnostic log file the plugin appends to, written next to the plugin EXE.
#define RELAY_LOG_FILENAME L"dvc-relay.log"
