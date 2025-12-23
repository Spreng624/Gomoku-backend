#include "Server.h"
#include "Logger.h"
#include <algorithm>
#include <ctime>

#define BUFFER_SIZE 4096
#define DEFAULT_PORT 8080
#define HEARTBEAT_INTERVAL_MS 30000 // 30 秒心跳间隔

// 定义跨平台的 错误码获取函数、I/O 阻塞错误码、关闭函数
#ifdef _WIN32
// Windows 平台

#define GET_LAST_ERROR() WSAGetLastError()
#define WOULD_BLOCK_ERROR WSAEWOULDBLOCK
#define CLOSE_SOCKET(s) closesocket(s)
#define SLEEP(ms) Sleep(ms)

#else
// Linux/POSIX 平台

#define GET_LAST_ERROR() errno
#define WOULD_BLOCK_ERROR EWOULDBLOCK
#define CLOSE_SOCKET(s) close(s)
#define SLEEP(ms) usleep((ms) * 1000)
#endif

// 实现跨平台的网络初始化、清理、设置非阻塞函数
#ifdef _WIN32
// --- Windows 实现 ---

bool Server::InitializeNetworking()
{
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

void Server::CleanupNetworking()
{
    WSACleanup();
}

bool Server::SetNonBlocking(SOCKET_TYPE sock)
{
    // FIONBIO 是在 Windows 上设置非阻塞模式的标准方法
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
}

#else
// --- Linux/POSIX 实现 ---

// Linux/POSIX 无需特殊初始化和清理
bool Server::InitializeNetworking() { return true; }
void Server::CleanupNetworking() {}

bool Server::SetNonBlocking(SOCKET_TYPE sock)
{
    // F_GETFL 获取标志，F_SETFL 设置标志，O_NONBLOCK 设置非阻塞
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1)
    {
        return false;
    }
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
}

#endif

// 构造时初始化 TimeWheel
Server::Server() : tw(120, std::chrono::seconds(1))
{
    port = DEFAULT_PORT;
    max_fd = 0;
    listen_sock = (SOCKET_TYPE)INVALID_SOCKET;
    client_sockets.clear();
    sockToId.clear();
    idToSession.clear();
    onPacketCb = nullptr;
}

Server::~Server()
{
    this->Stop();
}

void Server::SetOnPacketCallback(std::function<void(const Packet &)> cb)
{
    onPacketCb = cb;
}

SOCKET_TYPE Server::CreateListenSocket()
{
    SOCKET_TYPE listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in server_addr;

    if (listen_sock == -1)
    {
        LOG_ERROR("Error creating socket: " + std::to_string(GET_LAST_ERROR()));
        return (SOCKET_TYPE)INVALID_SOCKET;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(this->port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有接口

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        LOG_ERROR("Error binding socket: " + std::to_string(GET_LAST_ERROR()));
        CLOSE_SOCKET(listen_sock);
        return (SOCKET_TYPE)INVALID_SOCKET;
    }

    if (listen(listen_sock, 10) == -1)
    {
        LOG_ERROR("Error listening on socket: " + std::to_string(GET_LAST_ERROR()));
        CLOSE_SOCKET(listen_sock);
        return (SOCKET_TYPE)INVALID_SOCKET;
    }

    SetNonBlocking(listen_sock);
    return listen_sock;
}

int Server::HandleNewConnection(SOCKET_TYPE listen_sock)
{
    // 循环 accept 所有挂起的连接
    while (true)
    {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        SOCKET_TYPE new_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addrlen);

        if (new_sock == -1 || new_sock == (SOCKET_TYPE)INVALID_SOCKET)
        {
            int err = GET_LAST_ERROR();

            // 非预期的错误
            if (!(err == WOULD_BLOCK_ERROR || err == EWOULDBLOCK || err == EAGAIN))
                LOG_ERROR("Accept error: " + std::to_string(err));

            break;
        }
        Connect(new_sock);
    }

    return 0;
}

int Server::HandleClient(SOCKET_TYPE sock)
{
    auto &buffer = _session_buffers[sock];
    char temp_buf[BUFFER_SIZE];
    int n = recv(sock, temp_buf, sizeof(temp_buf), 0);

    if (n <= 0)
    {
        if (n < 0)
        {
            int err = GET_LAST_ERROR();
            LOG_ERROR("Error receiving data from client (Sock: " + std::to_string(sock) + "): " + std::to_string(err));
        };
        this->DisConnect(sock);
        return 0;
    }

    LOG_TRACE("Received " + std::to_string(n) + " bytes from client (Sock: " + std::to_string(sock) + "): ");
    buffer.insert(buffer.end(), temp_buf, temp_buf + n);

    Frame frame;
    while (frame.ReadStream(buffer))
    {
        LOG_TRACE("Received frame from client (Sock: " + std::to_string(sock) + "): ");
        this->OnFrame((int)sock, frame);
    }

    return 0;
}

int Server::Init()
{
    if (!InitializeNetworking())
    {
        LOG_ERROR("Failed to initialize networking.");
        return 1;
    }

    this->listen_sock = CreateListenSocket();

    if (this->listen_sock == (SOCKET_TYPE)INVALID_SOCKET)
    {
        CleanupNetworking();
        return 1;
    }

    this->client_sockets.push_back(this->listen_sock);
    max_fd = this->listen_sock;

    return 0;
}

int Server::Run()
{
    timeval timeout = {0, 0};

    LOG_INFO("Server Running on port " + std::to_string(this->port));

    while (true)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);

        for (SOCKET_TYPE sock : client_sockets)
        {
            FD_SET(sock, &read_fds);
        }

        int activity = select((int)max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            LOG_ERROR("Select error: " + std::to_string(GET_LAST_ERROR()));
            break;
        }

        if (FD_ISSET(this->listen_sock, &read_fds))
        {
            HandleNewConnection(this->listen_sock);
            activity--;
        }

        std::vector<SOCKET_TYPE> current_clients = client_sockets;
        for (SOCKET_TYPE sock : current_clients)
        {
            if (sock != this->listen_sock && FD_ISSET(sock, &read_fds))
            {
                HandleClient(sock);
                activity--;
            }
        }
        SLEEP(10);
    }

    this->Stop();
    return 0;
}

int Server::Stop()
{
    for (SOCKET_TYPE sock : client_sockets)
    {
        CLOSE_SOCKET(sock);
    }
    CleanupNetworking();
    return 0;
}

int Server::Connect(SOCKET_TYPE sock)
{
    SetNonBlocking(sock);
    this->client_sockets.push_back(sock);
    this->max_fd = std::max(this->max_fd, sock);
    LOG_INFO("New connection accepted (Sock: " + std::to_string(sock) + ")");
    LOG_DEBUG("Total connected clients: " + std::to_string(client_sockets.size()));
    return 0;
}

int Server::DisConnect(SOCKET_TYPE sock)
{
    _session_buffers.erase(sock);
    CLOSE_SOCKET(sock);
    auto it = std::find(this->client_sockets.begin(), this->client_sockets.end(), sock);
    if (it != this->client_sockets.end())
    {
        this->client_sockets.erase(it);
    }
    LOG_INFO("Connection closed (Sock: " + std::to_string(sock) + ")");
    LOG_DEBUG("Remaining connected clients: " + std::to_string(client_sockets.size()));
    return 0;
}

int Server::Send(SOCKET_TYPE sock, Frame frame)
{
    std::vector<uint8_t> buffer = frame.ToBytes();
    send(sock, reinterpret_cast<const char *>(buffer.data()), (int)buffer.size(), 0);
    return 0;
}

// --------------- 会话管理实现 -----------------

uint64_t Server::GenerateSessionId()
{
    std::vector<uint8_t> vec = GenerateRandomBytes(8);
    uint64_t sessionId = 0;
    for (auto byte : vec)
        sessionId = (sessionId << 8) | byte;
    return sessionId;
}

uint64_t Server::NewSession(int sock)
{
    uint64_t sessionId = GenerateSessionId();
    idToSession[sessionId] = std::make_unique<SessionContext>(sock, sessionId);
    sockToId[sock] = sessionId;

    // 添加超时清理任务（示例延迟槽 10）
    tw.AddTask(30, [this, sessionId]()
               {
                     auto it = idToSession.find(sessionId);
                     if (it == idToSession.end()) return;
                     auto *session = it->second.get();
                     if (GetTimeMS() - session->lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
                         CleanUp(sessionId);
                     } });
    return sessionId;
}

int Server::HeartBeat(uint64_t sessionId)
{
    auto it = idToSession.find(sessionId);
    if (it == idToSession.end())
        return -1;
    auto session = it->second.get();
    session->lastHeartbeat = GetTimeMS();
    return 0;
}

int Server::CleanUp(uint64_t sessionId)
{
    auto node = idToSession.extract(sessionId);
    if (node.empty())
        return -1;

    auto &session = node.mapped();
    // 关闭连接并通知上层
    DisConnect((SOCKET_TYPE)session->sock);
    sockToId.erase(session->sock);

    return 0;
}

int Server::SendStatus(int sock, uint64_t sessionId, Frame::Status status, std::vector<uint8_t> data)
{
    Frame frame(status, sessionId, {}, data);
    LOG_INFO("Sending status " + std::to_string(status) + " to client (Sock: " + std::to_string(sock) + ")");
    this->Send((SOCKET_TYPE)sock, frame);
    return 0;
}

int Server::OnFrame(int sock, Frame frame)
{
    auto it = sockToId.find(sock);
    uint64_t sessionId;
    Packet packet;

    if (it == sockToId.end())
        sessionId = NewSession(sock);
    else
        sessionId = it->second;

    auto p = idToSession[sessionId].get();

    switch (frame.head.status)
    {
    case Frame::Status::Hello:
        LOG_TRACE("Received Hello from client (Sock: " + std::to_string(sock) + ")");
        if (it == sockToId.end())
            SendStatus(sock, sessionId, Frame::Status::NewSession, p->Get_Pk_Sig());
        else
            SendStatus(sock, sessionId, Frame::Status::NewSession, p->Get_Pk_Sig());
        break;
    case Frame::Status::Pending:
        LOG_TRACE("Received Pending from client (Sock: " + std::to_string(sock) + ")");
        if (p->isActive)
        {
            SendStatus(sock, sessionId, Frame::Status::Activated);
            return 0;
        }
        p->pk2.assign(frame.data.begin(), frame.data.begin() + 32);
        if (!p->CalculateSharedKey())
            SendStatus(sock, sessionId, Frame::Status::Error);
        else
            SendStatus(sock, sessionId, Frame::Status::Activated);
        break;
    case Frame::Status::Active:
        LOG_TRACE("Received Active from client (Sock: " + std::to_string(sock) + ")");
        if (!p->isActive)
            SendStatus(sock, sessionId, Frame::Status::Inactive);
        else if (!p->Decrypt(frame.data))
            SendStatus(sock, sessionId, Frame::Status::Error);
        else if (!packet.FromData(sessionId, frame.data))
            SendStatus(sock, sessionId, Frame::Status::Error);
        else
        {
            HeartBeat(sessionId);
            // 通过回调通知上层
            if (onPacketCb)
                onPacketCb(packet);
        }
        break;
    default:
        LOG_WARN("Received Unknown frame from client (Sock: " + std::to_string(sock) + ")");
        SendStatus(sock, sessionId, Frame::Status::InvalidRequest);
        break;
    }
    return 0;
}

int Server::SendPacket(Packet packet)
{
    auto it = idToSession.find(packet.sessionId);
    if (it == idToSession.end())
        return -1;
    int sock = it->second->sock;
    std::array<uint8_t, 16> iv;
    std::vector<uint8_t> vec = GenerateRandomBytes(16);
    std::copy(vec.begin(), vec.end(), iv.begin());
    Frame frame(Frame::Status::Active, packet.sessionId, iv, packet.ToBytes());
    this->Send((SOCKET_TYPE)sock, frame);
    return 0;
}
