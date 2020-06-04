#ifndef PROTOCOL_H
#define PROTOCOL_H

#define KEY_ENCRYPTION           0x12345678
#define OK_RESPONSE			     1
#define NOK_RESPONSE		     2

/* TO check if Application size is suitable or not */
#define APP_SIZE_NOT_SUITABLE_RESPONSE		3



typedef enum
{
	Cmd_FlashNewApp  = 0x01,
	Cmd_WriteSector

} ReqCmd_t;

typedef struct
{
	u32 Key;
	u32 StartAddress;
	u32 AppSize;
	u32 EntryPoint;

} FlashNewApp_t;

typedef struct
{
	u32 Address;
	u32 FrameDataSize;
	u8  Data[8];

} WriteSector_t;

typedef struct
{
	u16 Request_No;
	u16 CMD_No;

} ReqHeader_t;


typedef struct
{
   ReqHeader_t ReqHeader;

   union
   {
      FlashNewApp_t	FlashNewApp;
      WriteSector_t	WriteSector;
   }Data_t;

} ReqDateFrame_t;



typedef struct
{
	u16 Request_No;
	u8 CMD_No;
	u8 Result;
	
} RespFrame_t;


#endif /* PROTOCOL_H */

