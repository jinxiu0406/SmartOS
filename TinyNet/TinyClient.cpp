﻿#include "TinyClient.h"
#include "SerialPort.h"

bool OnClientReceived(Message& msg, void* param);

void DiscoverTask(void* param);
void PingTask(void* param);

static uint _taskDiscover = 0;
static uint _taskPing = 0;

TinyClient::TinyClient(Controller* control)
{
	assert_ptr(control);

	_control = control;

	Server		= 0;
	DeviceType	= 0;
	Password	= 0;

	LastActive	= 0;

	_control->Received	= OnClientReceived;
	_control->Param		= this;
	Param		= NULL;

	OnDiscover	= NULL;
	OnPing		= NULL;
}

void TinyClient::Send(Message& msg)
{
	_control->Send(msg);
}

bool OnClientReceived(Message& msg, void* param)
{
	TinyClient* client = (TinyClient*)param;
	assert_ptr(client);
	
	// 消息转发
	if(client->Received) return client->Received(msg, client->Param);

	return true;
}

// 常用系统级消息

void TinyClient::SetDefault()
{
#if DEBUG
	_control->AddTransport(SerialPort::GetMessagePort());
#endif

	// 注册消息。每个消息代码对应一个功能函数
	_control->Register(1, Discover, this);
	_control->Register(2, Ping, this);
	_control->Register(3, SysID, this);
	_control->Register(4, SysTime, this);
	_control->Register(5, SysMode, this);

	// 发现服务端的任务
	debug_printf("开始寻找服务端 ");
	_taskDiscover = Sys.AddTask(DiscoverTask, this, 0, 2000000);
}

// 最后发送Discover消息的ID，防止被别人欺骗，直接向我发送Discover响应
static byte _lastDiscoverID;

void DiscoverTask(void* param)
{
	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;
	client->Discover();
}

// 发送发现消息，告诉大家我在这
// 格式：2字节设备类型 + 20字节系统ID
void TinyClient::Discover()
{
	Message msg(1);

	// 发送的广播消息，设备类型和系统ID
	MemoryStream ms(msg.Data, ArrayLength(msg.Data));
	ms.Length = 0;
	ms.Write(DeviceType);
	ms.Write(Sys.ID, 0, 20);
	msg.Length = ms.Length;

	_control->Send(msg);

	_lastDiscoverID = msg.Sequence;
}

// Discover响应
// 格式：1字节地址 + 8字节密码
bool TinyClient::Discover(Message& msg, void* param)
{
	// 客户端只处理Discover响应
	if(!msg.Reply || msg.Error) return true;

	// 校验不对
	if(_lastDiscoverID != msg.Sequence) return true;

	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;

	// 解析数据
	MemoryStream ms(msg.Data, msg.Length);
	if(ms.Remain())
		client->_control->Address = ms.Read<byte>();
	if(ms.Remain() >= 8)
		client->Password = ms.Read<ulong>();

	// 记住服务端地址
	client->Server = msg.Src;

	// 取消Discover任务
	debug_printf("停止寻找服务端 ");
	Sys.RemoveTask(_taskDiscover);
	_taskDiscover = 0;

	// 启动Ping任务
	debug_printf("开始Ping服务端 ");
	_taskPing = Sys.AddTask(PingTask, client, 0, 5000000);

	if(client->OnDiscover) return client->OnDiscover(msg, param);

	return true;
}

void PingTask(void* param)
{
	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;
	client->Ping();
}

// Ping指令用于保持与对方的活动状态
void TinyClient::Ping()
{
	if(LastActive > 0 && LastActive + 30000000 < Time.Current())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务
		debug_printf("30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务\r\n");

		debug_printf("停止Ping服务端 ");
		Sys.RemoveTask(_taskPing);

		debug_printf("开始寻找服务端 ");
		_taskDiscover = Sys.AddTask(DiscoverTask, this, 0, 2000000);

		Server = 0;
		Password = 0;

		return;
	}

	Message msg(2);
	msg.Length = 0;

	_control->Send(msg);

	if(LastActive == 0) LastActive = Time.Current();
}

bool TinyClient::Ping(Message& msg, void* param)
{
	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;

	// 忽略响应消息
	if(msg.Reply)
	{
		if(msg.Src == client->Server) client->LastActive = Time.Current();
		return true;
	}

	debug_printf("Message_Ping Length=%d\r\n", msg.Length);

	if(client->OnPing) return client->OnPing(msg, param);

	return true;
}

// 系统时间获取与设置
bool TinyClient::SysTime(Message& msg, void* param)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	debug_printf("Message_SysTime Length=%d\r\n", msg.Length);

	// 负载数据决定是读时间还是写时间
	if(msg.Length >= 8)
	{
		// 写时间
		ulong us = *(ulong*)msg.Data;

		Time.SetTime(us);
	}

	// 读时间
	ulong us2 = Time.Current();
	msg.Length = 8;
	*(ulong*)msg.Data = us2;

	return true;
}

bool TinyClient::SysID(Message& msg, void* param)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	debug_printf("Message_SysID Length=%d\r\n", msg.Length);

	if(msg.Length == 0)
	{
		// 12字节ID，4字节CPUID，4字节设备ID
		msg.SetData(Sys.ID, 5 << 2);
	}
	else
	{
		// 干脆直接输出Sys，前面11个uint
		msg.SetData((byte*)&Sys, 11 << 2);
	}

	return true;
}

bool TinyClient::SysMode(Message& msg, void* param)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	byte mode = 0;
	if(msg.Length > 0) mode = msg.Data[0];

	debug_printf("Message_SysMode Length=%d Mode=%d\r\n", msg.Length, mode);

	switch(mode)
	{
		case 1:	// 系统软重启
			Sys.Reset();
			break;
	}

	msg.Length = 1;
	msg.Data[0] = 0;

	return true;
}
