/*
 * protocol.h
 *
 *  Created on: May 27, 2020
 *      Author: Esraa Awad
 */


#define KEY_ENCRYPTION     0x12345678
#define OK_RESPONSE			1
#define NOK_RESPONSE		2


typedef enum
{
	Cmd_WriteSector = 0x01,
	Cmd_FlashNewApp

}ReqCmd_t;

typedef struct
{
	u32 Key;
	u32 StartAddress;
	u32 AppSize;
	u32 EntryPoint;

}FlashNewApp_t;

typedef struct
{
	u32 Address;
	u32 FrameDataSize;
	u8 Data[8];
	//u16 Data[1024];
}WriteSector_t;


typedef struct
{

	u16 Request_No;
	u8 CMD_No;
	//u32 FrameSize;

}ReqHeader_t;



typedef struct
{
	ReqHeader_t ReqHeader;
	union
	{
		FlashNewApp_t	FlashNewApp;
		WriteSector_t	WriteSector;


	}Data_t;


}ReqDateFrame_t;



typedef struct
{
	u16 Request_No;
	u8 CMD_No;
	u8 Result;


}RespFrame_t;


