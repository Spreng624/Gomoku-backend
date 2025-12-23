#ifndef SERVER_H
#define SERVER_H

#include "Frame.h"
#include "Packet.h"
#include "TimeWheel.hpp"
#include "Crypto.h"

#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <functional>
#include <unordered_map>
#include <chrono>

#ifdef _WIN32
// Windows 平台

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#undef byte
#pragma comment(lib, "ws2_32.lib")
using SOCKET_TYPE = SOCKET;

#else
// Linux 平台
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
using SOCKET_TYPE = int;

#endif

class Server
{
private:
    int port;                                // 服务器运行端口
    SOCKET_TYPE max_fd;                      // 维护最大的文件描述符，用于select选择
    SOCKET_TYPE listen_sock;                 // 监听 Socket
    std::vector<SOCKET_TYPE> client_sockets; // 维护客户端 Socket 列表
    std::map<SOCKET_TYPE, std::vector<uint8_t>> _session_buffers;

    // Session 合并后的数据与定时轮
    TimeWheel tw;
    std::unordered_map<int, uint64_t> sockToId;
    std::unordered_map<uint64_t, std::unique_ptr<SessionContext>> idToSession;

    // 回调函数：当接收到 Packet 时调用
    std::function<void(const Packet &)> onPacketCb;

    // 会话管理方法
    uint64_t GenerateSessionId();
    uint64_t NewSession(int sock);
    int HeartBeat(uint64_t sessionId);
    int CleanUp(uint64_t sessionId);
    int SendStatus(int sock, uint64_t sessionId, Frame::Status status, std::vector<uint8_t> data = {});
    int OnFrame(int sock, Frame frame); // 解析数据帧

    // 辅助函数
    bool InitializeNetworking();
    void CleanupNetworking();
    bool SetNonBlocking(SOCKET_TYPE);
    int HandleNewConnection(SOCKET_TYPE);
    int HandleClient(SOCKET_TYPE);
    SOCKET_TYPE CreateListenSocket();
    int Connect(SOCKET_TYPE sock);
    int DisConnect(SOCKET_TYPE sock);
    int Send(SOCKET_TYPE sock, Frame frame);

public:
    Server();
    ~Server();

    int Init();
    int Run();
    int Stop();

    // 注册回调：当接收到 Packet 时调用此回调
    void SetOnPacketCallback(std::function<void(const Packet &)> cb);
    int SendPacket(Packet packet); // Packet 序列化并发送
};

#endif