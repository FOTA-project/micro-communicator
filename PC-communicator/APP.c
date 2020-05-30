/* 1-send req number
 2-send cmd number  //flash new app
 3-send data

 */
#include <stdio.h>
//#include <stdlib.h>
#include <windows.h>

#include "STD_TYPES.h"
#include "protocol.h"
#include "APP.h"

u32 Start_Address;
u32 APP_SIZE;
u32 ENTRY_POINT;
u32 DATA_ADDRESS;
u32 DATA_SIZE;
u32 REM_DATA_SIZE;
u32 TEXT_SIZE;
u32 REM_TEXT_SIZE;

u32 InfoBuffer[3];
u8 TextBuffer[1024 * 1024];
u8 DataBuffer[1024]; //*1024

u16 REQ_NUMBER = 1;

u8 CURRENT_STATE = INITIAL_STATE;
u8 CURRENT_COMMAND;
u8 CURRENT_FLASH_SECTION;

static u8 RxFrameBuffer [sizeof (ReqDateFrame_t)] ;
ReqDateFrame_t * TXFrame = (ReqDateFrame_t *) RxFrameBuffer;



static u8 TxFrameBuffer [sizeof (RespFrame_t)] ;
RespFrame_t * RXFrame = (RespFrame_t *) TxFrameBuffer;

/* for usb */
HANDLE GetSerialPort(char *);

void main(void) {

	/* for usb */
	HANDLE h1;
	DWORD byteswritten = 0, bytesread = 0;
    //open port
	h1 = GetSerialPort("\\\\.\\COM5");

   printf("UART handle = %d\n", h1);

	void* InfoFilePtr;
	void * DataFilePtr;
	void *TextFilePtr;

	InfoFilePtr = fopen("INFO_FILE.txt", "rb");
	fread(InfoBuffer, 4, 5, InfoFilePtr);
	fclose(InfoFilePtr);

	Start_Address = InfoBuffer[0];
	APP_SIZE = InfoBuffer[1];
	ENTRY_POINT = InfoBuffer[2];
	DATA_ADDRESS = InfoBuffer[3];
	DATA_SIZE = InfoBuffer[4];
	REM_DATA_SIZE = DATA_SIZE % 8;
	TEXT_SIZE = APP_SIZE - DATA_SIZE;
	REM_TEXT_SIZE = TEXT_SIZE % 8;

	DataFilePtr = fopen("DATA_FILE.txt", "rb");
	fread(DataBuffer, 4, DATA_SIZE, DataFilePtr);
	fclose(DataFilePtr);

	TextFilePtr = fopen("TEXT_FILE.txt", "rb");
	fread(TextBuffer, 4, TEXT_SIZE, TextFilePtr);
	fclose(TextFilePtr);

	switch (CURRENT_STATE) {
	case INITIAL_STATE:
		RXFrame->Result = NOK_RESPONSE; //??
		//TXframe
		//header
		TXFrame->ReqHeader.Request_No = REQ_NUMBER;
		TXFrame->ReqHeader.CMD_No = Cmd_FlashNewApp;
		CURRENT_COMMAND = Cmd_FlashNewApp;
		//data of flash new app

		TXFrame->Data_t.FlashNewApp.Key = KEY_ENCRYPTION;
		TXFrame->Data_t.FlashNewApp.StartAddress = Start_Address;
		TXFrame->Data_t.FlashNewApp.AppSize = APP_SIZE;
		TXFrame->Data_t.FlashNewApp.EntryPoint = ENTRY_POINT;

      printf("sizeof(FlashNewApp_t) = %d\n", sizeof(FlashNewApp_t));
      printf("sizeof(WriteSector_t) = %d\n", sizeof(WriteSector_t));
      printf("sizeof(ReqHeader_t) = %d\n", sizeof(ReqHeader_t));
      printf("sizeof(ReqDateFrame_t) = %d\n", sizeof(ReqDateFrame_t));
      printf("sizeof(RespFrame_t) = %d\n", sizeof(RespFrame_t));

      printf("sizeof(u32) = %d\n", sizeof(u32));
      printf("sizeof(u16) = %d\n", sizeof(u16));
      printf("sizeof(u8) = %d\n", sizeof(u8));

		//send data over usb
		WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);//h1_buffer //data_size

      printf("before loop\n");
        
		//check bytesread flag??
		while (RXFrame->Result == NOK_RESPONSE) {
			ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
		}
        
        
      printf("after loop\n");
        
		if (RXFrame->Request_No
				== REQ_NUMBER&& RXFrame->CMD_No==CURRENT_COMMAND && RXFrame->Result==OK_RESPONSE) {
			CURRENT_STATE = WRITING_SECTOR;
			REQ_NUMBER++;
		}

		break;

	case WRITING_SECTOR:
		if (CURRENT_FLASH_SECTION == TEXT_SECTION) {
			while (TEXT_SIZE != 0) {
				RXFrame->Result = NOK_RESPONSE; //??
				//TXframe
				//header
				TXFrame->ReqHeader.Request_No = REQ_NUMBER;
				TXFrame->ReqHeader.CMD_No = Cmd_WriteSector;
				CURRENT_COMMAND = Cmd_WriteSector;

				TXFrame->Data_t.WriteSector.Address = Start_Address;
				TXFrame->Data_t.WriteSector.FrameDataSize = 8;
				u8 i;

				if (TEXT_SIZE == REM_TEXT_SIZE) {
					for (i = 0; i < 8; i++) {
						TXFrame->Data_t.WriteSector.Data[i] = 0xFF;
					}
					for (i = 0; i < REM_TEXT_SIZE; i++) {
						TXFrame->Data_t.WriteSector.Data[i] = TextBuffer[i];
					}
				} else {
					for (i = 0; i < 8; i++) {
						TXFrame->Data_t.WriteSector.Data[i] = TextBuffer[i];
					}
				}

				//send data over usb
				WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

				//check bytesread flag??
				while (RXFrame->Result == NOK_RESPONSE) {
					ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
				}
				if (RXFrame->Request_No
						== REQ_NUMBER&& RXFrame->CMD_No==CURRENT_COMMAND && RXFrame->Result==OK_RESPONSE) {
					REQ_NUMBER++;

					Start_Address += 0x2;
					TEXT_SIZE -= 8;
				}

			}//end of while
			CURRENT_FLASH_SECTION = DATA_SECTION;

		}

		else if (CURRENT_FLASH_SECTION == DATA_SECTION) {

			while (DATA_SIZE != 0) {
				RXFrame->Result = NOK_RESPONSE; //??
				//TXframe
				//header
				TXFrame->ReqHeader.Request_No = REQ_NUMBER;
				TXFrame->ReqHeader.CMD_No = Cmd_WriteSector;
				CURRENT_COMMAND = Cmd_WriteSector;

				TXFrame->Data_t.WriteSector.Address = DATA_ADDRESS;
				TXFrame->Data_t.WriteSector.FrameDataSize = 8;
				u8 i;

				if (DATA_SIZE == REM_DATA_SIZE) {
					for (i = 0; i < 8; i++) {
						TXFrame->Data_t.WriteSector.Data[i] = 0xFF;
					}
					for (i = 0; i < REM_TEXT_SIZE; i++) {
						TXFrame->Data_t.WriteSector.Data[i] = DataBuffer[i];
					}
				} else {
					for (i = 0; i < 8; i++) {
						TXFrame->Data_t.WriteSector.Data[i] = DataBuffer[i];
					}
				}

				//send data over usb
				WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

				//check bytesread flag??
				while (RXFrame->Result == NOK_RESPONSE) {
					ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
				}
				if (RXFrame->Request_No
						== REQ_NUMBER&& RXFrame->CMD_No==CURRENT_COMMAND && RXFrame->Result==OK_RESPONSE) {
					REQ_NUMBER++;

					DATA_ADDRESS += 0x2;
					DATA_SIZE -= 8;
				}

			}				//enf of while
			CURRENT_FLASH_SECTION = FLASH_DONE;
			CURRENT_STATE = FLASH_DONE;
		}
		break;
	case FLASH_DONE:
		//do nothing
		break;

	}

	//close USB Port
	  CloseHandle(h1);
}

HANDLE GetSerialPort(char *p)
{
   // https://docs.microsoft.com/en-us/windows/win32/devio/configuring-a-communications-resource

    HANDLE hSerial;
    hSerial = CreateFile(p,
                         GENERIC_READ | GENERIC_WRITE,
                         0,
                         NULL,
                         OPEN_EXISTING,
                         0,
                         NULL);

    DCB dcbSerialParams = {0};

    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    //  Build on the current configuration by first retrieving all current
    //  settings.
    GetCommState(hSerial, &dcbSerialParams);

    dcbSerialParams.BaudRate  = CBR_9600;
    dcbSerialParams.ByteSize  = 8;
    dcbSerialParams.StopBits  = ONESTOPBIT;
    dcbSerialParams.Parity    = NOPARITY;


    SetCommState(hSerial, &dcbSerialParams);

    return hSerial;
}
