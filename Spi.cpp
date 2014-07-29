#include "Sys.h"
#include "Port.h"
#include "Spi.h"

static SPI_TypeDef* g_Spis[] = SPIS;
static const Pin g_Spi_Pins_Map[][4] =  SPI_PINS_FULLREMAP;

Spi::Spi(int spi, int speedHz, bool useNss)
{
	if(spi > ArrayLength(g_Spis)) return;

	const Pin* ps = g_Spi_Pins_Map[spi];		//选定spi引脚
    SPI = g_Spis[spi];

    // 计算速度
    ushort pre = 0;
    int n = Sys.Clock / speedHz;   // 分频
    switch(n)
    {
        case 2: pre = SPI_BaudRatePrescaler_2; break;
        case 4: pre = SPI_BaudRatePrescaler_4; break;
        case 8: pre = SPI_BaudRatePrescaler_8; break;
        case 16: pre = SPI_BaudRatePrescaler_16; break;
        case 32: pre = SPI_BaudRatePrescaler_32; break;
        case 64: pre = SPI_BaudRatePrescaler_64; break;
        case 128: pre = SPI_BaudRatePrescaler_128; break;
        case 256: pre = SPI_BaudRatePrescaler_256; break;
        default:
            printf("Spi%d Init Error! speedHz=%d mush be dived with %d\r\n", spi, speedHz, Sys.Clock);
            return;
    }

    Speed = speedHz;
    Retry = 200;

    if(useNss)
    {
#ifdef STM32F10X
        if(SPI == SPI3)
        {
            //PA15是jtag接口中的一员 想要使用 必须开启remap
            RCC_APB2PeriphClockCmd( RCC_APB2Periph_AFIO, ENABLE);
            GPIO_PinRemapConfig( GPIO_Remap_SWJ_JTAGDisable, ENABLE);
        }
#endif
        _nss = new OutputPort(ps[0], false, 10);
    }

    // 仅仅配置，退出当前函数后，对象将自动释放
    AlternatePort clk(ps[1], false, 10);
    AlternatePort msio(ps[2], false, 10);
    AlternatePort mosi(ps[3], false, 10);

#ifdef STM32F10X
    /*使能SPI时钟*/
	switch(spi)
	{
		case SPI_1 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE); break;
		case SPI_2 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); break;
		case SPI_3 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE); break;
	}
#else
	// SPI都在GPIO_AF_0分组内
    GPIO_PinAFConfig(_GROUP(ps[1]), _PIN(ps[1]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(ps[2]), _PIN(ps[2]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(ps[3]), _PIN(ps[3]), GPIO_AF_0);

	switch(spi)
	{
        case SPI_1 : RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);  	break;
	}
#endif

	Stop();

	SPI_InitTypeDef sp;
    SPI_StructInit(&sp);
	sp.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //双线全双工
	sp.SPI_Mode = SPI_Mode_Master;      // 主模式
	sp.SPI_DataSize = SPI_DataSize_8b;  // 数据大小8位 SPI发送接收8位帧结构
	sp.SPI_CPOL = SPI_CPOL_Low;         // 时钟极性，空闲时为低
	sp.SPI_CPHA = SPI_CPHA_1Edge;       // 第1个边沿有效，上升沿为采样时刻
	sp.SPI_NSS = SPI_NSS_Soft;          // NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
	sp.SPI_BaudRatePrescaler = pre;     // 8分频，9MHz 定义波特率预分频的值
	sp.SPI_FirstBit = SPI_FirstBit_MSB; // 高位在前。指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
	sp.SPI_CRCPolynomial = 7;           // CRC值计算的多项式

    SPI_Init(SPI, &sp);
    SPI_Cmd(SPI, ENABLE);

    _spi = spi;
}

Spi::~Spi()
{
    Stop();

    SPI_Cmd(SPI, DISABLE);
    SPI_I2S_DeInit(SPI);
    
    if(_nss) delete _nss;
    _nss = NULL;
}

byte Spi::Write(byte data)
{
	int retry = Retry;
    while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_TXE) == RESET)
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }

#ifdef STM32F10X
	SPI_I2S_SendData(SPI, data);
#else
	SPI_SendData8(SPI, data);
#endif

	retry = Retry;
	while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_RXNE) == RESET) //是否发送成功
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }
#ifdef STM32F10X
	return SPI_I2S_ReceiveData(SPI);
#else
	return SPI_ReceiveData8(SPI); //返回通过SPIx最近接收的数据
#endif
}

ushort Spi::Write16(ushort data)
{
    // 双字节操作，超时次数加倍
	int retry = Retry << 1;
	while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_TXE) == RESET)
	{
        if(--retry <= 0) return ++Error; // 超时处理
	}

#ifdef STM32F10X
	SPI_I2S_SendData(SPI, data);
#else
	SPI_I2S_SendData16(SPI, data);
#endif

    retry = Retry << 1;
	while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_RXNE) == RESET)
	{
        if(--retry <= 0) return ++Error; // 超时处理
	}

#ifdef STM32F10X
	return SPI_I2S_ReceiveData(SPI);
#else
	return SPI_I2S_ReceiveData16(SPI);
#endif
}

// 拉低NSS，开始传输
void Spi::Start()
{
    //if(_nss != P0) Port::Write(_nss, false);
    if(_nss) *_nss = false;

    // 开始新一轮事务操作，错误次数清零
    Error = 0;
}

// 拉高NSS，停止传输
void Spi::Stop()
{
    //if(_nss != P0) Port::Write(_nss, true);
    if(_nss) *_nss = true;
}
