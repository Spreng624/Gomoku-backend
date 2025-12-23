#ifndef HANDLER_H
#define HANDLER_H

#include "Packet.h"
#include "EventBus.hpp"
#include <memory>
#include <functional>

class ObjectManager;

class Handler
{
private:
    ObjectManager &objMgr;
    EventBus<Event> &eventBus;
    std::function<void(const Packet &)> sendCallback;

    // 用户相关处理
    void OnLogin(const Packet &packet);
    void OnSignIn(const Packet &packet);
    void OnLoginByGuest(const Packet &packet);
    void OnGuest2User(const Packet &packet);
    void OnEditUsername(const Packet &packet);
    void OnEditPassword(const Packet &packet);
    void OnLogOut(const Packet &packet);

    // 房间相关处理
    void OnCreateRoom(const Packet &packet);
    void OnCreateSingleRoom(const Packet &packet);
    void OnJoinRoom(const Packet &packet);
    void OnExitRoom(const Packet &packet);
    void OnQuickMatch(const Packet &packet);

    // 座位操作处理
    void OnTakeBlack(const Packet &packet);
    void OnTakeWhite(const Packet &packet);
    void OnCancelTake(const Packet &packet);
    void OnStartGame(const Packet &packet);
    void OnEditRoomSetting(const Packet &packet);

    // 游戏相关处理
    void OnMakeMove(const Packet &packet);
    void OnBackMove(const Packet &packet);
    void OnDraw(const Packet &packet);
    void OnGiveUp(const Packet &packet);

    // 查询和订阅处理
    void OnGetUser(const Packet &packet);
    void OnSubscribeUserList(const Packet &packet);
    void OnSubscribeRoomList(const Packet &packet);

    // 辅助函数（暂时保留，后续改为事件发布）
    void SendResponse(const Packet &request, MsgType responseType, const MapType &params = {});
    void SendError(const Packet &request, const std::string &errMsg);

public:
    Handler(ObjectManager &objMgr, EventBus<Event> &eventBus, std::function<void(const Packet &)> sendCallback);
    ~Handler();

    // 公开方法：Server 通过回调调用此方法处理包
    void HandlePacket(const Packet &packet);
};

#endif
