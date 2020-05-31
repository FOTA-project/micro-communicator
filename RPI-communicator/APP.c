/* includes */
#include <stdio.h>          // printf
#include <unistd.h>         // delay
#include <wiringSerial.h>   // serial
#include "STD_TYPES.h"
#include "protocol.h"

/* flashing state (used in switch) */
#define INITIAL_STATE     0
#define WRITING_SECTOR    1
#define FLASH_DONE        2

#define TEXT_SECTION      0
#define DATA_SECTION      1


/* global variables */
u32 Start_Address;
u32 APP_SIZE;
u32 ENTRY_POINT;
u32 DATA_ADDRESS;
u32 DATA_SIZE;
u32 REM_DATA_SIZE;
u32 TEXT_SIZE;
u32 REM_TEXT_SIZE;

u32 InfoBuffer[5];

u8 TextBuffer[1024 * 1024];
u8 DataBuffer[1024]; //*1024???

u8 CURRENT_STATE = INITIAL_STATE;
u8 CURRENT_FLASH_SECTION = TEXT_SECTION;

u16 REQ_NUMBER = 1;
u16 CURRENT_COMMAND;

u32 TextOffest;
u32 DataOffest;

static u8 RxFrameBuffer [sizeof (ReqDateFrame_t)] ;
ReqDateFrame_t * TXFrame = (ReqDateFrame_t *) RxFrameBuffer;

static u8 TxFrameBuffer [sizeof (RespFrame_t)] ;
RespFrame_t * RXFrame = (RespFrame_t *) TxFrameBuffer;


/* write an array on the UART */
void WriteFile(u32 handleUART, void* buffer, u32 length, u32* outWrittenBytes, void* outNull);

/* Receive an array from the UART, this blocks untill the entire array is filled */
void ReadFile(u32 handleUART, void* outBuffer, u32 length, u32* outWrittenBytes, void* outNull);



void main(void)
{
	u8 i;

	u32 h1;
	u32 byteswritten = 0, bytesread = 0;
   
   //open port
	h1 = serialOpen("/dev/serial0", 9600);

   printf("UART handle = %d\n", h1);
	
    /* read data from INFO file */
	void*  InfoFilePtr;
	void*  DataFilePtr;
	void*  TextFilePtr;

	InfoFilePtr = fopen("INFO_FILE.txt", "rb");
	//fread(InfoBuffer, 4, 5, InfoFilePtr);

	for(i = 0; i < 5; i++)
	{
		fscanf(InfoFilePtr, "%d,", &InfoBuffer[i] );
		printf("InfoBuffer[i]  = %x\n", InfoBuffer[i] );
	}
	fclose(InfoFilePtr);

	Start_Address = 0x08002000;  //0X08002000//InfoBuffer[0]
	APP_SIZE = InfoBuffer[1] ; // 0x100
	ENTRY_POINT = InfoBuffer[2];  // 0X0800210d
	DATA_ADDRESS =  InfoBuffer[3] + 0x2000;// 0X08004000 //
	DATA_SIZE = InfoBuffer[4] ; // 0x10

	REM_DATA_SIZE = DATA_SIZE % 8;
	TEXT_SIZE = APP_SIZE - DATA_SIZE;
	REM_TEXT_SIZE = TEXT_SIZE % 8;

	DataFilePtr = fopen("DATA_FILE.txt", "rb");
	fread(DataBuffer, 4, DATA_SIZE, DataFilePtr);
	fclose(DataFilePtr);

	TextFilePtr = fopen("TEXT_FILE.txt", "rb");
	fread(TextBuffer, 4, TEXT_SIZE, TextFilePtr);
	fclose(TextFilePtr);


   while(CURRENT_STATE !=  FLASH_DONE )
   {
      switch (CURRENT_STATE) 
      {
      case INITIAL_STATE:

         RXFrame->Result = NOK_RESPONSE;

         //update TXFrame header with Request Header
         TXFrame->ReqHeader.Request_No = REQ_NUMBER;
         TXFrame->ReqHeader.CMD_No = Cmd_FlashNewApp;
         CURRENT_COMMAND = Cmd_FlashNewApp;

         //update TXFrame data with the data of flash new app
         TXFrame->Data_t.FlashNewApp.Key = KEY_ENCRYPTION;
         TXFrame->Data_t.FlashNewApp.StartAddress = Start_Address;
         TXFrame->Data_t.FlashNewApp.AppSize = APP_SIZE;
         TXFrame->Data_t.FlashNewApp.EntryPoint = ENTRY_POINT;

         // DEBUG
         printf("sizeof(FlashNewApp_t) = %d\n", sizeof(FlashNewApp_t));
         printf("sizeof(WriteSector_t) = %d\n", sizeof(WriteSector_t));
         printf("sizeof(ReqHeader_t) = %d\n", sizeof(ReqHeader_t));
         printf("sizeof(ReqDateFrame_t) = %d\n", sizeof(ReqDateFrame_t));
         printf("sizeof(RespFrame_t) = %d\n", sizeof(RespFrame_t));

         printf("sizeof(u32) = %d\n", sizeof(u32));
         printf("sizeof(u16) = %d\n", sizeof(u16));
         printf("sizeof(u8) = %d\n", sizeof(u8));

         //send data over UART
         usleep(150 * 1000); // in microsec
         WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

         printf("before loop\n");

         //check bytesread flag??
         while (RXFrame->Result == NOK_RESPONSE)
         {
            ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
         }

         printf("after loop\n");

         //read data from UART
         if ( (RXFrame->Request_No == REQ_NUMBER) &&
              (RXFrame->CMD_No == CURRENT_COMMAND) &&
              (RXFrame->Result == OK_RESPONSE) )
         {
            CURRENT_STATE = WRITING_SECTOR;
            REQ_NUMBER++;
         }

      break;

      case WRITING_SECTOR:
         if (CURRENT_FLASH_SECTION == TEXT_SECTION)
         {
            while (TEXT_SIZE != 0)
            {
               RXFrame->Result = NOK_RESPONSE;
               CURRENT_COMMAND = Cmd_WriteSector;

               //update TXFrame
               TXFrame->ReqHeader.Request_No = REQ_NUMBER;
               TXFrame->ReqHeader.CMD_No = Cmd_WriteSector;
               TXFrame->Data_t.WriteSector.Address = Start_Address;
               TXFrame->Data_t.WriteSector.FrameDataSize = 8;


               if (TEXT_SIZE == REM_TEXT_SIZE)
               {
                  // initialize all the values with an empty block value
                  for (i = 0; i < 8; i++)
                  {
                     TXFrame->Data_t.WriteSector.Data[i] = 0xFF;
                  }

                  // add the real data
                  for (i = 0; i < REM_TEXT_SIZE; i++)
                  {
                     TXFrame->Data_t.WriteSector.Data[i] = TextBuffer[i + TextOffest];
                  }
               }
               else
               {
                  for (i = 0; i < 8; i++)
                  {
                     TXFrame->Data_t.WriteSector.Data[i] = TextBuffer[i + TextOffest];
                  }
               }

               //send data over UART
               usleep(150 * 1000); // in microsec
               WriteFile(h1, TXFrame, sizeof(ReqDateFrame_t), &byteswritten, NULL);

               //check bytesread flag??
               while (RXFrame->Result == NOK_RESPONSE)
               {
                  ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
               }

               if ( (RXFrame->Request_No == REQ_NUMBER) &&
                    (RXFrame->CMD_No == CURRENT_COMMAND) &&
                    (RXFrame->Result==OK_RESPONSE) )
               {
                  REQ_NUMBER++;
                  Start_Address += 8;

                  if (TEXT_SIZE == REM_TEXT_SIZE)
                  {
                     TEXT_SIZE = 0 ;
                  }
                  else
                  {
                     TEXT_SIZE  -= 8;
                     TextOffest += 8;
                  }

               }

            }//end of while

            CURRENT_FLASH_SECTION = DATA_SECTION;
         }
         else if (CURRENT_FLASH_SECTION == DATA_SECTION)
         {
            while (DATA_SIZE != 0)
            {
               RXFrame->Result = NOK_RESPONSE;
               CURRENT_COMMAND = Cmd_WriteSector;

               //update TXFrame
               TXFrame->ReqHeader.Request_No = REQ_NUMBER;
               TXFrame->ReqHeader.CMD_No = Cmd_WriteSector;
               TXFrame->Data_t.WriteSector.Address = DATA_ADDRESS;
               TXFrame->Data_t.WriteSector.FrameDataSize = 8;


               if (DATA_SIZE == REM_DATA_SIZE)
               {
                  for (i = 0; i < 8; i++)
                  {
                     TXFrame->Data_t.WriteSector.Data[i] = 0xFF;
                  }
                  
                  for (i = 0; i < REM_TEXT_SIZE; i++)
                  {
                     TXFrame->Data_t.WriteSector.Data[i] = DataBuffer[i + DataOffest ];
                  }
               }
               else
               {
                  for (i = 0; i < 8; i++)
                  {
                     TXFrame->Data_t.WriteSector.Data[i] = DataBuffer[i + DataOffest];
                  }
               }

               //send data over UART
               usleep(150 * 1000); // in microsec
               WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

               //check bytesread flag??
               while (RXFrame->Result == NOK_RESPONSE)
               {
                  ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
               }

               //receive from UART
               if ( (RXFrame->Request_No == REQ_NUMBER) &&
                    (RXFrame->CMD_No == CURRENT_COMMAND) &&
                    (RXFrame->Result == OK_RESPONSE) )
               {
                  REQ_NUMBER++;
                  DATA_ADDRESS += 8;

                  if (DATA_SIZE == REM_DATA_SIZE)
                  {
                     DATA_SIZE = 0;
                  }
                  else
                  {
                     DATA_SIZE  -= 8;
                     DataOffest += 8;
                  }
               }
            } //end of while

            CURRENT_FLASH_SECTION = FLASH_DONE;
            CURRENT_STATE = FLASH_DONE;
         }
      break;

      case FLASH_DONE:

      break;
      }
   }

   //close UART Port
	serialClose(h1);
}


/* write an array on the UART */
void WriteFile(u32 handleUART, void* buffer, u32 length, u32* outWrittenBytes, void* outNull)
{
   for (u32 i = 0; i < length; i++)
   {
      serialPutchar(handleUART, ((u8*)buffer)[i]);
   }
   
   *outWrittenBytes = length;
}

/* Receive an array from the UART, this blocks untill the entire array is filled */
void ReadFile(u32 handleUART, void* outBuffer, u32 length, u32* outWrittenBytes, void* outNull)
{
   // foreach char required
   for (u32 i = 0; i < length; i++)
   {
      // trap while there're no bytes available on the UART
      while (serialDataAvail(handleUART) < 1)
      {
         // give CPU some time
         usleep(300); // in microsec
      }
      
      // while there're avaialble bytes
      while (serialDataAvail(handleUART))
      {
         // store each byte
         ((u8*)buffer)[i] = (u8)serialGetchar(handleUART);
         i++;
      }
      
      // this cancels the effect of the i++ of the for loop
      // so that we don't skip a location in the array
      i--;
   }
}

