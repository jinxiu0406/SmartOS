﻿#include "Message.h"

// 初始化消息，各字段为0
Message::Message(byte code)
{
	Code	= code;
	Length	= 0;
	Data	= nullptr;
	Reply	= false;
	Error	= false;
	State	= nullptr;
}

// 设置数据。
void Message::SetData(const Buffer& bs, uint offset)
{
	Length = bs.Length() + offset;
	if(Length > 0) bs.CopyTo(0, Data + offset, Length);
}

void Message::SetError(byte errorCode, const char* msg)
{
	//byte* p = (byte*)msg;
	//uint len = 0;
	//while(*p++) len++;

	Error	= true;

	Stream ms(Data, MaxDataSize());
	ms.Write(errorCode);
	//ms.Write((const byte*)msg, 0, len);
	ms.Write(String(msg));

	Length	= ms.Position();
}

bool Message::Clone(const Message& msg)
{
	MemoryStream ms;
	msg.Write(ms);

	ms.SetPosition(0);

	return Read(ms);
}

// 负载数据转数据流
Stream Message::ToStream()
{
	Stream ms(Data, MaxDataSize());
	ms.Length = Length;

	return ms;
}

Stream Message::ToStream() const
{
	Stream ms((const byte*)Data, MaxDataSize());
	ms.Length = Length;

	return ms;
}

/*// 负载数据转字节数组
ByteArray Message::ToArray()
{
	return ByteArray(Data, Length);
}*/
