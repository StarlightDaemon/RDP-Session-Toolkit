// Copyright (c) 2026. MIT License.
//
// ProbeIds.h
//
// Single source of truth for the identifiers that the client plugin and the
// server trigger MUST agree on for the DVC to connect. If these drift apart,
// the channel silently never opens.
//
// This is the PROOF-OF-MECHANISM probe, not the real relay plugin. Its only
// job is to prove that mstsc actually activates an out-of-process AddIn and
// that a DVC signal actually arrives. See ../../TESTING.md.

#pragma once

// The Dynamic Virtual Channel endpoint name. The client plugin calls
// CreateListener(kChannelName); the server trigger calls
// WTSVirtualChannelOpenEx(..., kChannelName, WTS_CHANNEL_OPTION_DYNAMIC).
// The two strings MUST be byte-for-byte identical.
#define PROBE_CHANNEL_NAME "dvc::taskbar::probe"

// COM CLSID of the plugin's COM class, in string form (used for registry).
// Fresh GUID minted for this project; does NOT collide with Microsoft's sample.
#define PROBE_CLSID_STRING L"{3194520D-DE59-4432-95B5-D5CB4FAFC30E}"

// Unique name under ...\Terminal Server Client\Default\AddIns\<name>.
#define PROBE_ADDIN_NAME L"HideRdpBarTaskbarDvcProbe"

// Log file the plugin appends to, written next to the plugin EXE.
#define PROBE_LOG_FILENAME L"dvc-probe.log"
