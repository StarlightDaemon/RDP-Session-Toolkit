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

// Plugin version, logged at LocalServer startup. Bumped per change like the
// mods' own version lines. 0.2.0: the status-reply leg (host-bound replies,
// the un-forwarded status query, and the registry-published status-file
// location) — the plugin's first change since the 0.1.0 production port.
#define RELAY_VERSION_STRING "0.2.0"

// ---------------------------------------------------------------------------
// Command bytes (host -> relay). Mirrored copies live in the host mod
// (RELAY_CMD_*_BYTE) and the client mod (RelayCommand); DECISIONS.md D-10 is
// the single source of truth that keeps the copies in sync.
//   0x01 minimize          -> forwarded to RELAY_TARGET_WINDOW_CLASS
//   0x02 fullscreen toggle -> forwarded to RELAY_TARGET_WINDOW_CLASS
//   0x03 status query      -> NOT forwarded; answered by this plugin alone,
//                             so the host can poll with no side effect on the
//                             client. 0x00 stays permanently unassigned.
// ---------------------------------------------------------------------------
#define RELAY_CMD_MINIMIZE          0x01
#define RELAY_CMD_FULLSCREEN_TOGGLE 0x02
#define RELAY_CMD_QUERY_STATUS      0x03

// ---------------------------------------------------------------------------
// Client status snapshot (read-only mirror of the client mod's contract).
//
// The client mod's mstsc branch writes a LocalWidgetStatus record about once
// a second to <ClientModStoragePath>\local-widget-status.dat, and publishes
// that directory as a REG_SZ value so this plugin — which is not a Windhawk
// mod and cannot call Wh_GetModStoragePath — can find the file without
// guessing at a directory name. This is a MIRRORED COPY of the client mod's
// definitions (same D-10 discipline as the channel name): field layout,
// magic, and version must match the client mod's shared-contracts section
// exactly, and a version mismatch means "cannot read the state", never
// "guess".
// ---------------------------------------------------------------------------
#define RELAY_STATUS_REG_KEY   L"Software\\RDPSessionToolkit"
#define RELAY_STATUS_REG_VALUE L"ClientModStoragePath"
#define RELAY_STATUS_FILENAME  L"local-widget-status.dat"

#define RELAY_CLIENT_STATUS_MAGIC   0x53545352u  /* 'RSTS' */
#define RELAY_CLIENT_STATUS_VERSION 2u
// A snapshot whose writeTick is older than this (GetTickCount64, same
// machine) is stale — the client is gone or wedged — and must be reported as
// state-unknown, mirroring the explorer branch's own staleness rule.
#define RELAY_CLIENT_STATUS_STALE_MS 4000u

#pragma pack(push, 8)
// Mirror of the client mod's LocalWidgetStatus, version 2.
typedef struct RelayClientStatus {
    unsigned long      magic;
    unsigned long      version;
    unsigned long      writerPid;
    int                sessionActive;
    unsigned long      localIdleMs;
    int                quality;
    long               bandwidth;
    long               rtt;
    int                qualityAvailable;
    int                hung;
    int                hungSeconds;
    int                iconic;
    int                fullscreen;
    unsigned long long writeTick;
    wchar_t            hostname[64];
} RelayClientStatus;
#pragma pack(pop)

// Layout pin: the client mod asserts the same number on its LocalWidgetStatus,
// so a drifted mirror fails to build on whichever side changed.
static_assert(sizeof(RelayClientStatus) == 192,
              "RelayClientStatus no longer matches the client mod's "
              "LocalWidgetStatus v2 layout");

// ---------------------------------------------------------------------------
// Status reply (relay -> host). Written back on the still-open
// IWTSVirtualChannel after ANY received command, before the host closes the
// channel. Mirrored copy in the host mod (RelayStatusReply). Same
// magic+version wire-contract discipline as the client mod's structs.
// ---------------------------------------------------------------------------
#define RELAY_STATUS_REPLY_MAGIC   0x50525352u  /* 'RSRP' */
#define RELAY_STATUS_REPLY_VERSION 1u

#pragma pack(push, 1)
typedef struct RelayStatusReply {
    unsigned long magic;          // RELAY_STATUS_REPLY_MAGIC
    unsigned long version;        // RELAY_STATUS_REPLY_VERSION
    unsigned char stateKnown;     // 1 = fields below came from a fresh, valid
                                  //     snapshot; 0 = they are meaningless
    unsigned char sessionActive;
    unsigned char fullscreen;
    unsigned char reserved;
} RelayStatusReply;
#pragma pack(pop)
