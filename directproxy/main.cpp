// amnezia-direct-proxy — a tiny local proxy whose upstream traffic is meant to
// bypass the VPN (the parent app excludes THIS process from the tunnel via the
// split-tunnel driver). Supports HTTP CONNECT and SOCKS5 (with remote DNS).
//
// Usage:
//   amnezia-direct-proxy --mode socks5|http --port 8899 [--host 127.0.0.1] [--log <file>]
//
// It resolves destination host names itself (remote DNS), so — being excluded
// from the VPN — both the DNS lookup and the connection go out the physical
// interface, giving the real geo. Optionally logs every destination host.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace {

enum class Mode { Http, Socks5 };

Mode g_mode = Mode::Socks5;
std::string g_host = "127.0.0.1";
int g_port = 8899;
std::string g_logPath;
unsigned long g_parentPid = 0; // if set, exit when this process dies (no orphans)

std::mutex g_logMutex;
std::set<std::string> g_seen;

std::string timestamp()
{
    std::time_t t = std::time(nullptr);
    std::tm tmv;
    localtime_s(&tmv, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

void logHost(const std::string &host, int port)
{
    if (g_logPath.empty()) {
        return;
    }
    const std::string key = host + ":" + std::to_string(port);
    bool isNew;
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        isNew = g_seen.insert(host).second;
    }
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream f(g_logPath, std::ios::app);
    if (f) {
        f << "[" << timestamp() << "] " << (isNew ? "NEW  " : "     ") << key << "\n";
    }
}

// Blocking send of the whole buffer.
bool sendAll(SOCKET s, const char *data, int len)
{
    int sent = 0;
    while (sent < len) {
        int n = send(s, data + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

// Connect to host:port, resolving the name locally (remote DNS from the client's
// point of view). Returns an open socket or INVALID_SOCKET.
SOCKET connectUpstream(const std::string &host, int port)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *res = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        return INVALID_SOCKET;
    }

    SOCKET up = INVALID_SOCKET;
    for (addrinfo *ai = res; ai; ai = ai->ai_next) {
        up = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (up == INVALID_SOCKET) {
            continue;
        }
        if (connect(up, ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0) {
            break;
        }
        closesocket(up);
        up = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    return up;
}

// Bidirectional relay until either side closes.
void pipeSockets(SOCKET a, SOCKET b)
{
    char buf[65536];
    for (;;) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(a, &fds);
        FD_SET(b, &fds);
        SOCKET maxfd = (a > b ? a : b);
        timeval tv{};
        tv.tv_sec = 120;
        int r = select(static_cast<int>(maxfd + 1), &fds, nullptr, nullptr, &tv);
        if (r <= 0) {
            break;
        }
        if (FD_ISSET(a, &fds)) {
            int n = recv(a, buf, sizeof(buf), 0);
            if (n <= 0 || !sendAll(b, buf, n)) {
                break;
            }
        }
        if (FD_ISSET(b, &fds)) {
            int n = recv(b, buf, sizeof(buf), 0);
            if (n <= 0 || !sendAll(a, buf, n)) {
                break;
            }
        }
    }
}

// Read exactly n bytes.
bool recvExact(SOCKET s, char *buf, int n)
{
    int got = 0;
    while (got < n) {
        int r = recv(s, buf + got, n - got, 0);
        if (r <= 0) {
            return false;
        }
        got += r;
    }
    return true;
}

void handleHttp(SOCKET client)
{
    // Read request headers up to CRLFCRLF.
    std::string head;
    char c;
    while (head.find("\r\n\r\n") == std::string::npos) {
        int n = recv(client, &c, 1, 0);
        if (n <= 0) {
            return;
        }
        head.push_back(c);
        if (head.size() > 65536) {
            return;
        }
    }

    const size_t sp1 = head.find(' ');
    const size_t sp2 = head.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) {
        return;
    }
    const std::string method = head.substr(0, sp1);
    const std::string target = head.substr(sp1 + 1, sp2 - sp1 - 1);

    if (method == "CONNECT") {
        std::string host = target;
        int port = 443;
        const size_t colon = target.rfind(':');
        if (colon != std::string::npos) {
            host = target.substr(0, colon);
            port = std::atoi(target.c_str() + colon + 1);
        }
        logHost(host, port);
        SOCKET up = connectUpstream(host, port);
        if (up == INVALID_SOCKET) {
            sendAll(client, "HTTP/1.1 502 Bad Gateway\r\n\r\n", 28);
            return;
        }
        const char *ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
        if (!sendAll(client, ok, static_cast<int>(std::strlen(ok)))) {
            closesocket(up);
            return;
        }
        pipeSockets(client, up);
        closesocket(up);
    } else {
        // Plain HTTP: host from absolute URI or Host header.
        std::string host;
        int port = 80;
        if (target.rfind("http://", 0) == 0) {
            const std::string rest = target.substr(7);
            const size_t slash = rest.find('/');
            std::string hostport = (slash == std::string::npos) ? rest : rest.substr(0, slash);
            const size_t colon = hostport.rfind(':');
            if (colon != std::string::npos) {
                host = hostport.substr(0, colon);
                port = std::atoi(hostport.c_str() + colon + 1);
            } else {
                host = hostport;
            }
        }
        if (host.empty()) {
            const std::string lower = head;
            size_t hp = std::string::npos;
            // case-insensitive search for "\nhost:"
            for (size_t i = 0; i + 5 < lower.size(); ++i) {
                if ((lower[i] == '\n') &&
                    (tolower(lower[i + 1]) == 'h') && (tolower(lower[i + 2]) == 'o') &&
                    (tolower(lower[i + 3]) == 's') && (tolower(lower[i + 4]) == 't') &&
                    (lower[i + 5] == ':')) {
                    hp = i + 6;
                    break;
                }
            }
            if (hp == std::string::npos) {
                return;
            }
            size_t end = head.find("\r\n", hp);
            std::string hv = head.substr(hp, end - hp);
            // trim
            size_t b = hv.find_first_not_of(" \t");
            size_t e = hv.find_last_not_of(" \t\r");
            if (b == std::string::npos) {
                return;
            }
            hv = hv.substr(b, e - b + 1);
            const size_t colon = hv.rfind(':');
            if (colon != std::string::npos) {
                host = hv.substr(0, colon);
                port = std::atoi(hv.c_str() + colon + 1);
            } else {
                host = hv;
            }
        }
        logHost(host, port);
        SOCKET up = connectUpstream(host, port);
        if (up == INVALID_SOCKET) {
            return;
        }
        // Forward the already-read request head, then relay the rest.
        if (sendAll(up, head.c_str(), static_cast<int>(head.size()))) {
            pipeSockets(client, up);
        }
        closesocket(up);
    }
}

void handleSocks5(SOCKET client)
{
    // Greeting: VER=5, NMETHODS, METHODS...
    unsigned char hdr[2];
    if (!recvExact(client, reinterpret_cast<char *>(hdr), 2) || hdr[0] != 0x05) {
        return;
    }
    const int nmethods = hdr[1];
    std::vector<char> methods(nmethods);
    if (nmethods > 0 && !recvExact(client, methods.data(), nmethods)) {
        return;
    }
    // Reply: no authentication required.
    const unsigned char noAuth[2] = {0x05, 0x00};
    if (!sendAll(client, reinterpret_cast<const char *>(noAuth), 2)) {
        return;
    }

    // Request: VER, CMD, RSV, ATYP, DST.ADDR, DST.PORT
    unsigned char req[4];
    if (!recvExact(client, reinterpret_cast<char *>(req), 4) || req[0] != 0x05) {
        return;
    }
    const unsigned char cmd = req[1];
    const unsigned char atyp = req[3];

    std::string host;
    if (atyp == 0x01) { // IPv4
        unsigned char a[4];
        if (!recvExact(client, reinterpret_cast<char *>(a), 4)) {
            return;
        }
        char tmp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, a, tmp, sizeof(tmp));
        host = tmp;
    } else if (atyp == 0x03) { // domain name (remote DNS)
        unsigned char len;
        if (!recvExact(client, reinterpret_cast<char *>(&len), 1)) {
            return;
        }
        std::vector<char> d(len);
        if (len > 0 && !recvExact(client, d.data(), len)) {
            return;
        }
        host.assign(d.data(), len);
    } else if (atyp == 0x04) { // IPv6
        unsigned char a[16];
        if (!recvExact(client, reinterpret_cast<char *>(a), 16)) {
            return;
        }
        char tmp[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, a, tmp, sizeof(tmp));
        host = tmp;
    } else {
        return;
    }

    unsigned char portb[2];
    if (!recvExact(client, reinterpret_cast<char *>(portb), 2)) {
        return;
    }
    const int port = (portb[0] << 8) | portb[1];

    auto reply = [&](unsigned char rep) {
        unsigned char r[10] = {0x05, rep, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        sendAll(client, reinterpret_cast<const char *>(r), 10);
    };

    if (cmd != 0x01) { // only CONNECT
        reply(0x07); // command not supported
        return;
    }

    logHost(host, port);
    SOCKET up = connectUpstream(host, port);
    if (up == INVALID_SOCKET) {
        reply(0x05); // connection refused
        return;
    }
    reply(0x00); // success
    pipeSockets(client, up);
    closesocket(up);
}

void handleClient(SOCKET client)
{
    if (g_mode == Mode::Http) {
        handleHttp(client);
    } else {
        handleSocks5(client);
    }
    closesocket(client);
}

void parseArgs(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--mode") {
            std::string m = next();
            g_mode = (m == "http") ? Mode::Http : Mode::Socks5;
        } else if (a == "--port") {
            g_port = std::atoi(next().c_str());
        } else if (a == "--host") {
            g_host = next();
        } else if (a == "--log") {
            g_logPath = next();
        } else if (a == "--parent-pid") {
            g_parentPid = std::strtoul(next().c_str(), nullptr, 10);
        }
    }
}

// Exit as soon as the parent (the client) process dies, so we never leave orphans.
void watchParent()
{
    if (g_parentPid == 0) {
        return;
    }
    std::thread([]() {
        HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, g_parentPid);
        if (h) {
            WaitForSingleObject(h, INFINITE);
            CloseHandle(h);
        }
        ExitProcess(0);
    }).detach();
}

} // namespace

int main(int argc, char **argv)
{
    parseArgs(argc, argv);
    watchParent();

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) {
        std::fprintf(stderr, "socket() failed\n");
        return 1;
    }
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(g_port));
    inet_pton(AF_INET, g_host.c_str(), &addr.sin_addr);

    if (bind(srv, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        std::fprintf(stderr, "bind() failed on %s:%d\n", g_host.c_str(), g_port);
        return 1;
    }
    if (listen(srv, 200) != 0) {
        std::fprintf(stderr, "listen() failed\n");
        return 1;
    }

    std::fprintf(stdout, "amnezia-direct-proxy %s on %s:%d\n",
                 g_mode == Mode::Http ? "http" : "socks5", g_host.c_str(), g_port);
    std::fflush(stdout);

    for (;;) {
        SOCKET client = accept(srv, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            continue;
        }
        std::thread(handleClient, client).detach();
    }
}
