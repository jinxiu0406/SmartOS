﻿#ifndef __TokenClient_H__
#define __TokenClient_H__

#include "Sys.h"
#include "TokenNet\TokenConfig.h"
#include "TokenMessage.h"
#include "HelloMessage.h"

class TokenSession;

// 微网客户端
class TokenClient
{
private:
	uint	_task;

public:
	uint	Token;		// 令牌
	ByteArray	ID;		// 设备标识
	ByteArray	Key;	// 登录密码

	int		Status;		// 状态。0准备、1握手完成、2登录后

	ulong	LoginTime;	// 登录时间ms
	ulong	LastActive;	// 最后活跃时间ms
	int		Delay;		// 心跳延迟。一条心跳指令从发出到收到所花费的时间

	bool IsOldOrder; 	//是否旧指令

	TokenConfig* TokenConfig;	//网络配置
	TokenController* Control;

	TokenClient();

	void Open();		// 打开客户端
	void Close();

	// 发送消息
	bool Send(TokenMessage& msg);
	bool Reply(TokenMessage& msg);
	bool OnReceive(TokenMessage& msg);

	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

// 本地网络支持
public:
	TokenController* Local;			// 本地网络控制器
	TArray<TokenSession*> Sessions;	// 会话集合

// 常用系统级消息
public:
	// 握手广播
	HelloMessage	Hello;
	void SayHello(bool broadcast = false, int port = 0);
	bool OnHello(TokenMessage& msg);

	// 登录
	void Login();
	bool OnLogin(TokenMessage& msg);
	//设置网络配置
	bool SetTokenConfig(TokenMessage& msg);

	// Ping指令用于保持与对方的活动状态
	void Ping();
	bool OnPing(TokenMessage& msg);
};

// 令牌会话
class TokenSession
{
public:
	TokenClient*	Client;

	uint	Token;		// 令牌
	ByteArray	ID;		// 设备标识
	ByteArray	Key;	// 密钥
	String	Name;		// 名称
	ushort	Version;	// 版本
	IPEndPoint	Addr;	// 地址

	int		Status;		// 状态。0准备、1握手完成、2登录后
	ulong	LoginTime;	// 登录时间ms
	ulong	LastActive;	// 最后活跃时间ms
};

#endif
