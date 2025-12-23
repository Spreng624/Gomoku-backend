#include "Player.h"

Player::Player(EventBus<Event> &eventBus) : eventBus(eventBus)
{
    userId = 0;
    playerId = 0;
    sessionId = 0;
    status = PlayerStatus::Offline;
    roomId = 0;
    isGuest = false;
    lastPushUserListTime = 0;
    lastPushRoomListTime = 0;
}

int Player::SendErrorMsg(std::string msg)
{
    Packet packet(sessionId, MsgType::Error);
    packet.AddParam("error", msg);
    eventBus.Publish(Event::SendPacket, packet);
    return 0;
}

int Player::Init(User &user, std::string session, PlayerStatus st)
{
    this->userId = user.GetID();
    this->sessionId = std::stoull(session);
    this->status = st;
    this->isGuest = false;
    return 0;
}

int Player::PushUserList()
{
    return 0;
}

int Player::PushRoomList()
{
    return 0;
}
