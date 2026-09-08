// amnezia-direct-proxy — a tiny local proxy. Supports HTTP CONNECT and SOCKS5
// (with remote DNS), optional login/password auth, a source-IP allowlist, and
// an optional TLS-encrypted client channel ("HTTPS proxy").
//
// Usage:
//   amnezia-direct-proxy --mode socks5|http --port 8899 [--host 127.0.0.1]
//                        [--user U --pass P] [--allow ip,ip] [--log <file>]
//                        [--tls --cert <pem> --key <pem> [--san <list>]]
//
// With --tls the listening socket speaks TLS (the client->proxy hop is encrypted);
// inside the tunnel it is the same HTTP CONNECT / SOCKS5 protocol. If the cert/key
// files do not exist they are generated as a self-signed pair.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

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

std::string g_user;            // optional basic-auth credentials
std::string g_pass;
bool g_authRequired = false;
std::set<std::string> g_allow; // allowed peer IPs; empty = allow any

bool g_tls = false;            // TLS-wrap the client connection
std::string g_certPath;
std::string g_keyPath;
std::string g_san = "DNS:localhost,IP:127.0.0.1";
SSL_CTX *g_sslCtx = nullptr;

std::mutex g_logMutex;
std::set<std::string> g_seen;

// A client connection: raw socket, or the same socket wrapped in TLS.
struct Conn {
    SOCKET s = INVALID_SOCKET;
    SSL *ssl = nullptr;
};

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

std::string base64Encode(const std::string &in)
{
    static const char *tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0;
    int bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(tbl[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) {
        out.push_back(tbl[((val << 8) >> (bits + 8)) & 0x3F]);
    }
    while (out.size() % 4) {
        out.push_back('=');
    }
    return out;
}

bool peerAllowed(const std::string &ip)
{
    if (g_allow.empty()) {
        return true;
    }
    return g_allow.count(ip) > 0;
}

// ---- raw socket helpers (upstream side) ----

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

// ---- client-side helpers (raw or TLS) ----

int cRecv(Conn &c, char *buf, int n)
{
    if (c.ssl) {
        return SSL_read(c.ssl, buf, n);
    }
    return recv(c.s, buf, n, 0);
}

bool cSendAll(Conn &c, const char *data, int len)
{
    int sent = 0;
    while (sent < len) {
        int n = c.ssl ? SSL_write(c.ssl, data + sent, len - sent)
                      : send(c.s, data + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

bool cRecvExact(Conn &c, char *buf, int n)
{
    int got = 0;
    while (got < n) {
        int r = cRecv(c, buf + got, n - got);
        if (r <= 0) {
            return false;
        }
        got += r;
    }
    return true;
}

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

// Bidirectional relay between the (possibly TLS) client and the raw upstream.
void pipeConn(Conn &c, SOCKET up)
{
    char buf[65536];
    for (;;) {
        // Drain any TLS record data already buffered before blocking in select().
        if (c.ssl && SSL_pending(c.ssl) > 0) {
            int n = SSL_read(c.ssl, buf, sizeof(buf));
            if (n <= 0 || !sendAll(up, buf, n)) {
                break;
            }
            continue;
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(c.s, &fds);
        FD_SET(up, &fds);
        SOCKET maxfd = (c.s > up ? c.s : up);
        timeval tv{};
        tv.tv_sec = 120;
        int r = select(static_cast<int>(maxfd + 1), &fds, nullptr, nullptr, &tv);
        if (r <= 0) {
            break;
        }
        if (FD_ISSET(c.s, &fds)) {
            int n = cRecv(c, buf, sizeof(buf));
            if (n <= 0 || !sendAll(up, buf, n)) {
                break;
            }
        }
        if (FD_ISSET(up, &fds)) {
            int n = recv(up, buf, sizeof(buf), 0);
            if (n <= 0 || !cSendAll(c, buf, n)) {
                break;
            }
        }
    }
}

// Checks Proxy-Authorization: Basic <base64(user:pass)> against the configured creds.
bool httpAuthOk(const std::string &head)
{
    if (!g_authRequired) {
        return true;
    }
    std::string lower = head;
    for (char &ch : lower) {
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    }
    const std::string key = "proxy-authorization:";
    size_t pos = lower.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    size_t vstart = pos + key.size();
    size_t vend = head.find("\r\n", vstart);
    std::string value = head.substr(vstart, vend - vstart);
    size_t b = value.find_first_not_of(" \t");
    if (b == std::string::npos) {
        return false;
    }
    value = value.substr(b);
    std::string schemeLower = value.substr(0, 6);
    for (char &ch : schemeLower) {
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    }
    if (schemeLower != "basic ") {
        return false;
    }
    return value.substr(6) == base64Encode(g_user + ":" + g_pass);
}

void handleHttp(Conn &c)
{
    std::string head;
    char ch;
    while (head.find("\r\n\r\n") == std::string::npos) {
        int n = cRecv(c, &ch, 1);
        if (n <= 0) {
            return;
        }
        head.push_back(ch);
        if (head.size() > 65536) {
            return;
        }
    }

    if (!httpAuthOk(head)) {
        const char *deny =
            "HTTP/1.1 407 Proxy Authentication Required\r\n"
            "Proxy-Authenticate: Basic realm=\"amnezia-proxy\"\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        cSendAll(c, deny, static_cast<int>(std::strlen(deny)));
        return;
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
            cSendAll(c, "HTTP/1.1 502 Bad Gateway\r\n\r\n", 28);
            return;
        }
        const char *ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
        if (!cSendAll(c, ok, static_cast<int>(std::strlen(ok)))) {
            closesocket(up);
            return;
        }
        pipeConn(c, up);
        closesocket(up);
    } else {
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
            size_t hp = std::string::npos;
            for (size_t i = 0; i + 5 < head.size(); ++i) {
                if ((head[i] == '\n') &&
                    (tolower(static_cast<unsigned char>(head[i + 1])) == 'h') &&
                    (tolower(static_cast<unsigned char>(head[i + 2])) == 'o') &&
                    (tolower(static_cast<unsigned char>(head[i + 3])) == 's') &&
                    (tolower(static_cast<unsigned char>(head[i + 4])) == 't') &&
                    (head[i + 5] == ':')) {
                    hp = i + 6;
                    break;
                }
            }
            if (hp == std::string::npos) {
                return;
            }
            size_t end = head.find("\r\n", hp);
            std::string hv = head.substr(hp, end - hp);
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
        if (sendAll(up, head.c_str(), static_cast<int>(head.size()))) {
            pipeConn(c, up);
        }
        closesocket(up);
    }
}

void handleSocks5(Conn &c)
{
    unsigned char hdr[2];
    if (!cRecvExact(c, reinterpret_cast<char *>(hdr), 2) || hdr[0] != 0x05) {
        return;
    }
    const int nmethods = hdr[1];
    std::vector<unsigned char> methods(nmethods);
    if (nmethods > 0 && !cRecvExact(c, reinterpret_cast<char *>(methods.data()), nmethods)) {
        return;
    }

    if (g_authRequired) {
        bool offersUserPass = false;
        for (unsigned char m : methods) {
            if (m == 0x02) {
                offersUserPass = true;
            }
        }
        if (!offersUserPass) {
            const unsigned char no[2] = {0x05, 0xFF};
            cSendAll(c, reinterpret_cast<const char *>(no), 2);
            return;
        }
        const unsigned char sel[2] = {0x05, 0x02};
        if (!cSendAll(c, reinterpret_cast<const char *>(sel), 2)) {
            return;
        }
        unsigned char ver;
        if (!cRecvExact(c, reinterpret_cast<char *>(&ver), 1) || ver != 0x01) {
            return;
        }
        unsigned char ulen;
        if (!cRecvExact(c, reinterpret_cast<char *>(&ulen), 1)) {
            return;
        }
        std::string uname(ulen, '\0');
        if (ulen > 0 && !cRecvExact(c, &uname[0], ulen)) {
            return;
        }
        unsigned char plen;
        if (!cRecvExact(c, reinterpret_cast<char *>(&plen), 1)) {
            return;
        }
        std::string passwd(plen, '\0');
        if (plen > 0 && !cRecvExact(c, &passwd[0], plen)) {
            return;
        }
        const bool ok = (uname == g_user && passwd == g_pass);
        const unsigned char resp[2] = {0x01, static_cast<unsigned char>(ok ? 0x00 : 0x01)};
        cSendAll(c, reinterpret_cast<const char *>(resp), 2);
        if (!ok) {
            return;
        }
    } else {
        const unsigned char noAuth[2] = {0x05, 0x00};
        if (!cSendAll(c, reinterpret_cast<const char *>(noAuth), 2)) {
            return;
        }
    }

    unsigned char req[4];
    if (!cRecvExact(c, reinterpret_cast<char *>(req), 4) || req[0] != 0x05) {
        return;
    }
    const unsigned char cmd = req[1];
    const unsigned char atyp = req[3];

    std::string host;
    if (atyp == 0x01) {
        unsigned char a[4];
        if (!cRecvExact(c, reinterpret_cast<char *>(a), 4)) {
            return;
        }
        char tmp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, a, tmp, sizeof(tmp));
        host = tmp;
    } else if (atyp == 0x03) {
        unsigned char len;
        if (!cRecvExact(c, reinterpret_cast<char *>(&len), 1)) {
            return;
        }
        std::vector<char> d(len);
        if (len > 0 && !cRecvExact(c, d.data(), len)) {
            return;
        }
        host.assign(d.data(), len);
    } else if (atyp == 0x04) {
        unsigned char a[16];
        if (!cRecvExact(c, reinterpret_cast<char *>(a), 16)) {
            return;
        }
        char tmp[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, a, tmp, sizeof(tmp));
        host = tmp;
    } else {
        return;
    }

    unsigned char portb[2];
    if (!cRecvExact(c, reinterpret_cast<char *>(portb), 2)) {
        return;
    }
    const int port = (portb[0] << 8) | portb[1];

    auto reply = [&](unsigned char rep) {
        unsigned char r[10] = {0x05, rep, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        cSendAll(c, reinterpret_cast<const char *>(r), 10);
    };

    if (cmd != 0x01) {
        reply(0x07);
        return;
    }

    logHost(host, port);
    SOCKET up = connectUpstream(host, port);
    if (up == INVALID_SOCKET) {
        reply(0x05);
        return;
    }
    reply(0x00);
    pipeConn(c, up);
    closesocket(up);
}

void handleClient(SOCKET s)
{
    Conn c;
    c.s = s;
    if (g_tls) {
        c.ssl = SSL_new(g_sslCtx);
        if (!c.ssl) {
            closesocket(s);
            return;
        }
        SSL_set_fd(c.ssl, static_cast<int>(s));
        if (SSL_accept(c.ssl) <= 0) {
            SSL_free(c.ssl);
            closesocket(s);
            return;
        }
    }

    if (g_mode == Mode::Http) {
        handleHttp(c);
    } else {
        handleSocks5(c);
    }

    if (c.ssl) {
        SSL_shutdown(c.ssl);
        SSL_free(c.ssl);
    }
    closesocket(s);
}

// ---- TLS: self-signed cert generation + context ----

bool fileExists(const std::string &p)
{
    std::ifstream f(p);
    return f.good();
}

bool generateSelfSigned(const std::string &certPath, const std::string &keyPath, const std::string &san)
{
    EVP_PKEY *pkey = EVP_RSA_gen(2048);
    if (!pkey) {
        return false;
    }
    X509 *x = X509_new();
    if (!x) {
        EVP_PKEY_free(pkey);
        return false;
    }
    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), static_cast<long>(std::time(nullptr)));
    X509_gmtime_adj(X509_get_notBefore(x), 0);
    X509_gmtime_adj(X509_get_notAfter(x), 60L * 60L * 24L * 3650L); // 10 years
    X509_set_pubkey(x, pkey);

    X509_NAME *name = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>("amnezia-proxy"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>("AmneziaVPN reconnect fork"), -1, -1, 0);
    X509_set_issuer_name(x, name);

    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, x, x, nullptr, nullptr, 0);
    if (X509_EXTENSION *ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name,
                                                   const_cast<char *>(san.c_str()))) {
        X509_add_ext(x, ext, -1);
        X509_EXTENSION_free(ext);
    }
    if (X509_EXTENSION *ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_basic_constraints,
                                                   const_cast<char *>("CA:FALSE"))) {
        X509_add_ext(x, ext, -1);
        X509_EXTENSION_free(ext);
    }
    if (X509_EXTENSION *ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_ext_key_usage,
                                                   const_cast<char *>("serverAuth"))) {
        X509_add_ext(x, ext, -1);
        X509_EXTENSION_free(ext);
    }

    bool ok = X509_sign(x, pkey, EVP_sha256()) > 0;
    if (ok) {
        // Write via BIO, not FILE*: passing the app's FILE* into the OpenSSL DLL requires
        // OPENSSL_Applink and otherwise aborts with "no OPENSSL_Applink".
        BIO *bc = BIO_new_file(certPath.c_str(), "wb");
        BIO *bk = BIO_new_file(keyPath.c_str(), "wb");
        ok = bc && bk
             && PEM_write_bio_X509(bc, x) > 0
             && PEM_write_bio_PrivateKey(bk, pkey, nullptr, nullptr, 0, nullptr, nullptr) > 0;
        if (bc) {
            BIO_free(bc);
        }
        if (bk) {
            BIO_free(bk);
        }
    }
    X509_free(x);
    EVP_PKEY_free(pkey);
    return ok;
}

bool initTls()
{
    if (g_certPath.empty() || g_keyPath.empty()) {
        std::fprintf(stderr, "--tls requires --cert and --key\n");
        return false;
    }
    if (!fileExists(g_certPath) || !fileExists(g_keyPath)) {
        if (!generateSelfSigned(g_certPath, g_keyPath, g_san)) {
            std::fprintf(stderr, "failed to generate self-signed certificate\n");
            return false;
        }
    }
    g_sslCtx = SSL_CTX_new(TLS_server_method());
    if (!g_sslCtx) {
        return false;
    }
    SSL_CTX_set_min_proto_version(g_sslCtx, TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_file(g_sslCtx, g_certPath.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(g_sslCtx, g_keyPath.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        !SSL_CTX_check_private_key(g_sslCtx)) {
        std::fprintf(stderr, "failed to load TLS certificate/key\n");
        return false;
    }
    return true;
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
        } else if (a == "--user") {
            g_user = next();
        } else if (a == "--pass") {
            g_pass = next();
        } else if (a == "--allow") {
            std::string list = next();
            size_t start = 0;
            while (start <= list.size()) {
                size_t comma = list.find(',', start);
                std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                size_t b = item.find_first_not_of(" \t");
                size_t e = item.find_last_not_of(" \t");
                if (b != std::string::npos) {
                    g_allow.insert(item.substr(b, e - b + 1));
                }
                if (comma == std::string::npos) {
                    break;
                }
                start = comma + 1;
            }
        } else if (a == "--tls") {
            g_tls = true;
        } else if (a == "--cert") {
            g_certPath = next();
        } else if (a == "--key") {
            g_keyPath = next();
        } else if (a == "--san") {
            g_san = next();
        }
    }
    g_authRequired = (!g_user.empty());
}

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

    if (g_tls && !initTls()) {
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

    std::fprintf(stdout, "amnezia-direct-proxy %s%s on %s:%d\n",
                 g_tls ? "tls+" : "", g_mode == Mode::Http ? "http" : "socks5", g_host.c_str(), g_port);
    std::fflush(stdout);

    for (;;) {
        sockaddr_in peer{};
        int plen = sizeof(peer);
        SOCKET client = accept(srv, reinterpret_cast<sockaddr *>(&peer), &plen);
        if (client == INVALID_SOCKET) {
            continue;
        }
        char ipbuf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
        if (!peerAllowed(ipbuf)) {
            closesocket(client);
            continue;
        }
        std::thread(handleClient, client).detach();
    }
}
