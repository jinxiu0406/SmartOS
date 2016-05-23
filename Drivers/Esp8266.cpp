﻿#include "Time.h"
#include "Message\DataStore.h"

#include "Esp8266.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/******************************** 内部Tcp/Udp ********************************/
class EspSocket : public Object, public ITransport, public ISocket
{
private:
	Esp8266&	_Host;

public:
	EspSocket(Esp8266& host, ProtocolType protocol);
	virtual ~EspSocket();

	// 应用配置，修改远程地址和端口
	void Change(const IPEndPoint& remote);

	virtual bool OnWrite(const Buffer& bs);
	virtual uint OnRead(Buffer& bs);

	// 发送数据
	virtual bool Send(const Buffer& bs);
	// 接收数据
	virtual uint Receive(Buffer& bs);
};

class EspTcp : public EspSocket
{
public:
	EspTcp(Esp8266& host);

	virtual String& ToStr(String& str) const { return str + "Tcp_" + Local.Port; }
};

class EspUdp : public EspSocket
{
public:
	EspUdp(Esp8266& host);

	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote);

	// 中断分发  维护状态
	virtual void OnProcess(byte reg);
	// 用户注册的中断事件处理 异步调用
	virtual void RaiseReceive();

	virtual String& ToStr(String& str) const { return str + "Udp_" + Local.Port; }

private:
	virtual bool OnWriteEx(const Buffer& bs, const void* opt);
};

/******************************** 测试 ********************************/
#include "SerialPort.h"
void EspTest(void * param)
{
	SerialPort sp(COM4);

	Esp8266 esp(&sp);
	//esp.Port.SetBaudRate(115200);
	esp.Open();

	Sys.Sleep(50);				//

	if (esp.GetMode() != Station)	// Station模式
		esp.SetMode(Station);

	String ate = "ATE1";			// 开回显
	esp.SendCmd(ate);

	String ssid = "yws007";
	String pwd = "yws52718";

	//String ssid = "FAST_2.4G";
	//String pwd = "yws52718*";
	bool isjoin = esp.JoinAP(ssid, pwd);
	if (isjoin)debug_printf("\r\nJoin ok\r\n");
	else debug_printf("\r\nJoin not ok\r\n");
	Sys.Sleep(1000);
	esp.UnJoinAP();
	Sys.Sleep(1000);
}

String busy = "busy p...";
String discon = "WIFI DISCONNECT";
String conn = "WIFI CONNECTED";
String okstr = "OK";
String gotIp = "WIFI GOT IP";



/******************************** Esp8266 ********************************/

Esp8266::Esp8266(ITransport* port, Pin rst)
{
	Set(port);

	//if(rst != P0) _rst.Set(rst);
}

bool Esp8266::OnOpen()
{
	if(!PackPort::OnOpen()) return false;

	/*if(!_rst.Empty())
	{
		_rst.Open();

		_rst = true;
		Sys.Delay(100);
		_rst = false;
		Sys.Delay(100);
		_rst = true;
	}*/

	return true;
}

void Esp8266::OnClose()
{
	//_rst.Close();

	PackPort::OnClose();
}

// 配置网络参数
void Esp8266::Config()
{

}

ISocket* Esp8266::CreateSocket(ProtocolType type)
{
	switch(type)
	{
		case ProtocolType::Tcp:
			return new EspTcp(*this);

		case ProtocolType::Udp:
			return new EspUdp(*this);

		default:
			return nullptr;
	}
}

/*Esp8266::Esp8266(COM com, AiEspMode mode)
{
	Port.Set(com,115200);
	Port.ByteTime = 1;
	Port.MinSize = 1;
	Mode = mode;
}*/

/*
发送：
"AT+CWMODE=1
"
返回：
"AT+CWMODE=1

OK
"
*/
bool Esp8266::SetMode(AiEspMode mode)
{
	String str = "AT+CWMODE=";
	String cmd;
	switch (mode)
	{
	case Unknown:
		return false;
		//break;
	case Station:
		cmd = str + '1';
		break;
	case Ap:
		cmd = str + '2';
		break;
	case Both:
		cmd = str + '3';
		break;
	default:
		return false;

	}
	if (SendCmd(cmd))
	{
		Mode = mode;
		return true;
	}
	else
		return false;
}

/*
发送：
"AT+CWMODE?
"
返回：
"AT+CWMODE? +CWMODE:1

OK
"
*/
AiEspMode Esp8266::GetMode()
{
	debug_printf("GetMode\r\n");
	String str = "AT+CWMODE?\r\n";
	Send(str);
	MemoryStream rsms;
	if (RevData(rsms, 1000) == 0)return Unknown;

	String mo1 = "+CWMODE:1";
	String mo2 = "+CWMODE:2";
	String mo3 = "+CWMODE:3";

	String rsstr((const char*)rsms.GetBuffer(), rsms.Position());
	Mode = Unknown;
	if (rsstr.IndexOf(mo1) != -1)Mode = Station;
	if (rsstr.IndexOf(mo2) != -1)Mode = Ap;
	if (rsstr.IndexOf(mo3) != -1)Mode = Both;

	debug_printf("Mode = %d\r\n", Mode);

	return Mode;
}

// 发送数据  并按照参数条件 适当返回收到的数据
void  Esp8266::Send(Buffer & dat)
{
	if (dat.Length() != 0)
	{
		Write(dat);
		// 仅仅 send 不需要刷出去
		//if (needRead)
		//	Port.Flush();
	}
}

int Esp8266::RevData(MemoryStream &ms, uint timeMs)
{
	byte temp[64];
	Buffer bf(temp, sizeof(temp));

	TimeWheel tw(0);
	tw.Reset(timeMs / 1000, timeMs % 1000);
	while (!tw.Expired())
	{
		// Port.Read 会修改 bf 的 length 为实际读到的数据长度
		// 所以每次都需要修改传入 buffer 的长度为有效值
		Sys.Sleep(10);
		bf.SetLength(sizeof(temp));
		Read(bf);
		if (bf.Length() != 0) ms.Write(bf);// 把读到的数据
	}

	return	ms.Position();
}

// str 不需要换行
bool Esp8266::SendCmd(String &str, uint timeOutMs)
{
	String cmd(str);
	cmd += "\r\n";
	Send(cmd);

	MemoryStream rsms;
	auto rslen = RevData(rsms, timeOutMs);
	debug_printf("\r\n ");
	str.Show(false);
	debug_printf(" SendCmd rev len:%d\r\n", rslen);
	if (rslen == 0)return false;

	String rsstr((const char *)rsms.GetBuffer(), rsms.Position());
	//rsstr.Show(true);

	bool isOK = true;
	int index = 0;
	int indexnow = 0;

	index = rsstr.IndexOf(str);
	if (index == -1)
	{
		// 没有找到命令本身就表示不通过
		isOK = false;
		debug_printf("not find cmd\r\n");
	}
	else
	{
		indexnow = index + str.Length();
		// 去掉busy
		index = rsstr.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 发现有 busy 并在允许范围就跳转    没有就不动作
			// debug_printf("find busy\r\n");
			indexnow = index + busy.Length();
		}

		index = rsstr.IndexOf(okstr, indexnow);
		if (index == -1 || index - 5 > indexnow)
		{
			// 没有发现  或者 不在范围内就认为是错的
			debug_printf("not find ok\r\n");
			isOK = false;
		}
		else
		{
			// 到达这里表示 得到正确的结果了
			indexnow = index + okstr.Length();
		}
	}
	// 还有多余数据 就扔出去
	//if(rslen -indexnow>5)xx()
	if (isOK)
		debug_printf(" Cmd OK\r\n");
	else
		debug_printf(" Cmd Fail\r\n");
	return isOK;
}

/*
发送：
"AT+CWJAP="yws007","yws52718"
"
返回：	( 一般3秒钟能搞定   密码后面位置会停顿，  WIFI GOT IP 前面也会停顿 )
"AT+CWJAP="yws007","yws52718"WIFI CONNECTED
WIFI GOT IP

OK
"
也可能  (已连接其他WIFI)  70bytes
"AT+CWJAP="yws007","yws52718" WIFI DISCONNECT
WIFI CONNECTED
WIFI GOT IP

OK
"
密码错误返回
"AT+CWJAP="yws007","7" +CWJAP:1

FAIL
"
*/
bool Esp8266::JoinAP(String& ssid, String& pwd)
{
	String str = "AT+CWJAP=";
	str.Show(true);
	str = str + "\"" + ssid + "\",\"" + pwd + "\"";

	String cmd = str + "\r\n";

	// 因为时间跨度很长  所以自己来捞数据
	Send(cmd);
	MemoryStream rsms;
	auto rslen = RevData(rsms, 10000);	// 这里注定捞不干净  哪怕延时20s都不行  原因不明
	debug_printf("\r\nRevData len:%d\r\n", rslen);
	String rsstr((const char *)rsms.GetBuffer(), rsms.Position());
	//rsstr.Show(true);
	int index = 0;
	int indexnow = 0;

	if (indexnow + 20 > rslen)			// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}

	index = rsstr.IndexOf("AT+CWJAP");
	if (index != -1)
		indexnow = index;
	else
	{
		debug_printf("not find cmd\r\n");
		return false;
	}

	// 补捞  重组字符串   因为不动rsms.Position()  所以数据保留不变
	if (cmd.Length()+discon.Length() > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}
	// 干掉 WIFI DISCONNECT
	index = rsstr.IndexOf(discon, indexnow);
	if (index != -1)indexnow = index + discon.Length();

	// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	auto comNum = index <= 0 ? cmd.Length() + conn.Length() : indexnow + conn.Length();
	if (comNum > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}

	index = rsstr.IndexOf(conn, indexnow);
	if (index == -1)
	{
		debug_printf("\r\nindex:%d,  find conn not ok\r\n",index);
		return false;
	}
	else
	{
		// 到达这里表示  已经连接到了
		indexnow = index + conn.Length();
	}

	// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	if (indexnow + 20 > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}

	index = rsstr.IndexOf(gotIp, indexnow);
	if (index == -1 || index - 5 > indexnow)
	{
		debug_printf("not find gotIp\r\n");
		return false;
	}
	else
	{
		// 到达这里表示  已经连接到了
		indexnow = index + gotIp.Length();
	}

	index = rsstr.IndexOf(okstr, indexnow);
	if (index == -1 || index - 5 > indexnow)
	{
		// 没有发现  或者 不在范围内就认为是错的
		debug_printf("not find ok\r\n");
		return false;
	}
	else
	{
		// 到达这里表示 得到正确的结果了
		indexnow = index + okstr.Length();
	}

	debug_printf("\r\nJoinAP  ok\r\n");
	return true;
}

/*
返回：
"AT+CWQAP WIFI DISCONNECT

OK
"
*/
bool Esp8266::UnJoinAP()
{
	String str = "AT+CWQAP";

	String cmd(str);
	cmd += "\r\n";
	Send(cmd);
	MemoryStream rsms;
	auto rslen = RevData(rsms, 2000);
	debug_printf("\r\n ");
	str.Show(false);

	debug_printf(" SendCmd rev len:%d\r\n", rslen);
	if (rslen == 0)return false;
	String rsstr((const char *)rsms.GetBuffer(), rsms.Position());
	//rsstr.Show(true);

	bool isOK = true;
	int index = 0;
	int indexnow = 0;

	index = rsstr.IndexOf(str);
	if (index == -1)
	{
		// 没有找到命令本身就表示不通过
		isOK = false;
		debug_printf("not find cmd\r\n");
	}
	else
	{
		indexnow = index + str.Length();
		// 去掉busy
		index = rsstr.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 发现有 busy 并在允许范围就跳转    没有就不动作
			// debug_printf("find busy\r\n");
			indexnow = index + busy.Length();
		}
		// 断开连接
		index = rsstr.IndexOf(discon, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 断开连接
			indexnow = index + discon.Length();
		}
		// 去掉busy
		index = rsstr.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			indexnow = index + busy.Length();
		}

		index = rsstr.IndexOf(okstr, indexnow);
		if (index == -1 || index - 5 > indexnow)
		{
			// 没有发现  或者 不在范围内就认为是错的
			debug_printf("not find ok\r\n");
			isOK = false;
		}
		else
		{
			// 到达这里表示 得到正确的结果了
			indexnow = index + okstr.Length();
		}
	}
	// 还有多余数据 就扔出去
	//if(rslen -indexnow>5)xx()
	if (isOK)
		debug_printf(" Cmd OK\r\n");
	else
		debug_printf(" Cmd Fail\r\n");
	return isOK;
}

/*
开机自动连接WIFI
*/
bool Esp8266::AutoConn(bool enable)
{
	String cmd = "AT+ CWAUTOCONN=";
	if (enable)
		cmd += '1';
	else
		cmd += '0';

	return SendCmd(cmd);
}


/******************************** Socket ********************************/

EspSocket::EspSocket(Esp8266& host, ProtocolType protocol)
	: _Host(host)
{
	Host		= &host;
	Protocol	= protocol;
}

EspSocket::~EspSocket()
{
	Close();
}

// 应用配置，修改远程地址和端口
void EspSocket::Change(const IPEndPoint& remote)
{
#if DEBUG
	/*debug_printf("%s::Open ", Protocol == 0x01 ? "Tcp" : "Udp");
	Local.Show(false);
	debug_printf(" => ");
	remote.Show(true);*/
#endif

	// 设置端口目的(远程)IP地址
	/*SocRegWrites(DIPR, remote.Address.ToArray());
	// 设置端口目的(远程)端口号
	SocRegWrite2(DPORT, _REV16(remote.Port));*/
}

// 接收数据
uint EspSocket::Receive(Buffer& bs)
{
	if(!Open()) return 0;

	/*// 读取收到数据容量
	ushort size = _REV16(SocRegRead2(RX_RSR));
	if(size == 0)
	{
		// 没有收到数据时，需要给缓冲区置零，否则系统逻辑会混乱
		bs.SetLength(0);
		return 0;
	}

	// 读取收到数据的首地址
	ushort offset = _REV16(SocRegRead2(RX_RD));

	// 长度受 bs 限制时 最大读取bs.Lenth
	if(size > bs.Length()) size = bs.Length();

	// 设置 实际要读的长度
	bs.SetLength(size);

	_Host.ReadFrame(offset, bs, Index, 0x03);

	// 更新实际物理地址,
	SocRegWrite2(RX_RD, _REV16(offset + size));
	// 生效 RX_RD
	WriteConfig(RECV);

	// 等待操作完成
	// while(ReadConfig());

	//返回接收到数据的长度
	return size;*/
	return 0;
}

// 发送数据
bool EspSocket::Send(const Buffer& bs)
{
	if(!Open()) return false;
	/*debug_printf("%s::Send [%d]=", Protocol == 0x01 ? "Tcp" : "Udp", bs.Length());
	bs.Show(true);*/

	/*// 读取状态
	byte st = ReadStatus();
	// 不在UDP  不在TCP连接OK 状态下返回
	if(!(st == SOCK_UDP || st == SOCK_ESTABLISHE))return false;
	// 读取缓冲区空闲大小 硬件内部自动计算好空闲大小
	ushort remain = _REV16(SocRegRead2(TX_FSR));
	if( remain < bs.Length())return false;

	// 读取发送缓冲区写指针
	ushort addr = _REV16(SocRegRead2(TX_WR));
	_Host.WriteFrame(addr, bs, Index, 0x02);
	// 更新发送缓存写指针位置
	addr += bs.Length();
	SocRegWrite2(TX_WR,_REV16(addr));

	// 启动发送 异步中断处理发送异常等
	WriteConfig(SEND);

	// 控制轮询任务，加快处理
	Sys.SetTask(_Host.TaskID, true, 20);*/

	return true;
}

bool EspSocket::OnWrite(const Buffer& bs) {	return Send(bs); }
uint EspSocket::OnRead(Buffer& bs) { return Receive(bs); }

/******************************** Tcp ********************************/

EspTcp::EspTcp(Esp8266& host)
	: EspSocket(host, ProtocolType::Tcp)
{

}

/******************************** Udp ********************************/

EspUdp::EspUdp(Esp8266& host)
	: EspSocket(host, ProtocolType::Udp)
{

}

bool EspUdp::SendTo(const Buffer& bs, const IPEndPoint& remote)
{
	if(remote == Remote) return Send(bs);

	Change(remote);
	bool rs = Send(bs);
	Change(Remote);

	return rs;
}

bool EspUdp::OnWriteEx(const Buffer& bs, const void* opt)
{
	auto ep = (IPEndPoint*)opt;
	if(!ep) return OnWrite(bs);

	return SendTo(bs, *ep);
}

void EspUdp::OnProcess(byte reg)
{
	/*S_Interrupt ir;
	ir.Init(reg);
	// UDP 模式下只处理 SendOK  Recv 两种情况

	if(ir.RECV)
	{
		RaiseReceive();
	}*/
	//	SEND OK   不需要处理 但要清空中断位
}

// UDP 异步只有一种情况  收到数据  可能有多个数据包
// UDP接收到的数据结构： RemoteIP(4 byte) + RemotePort(2 byte) + Length(2 byte) + Data(Length byte)
void EspUdp::RaiseReceive()
{
	/*byte buf[1500];
	Buffer bs(buf, ArrayLength(buf));
	ushort size = Receive(bs);
	Stream ms(bs.GetBuffer(), size);

	// 拆包
	while(ms.Remain())
	{
		IPEndPoint ep	= ms.ReadArray(6);
		ep.Port = _REV16(ep.Port);

		ushort len = ms.ReadUInt16();
		len = _REV16(len);
		// 数据长度不对可能是数据错位引起的，直接丢弃数据包
		if(len > 1500)
		{
			net_printf("W5500 UDP数据接收有误, ep=%s Length=%d \r\n", ep.ToString().GetBuffer(), len);
			return;
		}
		// 回调中断
		Buffer bs3(ms.ReadBytes(len), len);
		OnReceive(bs3, &ep);
	};*/
}