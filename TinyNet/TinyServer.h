﻿#ifndef __TinyServer_H__
#define __TinyServer_H__

#include "Sys.h"
#include "Message.h"
#include "Controller.h"
#include "TinyMessage.h"

// 微网客户端
class TinyServer
{
private:
	TinyController* _control;

public:
	ushort	DeviceType;	// 设备类型。两个字节可做二级分类

	TinyServer(TinyController* control);

	// 发送消息
	bool Send(Message& msg);
	bool Reply(Message& msg);

	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

// 常用系统级消息
public:
	// 设置默认系统消息
	void Start();

	// 广播发现系统
	void Discover();
	MessageHandler OnDiscover;
	static bool Discover(Message& msg, void* param);

	// Ping指令用于保持与对方的活动状态
	void Ping();
	MessageHandler OnPing;
	static bool Ping(Message& msg, void* param);

	// 询问及设置系统时间
	static bool SysTime(Message& msg, void* param);

	// 询问系统标识号
	static bool SysID(Message& msg, void* param);

	// 设置系统模式
	static bool SysMode(Message& msg, void* param);
};

#endif
