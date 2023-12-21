#ifndef ISTEAMNETWORKINGMESSAGES
#define ISTEAMNETWORKINGMESSAGES
#pragma once

#include "steamnetworkingtypes.h"
#include "steam_api_common.h"

/// <summary>
/// 发送和接收消息的非面向连接的接口（无论它们是“客户端”还是“服务器”）
/// ISteamNetworkingSockets是面向连接的（像TCP一样），这意味着需要监听和连接，然后使用连接发送消息
/// ISteamNetworkingMessages更像UDP，因为您可以随时向任意对等方发送消息。底层连接是隐式建立的。
/// ISteamNetworkingMessages在ISteamNetworksSockets代码之上工作，因此您可以获得相同的路由和消息传递效率。
/// 区别主要在于你明确建立联系的责任以及你得到的关于联系状态的反馈类型。这两个接口都可以进行“P2P”通信，并且都支持不可靠和可靠的消息、碎片和重组。
/// </summary>
class ISteamNetworkingMessages
{
public:
	/// <summary>
	/// 向指定的主机发送消息.如果我们还没有与该用户进行连接,则会隐式创建连接
	/// 在我们真正开始发送消息数据之前,可能需要进行一些握手. 如果这次握手失败,我们无法接通
	/// 将通过回调SteamNetworkingMessagesSessionFailed_t发布一个错误,操作成功时没有通知(为此,应该让对等放发送回复)
	/// 
	/// 向主机发送消息也会隐式接受来自该主机的任何传入连接
	/// 
	/// nSendFlags 是k_nSteamNetworkingSend_xxx 选项的位掩码
	/// 
	/// nRemoteChannel 是一个路由号码,您可以使用它来帮助将消息路由到不同的系统
	/// 您必须使用相同的通道号调用ReceiveMessagesOnChannel() 才能检索另一端的数据
	/// 
	/// 使用不同的频道与同一用户通话仍将使用相同的底层连接，从而节省资源。如果您不需要此功能，请使用0。否则，小整数是最有效的。
	/// 
	/// 可以保证远程主机（如果接收到可靠消息的话）只接收一次到同一信道上同一主机的可靠消息，并且接收顺序与发送顺序相同。
	/// 
	/// 不存在其他订单担保！特别地，不可靠消息可能被丢弃、相对于彼此和相对于可靠数据无序地接收，或者可能被多次接收。不同频道上的消息不能保证按发送顺序接收。
	/// 
	/// 对于那些熟悉TCP/IP端口或转换打开多个套接字的现有代码库的人来说，请注意：您可能会注意到只有一个通道，
	/// 使用TCP/IP，每个端点都有一个端口号。你可以把频道号想象成*目的地*端口。
	/// 如果你需要每条消息都包括一个“源端口”（这样收件人就可以路由回复），那么就把它放在你的消息中。UDP就是这样工作的！
	/// 
	/// </summary>
	/// <param name="identityRemote">远程标识</param>
	/// <param name="pubData"></param>
	/// <param name="cubData"></param>
	/// <param name="nSendFlags">k_nSteamNetworkingSend_xxx 选项的位掩码</param>
	/// <param name="nRemoteChannel">路由号码</param>
	/// <returns></returns>
	virtual EResult SendMessageToUser(const SteamNetworkingIdentity& identityRemote, const void* pubData, uint32 cubData, int nSendFlags, int nRemoteChannel) = 0;

	/// Reads the next message that has been sent from another user via SendMessageToUser() on the given channel.
	/// Returns number of messages returned into your list.  (0 if no message are available on that channel.)
	///
	/// When you're done with the message object(s), make sure and call SteamNetworkingMessage_t::Release()!
	virtual int ReceiveMessagesOnChannel(int nLocalChannel, SteamNetworkingMessage_t** ppOutMessages, int nMaxMessages) = 0;

	/// Call this in response to a SteamNetworkingMessagesSessionRequest_t callback.
	/// SteamNetworkingMessagesSessionRequest_t are posted when a user tries to send you a message,
	/// and you haven't tried to talk to them first.  If you don't want to talk to them, just ignore
	/// the request.  If the user continues to send you messages, SteamNetworkingMessagesSessionRequest_t
	/// callbacks will continue to be posted periodically.
	///
	/// Returns false if there is no session with the user pending or otherwise.  If there is an
	/// existing active session, this function will return true, even if it is not pending.
	///
	/// Calling SendMessageToUser() will implicitly accepts any pending session request to that user.
	virtual bool AcceptSessionWithUser(const SteamNetworkingIdentity& identityRemote) = 0;

	/// Call this when you're done talking to a user to immediately free up resources under-the-hood.
	/// If the remote user tries to send data to you again, another SteamNetworkingMessagesSessionRequest_t
	/// callback will be posted.
	///
	/// Note that sessions that go unused for a few minutes are automatically timed out.
	virtual bool CloseSessionWithUser(const SteamNetworkingIdentity& identityRemote) = 0;

	/// Call this  when you're done talking to a user on a specific channel.  Once all
	/// open channels to a user have been closed, the open session to the user will be
	/// closed, and any new data from this user will trigger a
	/// SteamSteamNetworkingMessagesSessionRequest_t callback
	virtual bool CloseChannelWithUser(const SteamNetworkingIdentity& identityRemote, int nLocalChannel) = 0;

	/// Returns information about the latest state of a connection, if any, with the given peer.
	/// Primarily intended for debugging purposes, but can also be used to get more detailed
	/// failure information.  (See SendMessageToUser and k_nSteamNetworkingSend_AutoRestartBrokenSession.)
	///
	/// Returns the value of SteamNetConnectionInfo_t::m_eState, or k_ESteamNetworkingConnectionState_None
	/// if no connection exists with specified peer.  You may pass nullptr for either parameter if
	/// you do not need the corresponding details.  Note that sessions time out after a while,
	/// so if a connection fails, or SendMessageToUser returns k_EResultNoConnection, you cannot wait
	/// indefinitely to obtain the reason for failure.
	virtual ESteamNetworkingConnectionState GetSessionConnectionInfo(const SteamNetworkingIdentity& identityRemote, SteamNetConnectionInfo_t* pConnectionInfo, SteamNetConnectionRealTimeStatus_t* pQuickStatus) = 0;
};
#define STEAMNETWORKINGMESSAGES_INTERFACE_VERSION "SteamNetworkingMessages002"

//
// Callbacks
//

#pragma pack( push, 1 )

/// Posted when a remote host is sending us a message, and we do not already have a session with them
struct SteamNetworkingMessagesSessionRequest_t
{
	enum { k_iCallback = k_iSteamNetworkingMessagesCallbacks + 1 };
	SteamNetworkingIdentity m_identityRemote;			// user who wants to talk to us
};

/// Posted when we fail to establish a connection, or we detect that communications
/// have been disrupted it an unusual way.  There is no notification when a peer proactively
/// closes the session.  ("Closed by peer" is not a concept of UDP-style communications, and
/// SteamNetworkingMessages is primarily intended to make porting UDP code easy.)
///
/// Remember: callbacks are asynchronous.   See notes on SendMessageToUser,
/// and k_nSteamNetworkingSend_AutoRestartBrokenSession in particular.
///
/// Also, if a session times out due to inactivity, no callbacks will be posted.  The only
/// way to detect that this is happening is that querying the session state may return
/// none, connecting, and findingroute again.
struct SteamNetworkingMessagesSessionFailed_t
{
	enum { k_iCallback = k_iSteamNetworkingMessagesCallbacks + 2 };

	/// Detailed info about the session that failed.
	/// SteamNetConnectionInfo_t::m_identityRemote indicates who this session
	/// was with.
	SteamNetConnectionInfo_t m_info;
};

#pragma pack(pop)

// Global accessors

// Using standalone lib
#ifdef STEAMNETWORKINGSOCKETS_STANDALONELIB

static_assert(STEAMNETWORKINGMESSAGES_INTERFACE_VERSION[25] == '2', "Version mismatch");

STEAMNETWORKINGSOCKETS_INTERFACE ISteamNetworkingMessages* SteamNetworkingMessages_LibV2();
inline ISteamNetworkingMessages* SteamNetworkingMessages_Lib() { return SteamNetworkingMessages_LibV2(); }

// If running in context of steam, we also define a gameserver instance.
STEAMNETWORKINGSOCKETS_INTERFACE ISteamNetworkingMessages* SteamGameServerNetworkingMessages_LibV2();
inline ISteamNetworkingMessages* SteamGameServerNetworkingMessages_Lib() { return SteamGameServerNetworkingMessages_LibV2(); }

#ifndef STEAMNETWORKINGSOCKETS_STEAMAPI
inline ISteamNetworkingMessages* SteamNetworkingMessages() { return SteamNetworkingMessages_LibV2(); }
inline ISteamNetworkingMessages* SteamGameServerNetworkingMessages() { return SteamGameServerNetworkingMessages_LibV2(); }
#endif
#endif

// Using Steamworks SDK
#ifdef STEAMNETWORKINGSOCKETS_STEAMAPI

	// Steamworks SDK
STEAM_DEFINE_USER_INTERFACE_ACCESSOR(ISteamNetworkingMessages*, SteamNetworkingMessages_SteamAPI, STEAMNETWORKINGMESSAGES_INTERFACE_VERSION);
STEAM_DEFINE_GAMESERVER_INTERFACE_ACCESSOR(ISteamNetworkingMessages*, SteamGameServerNetworkingMessages_SteamAPI, STEAMNETWORKINGMESSAGES_INTERFACE_VERSION);

#ifndef STEAMNETWORKINGSOCKETS_STANDALONELIB
inline ISteamNetworkingMessages* SteamNetworkingMessages() { return SteamNetworkingMessages_SteamAPI(); }
inline ISteamNetworkingMessages* SteamGameServerNetworkingMessages() { return SteamGameServerNetworkingMessages_SteamAPI(); }
#endif
#endif

#endif // ISTEAMNETWORKINGMESSAGES
