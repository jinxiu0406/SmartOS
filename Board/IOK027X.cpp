﻿#include "IOK027X.h"

#include "Task.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TokenNet\TokenController.h"
#include "App\FlushPort.h"


IOK027X::IOK027X()
{
	Host	= nullptr;
	Client	= nullptr;
	HostAP	= nullptr;	// 网络主机
}

void IOK027X::Setup(ushort code, cstring name, COM message, int baudRate)
{
	auto& sys	= (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

    // 初始化系统
    //Sys.Clock = 48000000;
    sys.Init();
#if DEBUG
    sys.MessagePort = message; // 指定printf输出的串口
    Sys.ShowInfo();
#endif

	// WatchDog::Start();
	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();
}

// 网络就绪
void OnNetReady(IOK027X& ap, ISocketHost& host)
{
	if (ap.Client) ap.Client->Open();
}

static void SetWiFiTask(void* param)
{
	auto ap	= (IOK027X*)param;
	auto client	= ap->Client;
	auto esp	= (Esp8266*)ap->Host;

	client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
}

ISocketHost* IOK027X::Open8266(bool apOnly)
{
	auto host	= (Esp8266*)Create8266();

	if(apOnly) host->WorkMode	= SocketMode::AP;

	Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	host->NetReady	= Delegate<ISocketHost&>(OnNetReady, this);

	host->OpenAsync();

	return host;
}

ISocketHost* IOK027X::Create8266()
{
	debug_printf("\r\nEsp8266::Create \r\n");

	auto srp	= new SerialPort(COM2, 115200);
	srp->Tx.SetCapacity(0x100);
	srp->Rx.SetCapacity(0x100);

	auto net	= new Esp8266(srp, PB2, PA1);
	net->InitConfig();
	net->LoadConfig();

	// 配置模式作为工作模式
	net->WorkMode	= net->Mode;

	return net;
}


/******************************** Token ********************************/

void IOK027X::CreateClient()
{
	if(Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client		= new TokenClient();
	//client->Control	= ctrl;
	//client->Local	= ctrl;
	client->Cfg		= tk;

	Client	= client;
}

void IOK027X::OpenClient()
{
	debug_printf("\r\n OpenClient \r\n");
	assert(Host, "Host");

	auto tk = TokenConfig::Current;
	AddControl(*Host, *tk);

	TokenConfig cfg;
	cfg.Protocol	= ProtocolType::Udp;
	cfg.ServerIP	= IPAddress::Broadcast().Value;
	cfg.ServerPort	= 3355;
	AddControl(*Host, cfg);

	if(HostAP) AddControl(*HostAP, cfg);
}

void IOK027X::AddControl(ISocketHost& host, TokenConfig& cfg)
{
	// 创建连接服务器的Socket
	auto socket	= host.CreateSocket(cfg.Protocol);
	socket->Remote.Port		= cfg.ServerPort;
	socket->Remote.Address	= IPAddress(cfg.ServerIP);
	socket->Server	= cfg.Server();

	// 创建连接服务器的控制器
	auto ctrl		= new TokenController();
	//ctrl->Port = dynamic_cast<ITransport*>(socket);
	ctrl->Socket	= socket;

	// 创建客户端
	auto client		= Client;
	client->Controls.Add(ctrl);

	// 如果不是第一个，则打开远程
	if(client->Controls.Count() > 1) ctrl->ShowRemote	= true;
}

/******************************** 2401 ********************************/

/*int Fix2401(const Buffer& bs)
{
	//auto& bs	= *(Buffer*)param;
	// 微网指令特殊处理长度
	uint rs	= bs.Length();
	if(rs >= 8)
	{
		rs = bs[5] + 8;
		//if(rs < bs.Length()) bs.SetLength(rs);
	}
	return rs;
}

ITransport* IOK027X::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	debug_printf("\r\n Create2401 \r\n");

	static Spi spi(spi_, 10000000, true);
	static NRF24L01 nrf;
	nrf.Init(&spi, ce, irq, power);

	auto tc	= TinyConfig::Create();
	if(tc->Channel == 0)
	{
		tc->Channel	= 120;
		tc->Speed	= 250;
	}
	if(tc->Interval == 0)
	{
		tc->Interval= 40;
		tc->Timeout	= 1000;
	}

	nrf.AutoAnswer	= false;
	nrf.DynPayload	= false;
	nrf.Channel		= tc->Channel;
	//nrf.Channel		= 120;
	nrf.Speed		= tc->Speed;

	nrf.FixData	= Fix2401;

	if(WirelessLed) net->Led	= CreateFlushPort(WirelessLed);

	nrf.Master	= true;

	return &nrf;
}*/

/*
NRF24L01+ 	(SPI3)		|	W5500		(SPI2)		|	TOUCH		(SPI3)
NSS			|				NSS			|	PD6			NSS
CLK			|				SCK			|				SCK
MISO		|				MISO		|				MISO
MOSI		|				MOSI		|				MOSI
PE3			IRQ			|	PE1			INT(IRQ)	|	PD11		INT(IRQ)
PD12		CE			|	PD13		NET_NRST	|				NET_NRST
PE6			POWER		|				POWER		|				POWER

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

TFT
FSMC_D 0..15		TFT_D 0..15
NOE					RD
NWE					RW
NE1					RS
PE4					CE
PC7					LIGHT
PE5					RST

PE13				KEY1
PE14				KEY2

PE15				LED2
PD8					LED1


USB
PA11 PA12
*/
