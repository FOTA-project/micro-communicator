/* includes */
#include <stdio.h>          // printf, fopen, ...
#include <unistd.h>         // delay
#include <wiringSerial.h>   // serial
#include "STD_TYPES.h"
#include "protocol.h"

#define ERROR_SUCCESS                   0
#define ERROR_COULDNT_OPEN_UART        -1
#define ERROR_COULDNT_OPEN_INFO_FILE   -2
#define ERROR_COULDNT_OPEN_DATA_FILE   -3
#define ERROR_COULDNT_OPEN_TEXT_FILE   -4
#define ERROR_COULDNT_OPEN_INSTRUCTIONS_FILE -5
#define ERROR_COULDNT_OPEN_PROGRESS_FILE  -6
#define ERROR_ACK_TIMEDOUT            -7
#define ERROR_APP_SIZE_LARGE          -8

/* flashing state (used in switch) */
#define INITIAL_STATE     0
#define WRITING_SECTOR    1
#define FLASH_DONE        2

#define TEXT_SECTION      0
#define DATA_SECTION      1

#define LOADING_STRING    "#"
#define PRINT_LOADING()   printf(LOADING_STRING); \
                          fflush(stdout)

// python script instructions
#define INSTRUCTION_GET_PROGRESS_FLAG     -1
#define INSTRUCTION_COMM_TIMEOUT          -2
#define INSTRUCTION_TERMINATE_ON_SUCCESS  -3
#define INSTRUCTION_WRITE_MAX_REQUESTS    -4
#define INSTRUCTION_GET_PROGRESS_FLAG_ARB -5

#define PROGRESS_DELAY_CTR_MAX            30


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
u8 DataBuffer[1024]; 

u8 CURRENT_STATE = INITIAL_STATE;
u8 CURRENT_FLASH_SECTION = TEXT_SECTION;

u16 REQ_NUMBER = 0;
u16 CURRENT_COMMAND;

u32 TextOffest;
u32 DataOffest;

u8 failCtr;

u32 maxRequests;
u32 skipRequestNo;
u8 progressDelayCtr;

static u8 RxFrameBuffer [sizeof (ReqDateFrame_t)] ;
ReqDateFrame_t * TXFrame = (ReqDateFrame_t *) RxFrameBuffer;

static u8 TxFrameBuffer [sizeof (RespFrame_t)] ;
RespFrame_t * RXFrame = (RespFrame_t *) TxFrameBuffer;


/* write an array on the UART */
void WriteFile(u32 handleUART, void* buffer, u32 length, u32* outWrittenBytes, void* outNull);

/* Receive an array from the UART, this blocks untill the entire array is filled */
void ReadFile(u32 handleUART, void* outBuffer, u32 length, u32* outWrittenBytes, void* outNull);



int main(void)
{
	u8 i;

	u32 h1;
	u32 byteswritten = 0, bytesread = 0;
   
   s8 state = ERROR_SUCCESS;
   
   failCtr = 0;
   progressDelayCtr = PROGRESS_DELAY_CTR_MAX;
   
   //open port
	h1 = serialOpen("/dev/serial0", 115200);
   
   if (h1 == -1) // if opening the UART handle failed
   {
      printf("### ERROR: couldn't open a handle to the UART device ###\n");
      return ERROR_COULDNT_OPEN_UART;
   }

   /* read data from INFO , TEXT , DATA files  */
	FILE* InfoFilePtr;
	FILE* DataFilePtr;
	FILE* TextFilePtr;
   FILE* progressInstructionFile;
   FILE* lastProgressFile;


	InfoFilePtr = fopen("INFO_FILE.txt", "rb");
   
   if (InfoFilePtr == NULL)
   {
      printf("### ERROR: couldn't open INFO_FILE.txt ###\n");
      return ERROR_COULDNT_OPEN_INFO_FILE;
   }
   
	fread(InfoBuffer, 4, 5, InfoFilePtr);

	for(i = 0; i < 5; i++)
	{
		printf("InfoBuffer[%d] = 0x%X\n", i, InfoBuffer[i]);
	}
	fclose(InfoFilePtr);

	Start_Address = InfoBuffer[0]; 
	APP_SIZE      = InfoBuffer[1]; 
	ENTRY_POINT   = InfoBuffer[2]; 
	DATA_ADDRESS  = InfoBuffer[3]; 
	DATA_SIZE     = InfoBuffer[4]; 

	REM_DATA_SIZE = DATA_SIZE % 8;
	TEXT_SIZE = APP_SIZE - DATA_SIZE;
	REM_TEXT_SIZE = TEXT_SIZE % 8;
   
   maxRequests = (APP_SIZE + 8 - (APP_SIZE % 8)) / 8; // round up to nearest multiple of 8

	DataFilePtr = fopen("DATA_FILE.txt", "rb");
   if (DataFilePtr == NULL)
   {
      printf("### ERROR: couldn't open DATA_FILE.txt ###\n");
      return ERROR_COULDNT_OPEN_DATA_FILE;
   }
   
	fread(DataBuffer, 4, DATA_SIZE, DataFilePtr);
	fclose(DataFilePtr);

	TextFilePtr = fopen("TEXT_FILE.txt", "rb");
   if (TextFilePtr == NULL)
   {
      printf("### ERROR: couldn't open TEXT_FILE.txt ###\n");
      return ERROR_COULDNT_OPEN_TEXT_FILE;
   }
   
	fread(TextBuffer, 4, TEXT_SIZE, TextFilePtr);
	fclose(TextFilePtr);

   progressInstructionFile = fopen("progress.txt", "w+");
   if (progressInstructionFile == NULL)
   {
      printf("### ERROR: couldn't open progress.txt ###\n");
      return ERROR_COULDNT_OPEN_INSTRUCTIONS_FILE;
   }
   
   lastProgressFile = fopen("last-progress.txt", "w+");
   if (lastProgressFile == NULL)
   {
      printf("### ERROR: couldn't open last-progress.txt ###\n");
      return ERROR_COULDNT_OPEN_PROGRESS_FILE;
   }
   
   fseek(progressInstructionFile, 0, SEEK_SET);
   
   // write max requests to server
   fprintf(progressInstructionFile, "%d %d\n", INSTRUCTION_WRITE_MAX_REQUESTS, 
                                               maxRequests);
   fflush(progressInstructionFile);
   
   // request the previous elf progress
   fprintf(progressInstructionFile, "%d\n", INSTRUCTION_GET_PROGRESS_FLAG);
   fflush(progressInstructionFile);
   
   // write the arbitrary value
   fseek(lastProgressFile, 0, SEEK_SET);
   fprintf(lastProgressFile, "%d\n", INSTRUCTION_GET_PROGRESS_FLAG_ARB);
   
   // get number of requests to skip
   do
   {
      fflush(lastProgressFile); // In some implementations, flushing a stream open for reading causes its input buffer to be cleared
      fseek(lastProgressFile, 0, SEEK_SET);
      fscanf(lastProgressFile, "%d", &skipRequestNo);
      //printf("skipRequestNo = %d\n", skipRequestNo);
      usleep(100 * 1000); // 100 ms
   } while (skipRequestNo == INSTRUCTION_GET_PROGRESS_FLAG_ARB);
   
   skipRequestNo -= skipRequestNo % 128; // round down to nearest multiple of 128 (nearest 1KB page)
   
   APP_SIZE -= skipRequestNo * 8; // remove the amount of already-flashed requests (each request = 8 bytes)
   
   /* TO check if Application size is not suitable break while then print error and terminate 
      or TO check if CURRENT_STATE == FLASH_DONE  break while and terminate */
   while((CURRENT_STATE != FLASH_DONE) && (RXFrame->Result != APP_SIZE_NOT_SUITABLE_RESPONSE))
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
         /*printf("sizeof(FlashNewApp_t) = %d\n", sizeof(FlashNewApp_t));
         printf("sizeof(WriteSector_t) = %d\n", sizeof(WriteSector_t));
         printf("sizeof(ReqHeader_t) = %d\n", sizeof(ReqHeader_t));
         printf("sizeof(ReqDateFrame_t) = %d\n", sizeof(ReqDateFrame_t));
         printf("sizeof(RespFrame_t) = %d\n", sizeof(RespFrame_t));

         printf("sizeof(u32) = %d\n", sizeof(u32));
         printf("sizeof(u16) = %d\n", sizeof(u16));
         printf("sizeof(u8) = %d\n", sizeof(u8));*/
         
         PRINT_LOADING();
         
         //send data over UART
         //WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

         /* check if response is not ok send the frame again and receive ack again */
         while (RXFrame->Result == NOK_RESPONSE) //we should test this case 
         {
            WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

            ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
            
            if (RXFrame->Result == NOK_RESPONSE)
            {
               failCtr++;
            }
            else
            {
               failCtr = 0;
            }
            
            if (failCtr == 10)
            {
               printf("### ERROR: failed to send FLASH_NEW_APP request %d times ###\n", failCtr);
               CURRENT_STATE = FLASH_DONE;
               state = ERROR_ACK_TIMEDOUT;
               break;
            }
            
            if (bytesread != sizeof(RespFrame_t))
            {
               printf("### ERROR: bytesread != sizeof(RespFrame_t), terminating ###\n");
               CURRENT_STATE = FLASH_DONE;
               state = ERROR_ACK_TIMEDOUT;
               break;
            }
         }

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
               if (skipRequestNo != 0)
               {
                  skipRequestNo--;
                  
                  REQ_NUMBER++;
                  Start_Address += 8;

                  if (TEXT_SIZE == REM_TEXT_SIZE)
                  {
                     TEXT_SIZE = 0;
                  }
                  else
                  {
                     TEXT_SIZE  -= 8;
                     TextOffest += 8;
                  }
                  
                  continue;
               }
               
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

               PRINT_LOADING();
         
               //send data over UART
               //WriteFile(h1, TXFrame, sizeof(ReqDateFrame_t), &byteswritten, NULL);

               /* check if response is not ok send the frame again and receive ack again */
               while (RXFrame->Result == NOK_RESPONSE) //we should test this case 
               {
                  WriteFile(h1, TXFrame, sizeof(ReqDateFrame_t), &byteswritten, NULL);

                  ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
                  
                  if (RXFrame->Result == NOK_RESPONSE)
                  {
                     failCtr++;
                  }
                  else
                  {
                     failCtr = 0;
                  }
                  
                  if (failCtr == 10)
                  {
                     printf("### ERROR: failed to send TEXT_SECTION request %d times ###\n", failCtr);
                     TEXT_SIZE = 0;
                     CURRENT_STATE = FLASH_DONE;
                     state = ERROR_ACK_TIMEDOUT;
                     break;
                  }
                  
                  if (bytesread != sizeof(RespFrame_t))
                  {
                     printf("### ERROR: bytesread != sizeof(RespFrame_t), terminating ###\n");
                     TEXT_SIZE = 0;
                     CURRENT_STATE = FLASH_DONE;
                     state = ERROR_ACK_TIMEDOUT;
                     break;
                  }
               }

               if ( (RXFrame->Request_No == REQ_NUMBER) &&
                    (RXFrame->CMD_No == CURRENT_COMMAND) &&
                    (RXFrame->Result == OK_RESPONSE) )
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
                  
                  progressDelayCtr--;
                  
                  if (progressDelayCtr == 0)
                  {
                     progressDelayCtr = PROGRESS_DELAY_CTR_MAX;
                     fprintf(progressInstructionFile, "%d\n", REQ_NUMBER - 1);
                     fflush(progressInstructionFile);
                  }
               }
            } //end of while

            CURRENT_FLASH_SECTION = DATA_SECTION;
         }
         else if (CURRENT_FLASH_SECTION == DATA_SECTION)
         {
            while (DATA_SIZE != 0)
            {
               if (skipRequestNo != 0)
               {
                  skipRequestNo--;
                  
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
                  
                  continue;
               }
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
               
               PRINT_LOADING();
         
               //send data over UART
               //WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

               /* check if response is not ok send the frame again and receive ack again */
               while (RXFrame->Result == NOK_RESPONSE) //we should test this case 
               {
                  WriteFile(h1, TXFrame , sizeof(ReqDateFrame_t) , &byteswritten, NULL);

                  ReadFile(h1, RXFrame, sizeof(RespFrame_t), &bytesread, NULL);
                  
                  if (RXFrame->Result == NOK_RESPONSE)
                  {
                     failCtr++;
                  }
                  else
                  {
                     failCtr = 0;
                  }
                  
                  if (failCtr == 10)
                  {
                     printf("### ERROR: failed to send DATA_SECTION request %d times ###\n", failCtr);
                     DATA_SIZE = 0;
                     CURRENT_STATE = FLASH_DONE;
                     state = ERROR_ACK_TIMEDOUT;
                     break;
                  }
                  
                  if (bytesread != sizeof(RespFrame_t))
                  {
                     printf("### ERROR: bytesread != sizeof(RespFrame_t), terminating ###\n");
                     DATA_SIZE = 0;
                     CURRENT_STATE = FLASH_DONE;
                     state = ERROR_ACK_TIMEDOUT;
                     break;
                  }
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
                  
                  progressDelayCtr--;
                  
                  if (progressDelayCtr == 0)
                  {
                     progressDelayCtr = PROGRESS_DELAY_CTR_MAX;
                     
                     fprintf(progressInstructionFile, "%d\n", REQ_NUMBER - 1);
                     fflush(progressInstructionFile);
                  }
               }
            } //end of while
            
            CURRENT_FLASH_SECTION = FLASH_DONE;
            CURRENT_STATE = FLASH_DONE; //will break the while 
         }
      break;

      /* should be removed as it is useless 
      case FLASH_DONE:
      break; 
      */

      }
   }

   /* TO check if Application size is not suitable termiate and print error */
   if (RXFrame->Result == APP_SIZE_NOT_SUITABLE_RESPONSE) 
   {
      printf("### ERROR: RXFrame->Result == APP_SIZE_NOT_SUITABLE_RESPONSE , terminating ###\n");
      state = ERROR_APP_SIZE_LARGE;
   }

   fprintf(progressInstructionFile, "%d\n", REQ_NUMBER - 1);
   fflush(progressInstructionFile);
   
   // terminate the .py script
   fprintf(progressInstructionFile, "%d\n", INSTRUCTION_TERMINATE_ON_SUCCESS);
   fflush(progressInstructionFile);
              
   //close UART Port
	serialClose(h1);
   
   //fclose(progressInstructionFile);
   //fclose(InfoFilePtr);
   //fclose(DataFilePtr);
   //fclose(TextFilePtr);
       
   printf("\n\nFinal state = %d\n", state);
   
   return state;
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
   u16 timeoutCtr = 0x385; // timeout of ~0.5sec
   u32 i = 0;
   
   // foreach char required
   for (; i < length; i++)
   {
      // trap while there're no bytes available on the UART
      while ((serialDataAvail(handleUART) < 1) && timeoutCtr)
      {
         timeoutCtr--;
         // give CPU some time
         usleep(138); // time of each request (8 bytes) / 4 (in microsec)
      }
      
      if (timeoutCtr == 0) // if no data was received during the timeout period
      {
         break;
      }
      
      // while there're avaialble bytes
      while (serialDataAvail(handleUART) > 0)
      {
         // store each byte
         ((u8*)outBuffer)[i] = (u8)serialGetchar(handleUART);
         
         i++;
      }
      
      // this cancels the effect of the i++ of the for loop
      // so that we don't skip a location in the array
      i--;
   }
   
   *outWrittenBytes = i;
}

