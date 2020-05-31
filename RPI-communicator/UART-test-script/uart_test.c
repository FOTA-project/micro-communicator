#include <stdio.h>
#include <unistd.h>
//#include <wiringPi.h>
#include <wiringSerial.h>

void main(void)
{
	char isDone = 0;
	int uartHandle = -1;
	int key;
	
	//wiringPiSetupGpio();
	//serialOpen();
	
	// try to open the UART device
	uartHandle = serialOpen("/dev/serial0", 9600);
	
	if (uartHandle == -1) // if an error occured when opening UART
	{
		printf("ERROR: couldn't open UART device\n");
	}
	else // if opening UART succeeded
	{
		// write a welcome message
		serialPuts(uartHandle, "Hello from UART C++ communicator!\r\n");
		
		// while we're not done
		while (!isDone)
		{
			// give CPU some time
			usleep(5000 * 1000); // in microsec
			
			if (serialDataAvail(uartHandle) > 0) // if data is available on the UART
			{
				printf("Bytes available = %d\n", serialDataAvail(uartHandle));
				
				// get char
				key = serialGetchar(uartHandle);
				
				if (key == 'c') // if the user wants to close the app
				{
					isDone = 1;
				}
				else // if the user pressed any other key
				{
					
				}
				
				// give feedback for the pressed key
				serialPrintf(uartHandle, "Pressed: %c (ASCII code = %d)\r\n", (char)key, key);
			}
			else // if no data is available on the UART
			{
				
			}
		}
	}
	
	// close UART device
	serialClose(uartHandle);
	
	// print an exit msg
	printf("Program exit\n");	
}
