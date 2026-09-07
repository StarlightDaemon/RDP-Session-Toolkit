// Copyright (c) 2026. MIT License.
//
// Trigger.cpp -- minimal server-side DVC trigger. PROOF OF MECHANISM.
//
// Run this INSIDE the RDP session (on the remote host). It opens the same
// Dynamic Virtual Channel the client plugin is listening on and writes one
// trivial payload. If the plumbing works, the client plugin's OnDataReceived
// fires and a line appears in dvc-probe.log on the CLIENT machine.
//
// This is deliberately the smallest thing that proves the send side. It does
// NOT reassemble, echo, or speak any protocol -- that comes later, once this
// mechanism is confirmed. Adapted from the OpenChannel/Write path of
// microsoft/rdp-dvc-plugin-samples -> Simple/cpp/rdp-simple-plugin-server.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wtsapi32.h>
#include <cstdio>

#include "../common/ProbeIds.h"

int main(int argc, char* argv[])
{
    int count = 3;               // send a few, so a single dropped frame is obvious
    for (int i = 1; i < argc; ++i)
    {
        if (lstrcmpiA(argv[i], "/once") == 0) count = 1;
        else if (lstrcmpiA(argv[i], "/help") == 0 || lstrcmpiA(argv[i], "/?") == 0)
        {
            printf("Usage: dvc-probe-trigger.exe [/once]\n");
            printf("  Run inside an RDP session. Opens DVC '%s' and sends %d test payload(s).\n",
                   PROBE_CHANNEL_NAME, count);
            return 0;
        }
    }

    printf("[trigger] pid=%lu session=%lu\n", GetCurrentProcessId(), WTSGetActiveConsoleSessionId());
    printf("[trigger] opening DVC '%s' (WTS_CHANNEL_OPTION_DYNAMIC)...\n", PROBE_CHANNEL_NAME);

    // NULL return means failure (NOT INVALID_HANDLE_VALUE).
    HANDLE h = WTSVirtualChannelOpenEx(
        WTS_CURRENT_SESSION,
        const_cast<LPSTR>(PROBE_CHANNEL_NAME),
        WTS_CHANNEL_OPTION_DYNAMIC);
    if (!h)
    {
        DWORD e = GetLastError();
        printf("[trigger] WTSVirtualChannelOpenEx FAILED, GetLastError=%lu\n", e);
        printf("[trigger] Common causes: not running inside an RDP session, or the\n");
        printf("          client plugin never activated / never called CreateListener.\n");
        return 1;
    }
    printf("[trigger] channel opened. Sending payload(s)...\n");

    int rc = 0;
    for (int i = 1; i <= count; ++i)
    {
        char payload[64];
        int n = snprintf(payload, sizeof(payload), "PROBE-PING #%d", i);

        DWORD written = 0;
        if (!WTSVirtualChannelWrite(h, payload, (ULONG)n, &written))
        {
            printf("[trigger] WTSVirtualChannelWrite #%d FAILED, GetLastError=%lu\n",
                   i, GetLastError());
            rc = 1;
            break;
        }
        printf("[trigger] sent #%d: \"%s\" (%lu bytes)\n", i, payload, written);
        if (i < count) Sleep(500);
    }

    // Give the last write time to drain before the channel tears down.
    Sleep(500);
    WTSVirtualChannelClose(h);
    printf("[trigger] channel closed. Done.\n");
    printf("[trigger] Now check dvc-probe.log next to the plugin EXE on the CLIENT.\n");
    return rc;
}
