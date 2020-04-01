//*****************************************************************
// Lab09 - SD Card Writing
// Names: Tyler Horiuchi and Joshua Watson
// This lab introduces us to the SD card subsystem of the board. In this lab
// we sample the microphone and write the values to the SD card
//*****************************************************************
#include "mcc_generated_files/mcc.h"
#include "sdCard.h"

#pragma warning disable 328
#pragma warning disable 373
#pragma warning disable 520     
#pragma warning disable 1498

#pragma warning disable 2029
#pragma warning disable 2030

void myTMR0ISR(void);

typedef enum{
    BLUE,
    RED
} activeBuffer_t;
 
#define BLOCK_SIZE          512
#define RATE                1600
#define MAX_NUM_BLOCKS      4
#define SINE_WAVE_ARRAY_LENGTH  26

uint16_t sampleRate = 1600;
const uint8_t sin[SINE_WAVE_ARRAY_LENGTH] = {128, 187, 213, 233, 248, 255, 255, 248, 233, 213, 187, 159, 128, 97, 69, 43, 23, 8, 1, 1, 8, 23, 43, 69, 97};

// Large arrays need to be defined as global even though you may only need to 
// use them in main.  This quirk will be important in the next two assignments.
uint8_t     sdCardBufferRed[BLOCK_SIZE];  
uint8_t     sdCardBufferBlue[BLOCK_SIZE];

uint8_t collectSamples = false;
uint8_t redBufferFull = false;
uint8_t blueBufferFull = false;
        
//----------------------------------------------
// Main "function"
//----------------------------------------------
void main (void) {

    uint8_t     status;
    uint16_t    i;
    uint16_t    j;
    uint32_t    sdCardAddress = 0x00000000;     
    char        cmd, letter;
    
    letter = '0';
    uint8_t recordedBlocks;
    uint8_t readBlocks;
    
    SYSTEM_Initialize();
    CS_SetHigh();
    
    // Provide Baud rate generator time to stabilize before splash screen
    TMR0_WriteTimer(0x0000);
    INTCONbits.TMR0IF = 0;
    while (INTCONbits.TMR0IF == 0);
    
    TMR0_SetInterruptHandler(myTMR0ISR);
    
    INTERRUPT_GlobalInterruptEnable();    
    INTERRUPT_PeripheralInterruptEnable();

	printf("Development Board\r\n");
	printf("inLab 09 terminal\r\n");
    printf("SD card testing\r\n");
	printf("\r\n> ");                       // print a nice command prompt

	for(;;) {

		if (EUSART1_DataReady) {			// wait for incoming data on USART
            cmd = EUSART1_Read();
			switch (cmd) {		// and do what it tells you to do

			//--------------------------------------------
			// Reply with help menu
			//--------------------------------------------
			case '?':
				printf("\r\n-------------------------------------------------\r\n");
                printf("SD card address:  ");
                printf("%04x", sdCardAddress>>16); printf(":"); printf("%04x", sdCardAddress&0X0000FFFF); printf("\r\n");
                printf("-------------------------------------------------\r\n");
                printf("?: help menu\r\n");
				printf("o: k\r\n");
                printf("Z: Reset processor\r\n");                     
                printf("z: Clear the terminal\r\n");                    
                printf("-------------------------------------------------\r\n");
                printf("i: Initialize SD card\r\n");
                printf("-------------------------------------------------\r\n");
                printf("a/A decrease/increase read address\r\n");                        
                printf("r: read a block of %d bytes from SD card\r\n", BLOCK_SIZE);
                printf("1: write a perfect 26 value sine wave to the SD card\r\n", BLOCK_SIZE);                
                printf("-------------------------------------------------\r\n");
                printf("+/-: Increase/Decrease the sample rate by 10 us\r\n");                        
                printf("W: Write microphone => SD card at %d us\r\n", sampleRate);
                printf("s: spool memory to a csv file\r\n");                
                printf("-------------------------------------------------\r\n");
				break;

			//--------------------------------------------
			// Reply with "k", used for PC to PIC test
			//--------------------------------------------
			case 'o':
				printf("o:	ok\r\n");
				break;

            //--------------------------------------------
            // Reset the processor after clearing the terminal
            //--------------------------------------------                      
            case 'Z':
                for (i=0; i<40; i++) printf("\n");
                RESET();                    
                break;

            //--------------------------------------------
            // Clear the terminal
            //--------------------------------------------                      
            case 'z':
                for (i=0; i<40; i++) printf("\n");                            
                break; 
                
            //-------------------------------------------- 
            // Init SD card to get it read to perform read/write
            // Will hang in infinite loop on error.
            //--------------------------------------------    
            case 'i':
                printf("Make sure SD card is properly inserted.\r\n");
                printf("Press any key to continue.\r\n");
                SPI2_Close();
                SPI2_Open(SPI2_DEFAULT);
                while (EUSART1_DataReady == false);
                (void) EUSART1_Read();                  // clear character for next menu action
                SDCARD_HCInitialize(true);
                break;
                
            //--------------------------------------------
            // Increase or decrease block address
            //--------------------------------------------                 
            case 'A':
            case 'a':
                if (cmd == 'a') {
                    sdCardAddress -= BLOCK_SIZE;
                    printf("Decreased ");
                } else{
                    sdCardAddress += BLOCK_SIZE;                
                    printf("Increased ");
                }
                printf("SD card address:  ");
                printf("%04x", sdCardAddress>>16); printf(":"); printf("%04x", sdCardAddress&0X0000FFFF); printf("\r\n");
                break; 
                
            //--------------------------------------------
			// r: read a block of BLOCK_SIZE bytes from SD card                
			//--------------------------------------------
            case 'r':                  
                SDCARD_ReadBlock(sdCardAddress,sdCardBufferRed);
                printf("Read block: \r\n");                               
                printf("    Address:    ");
                printf("%04x", sdCardAddress>>16); printf(":"); printf("%04x", sdCardAddress&0X0000FFFF);    printf("\r\n");
                hexDumpBuffer(sdCardBufferRed);
                break;
                
            //--------------------------------------------
			// 1: write a perfect sine wave to the SD card             
			//--------------------------------------------
            case '1':
                recordedBlocks = 0;
                j = 0;
                while(!EUSART1_DataReady){
                    for(i = 0; i < BLOCK_SIZE; i++){
                        sdCardBufferRed[i] = sin[j];
                        if(j == 26){
                            j = 0;
                        }else{
                            j += 1;
                        }
                    }
                    SDCARD_WriteBlock(sdCardAddress, sdCardBufferRed);
                    while((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);               
                    printf("Write block:\r\n");
                    printf("    Address:    ");
                    printf("%04x", sdCardAddress>>16); printf(":");  printf("%04x", sdCardAddress&0X0000FFFF);    printf("\r\n");
                    sdCardAddress += BLOCK_SIZE;
                    recordedBlocks += 1;
                }
                (void) EUSART1_Read();
                printf("There were %d blocks written to the SD card", recordedBlocks);
                break;
                
            //--------------------------------------------
			// +: increase sampling rate by 10 us             
			//--------------------------------------------
            case '+':                  
                sampleRate += 160;
                break;
                
            //--------------------------------------------
			// -: decrease sampling rate by 10 us             
			//--------------------------------------------
            case '-':
                if(sampleRate <= 480){
                    sampleRate = 320;
                }else{
                    sampleRate -= 160;
                }
                break;
                
            //--------------------------------------------
			// W: write microphone samples to the SD card             
			//--------------------------------------------
            case 'W':                  
                printf("Press any key to start recording audio and press any key to stop recording.\r\n");
                while(!EUSART1_DataReady);
                (void) EUSART1_Read();
                collectSamples = true;
                while(!EUSART1_DataReady){
                    if(redBufferFull){
                        SDCARD_WriteBlock(sdCardAddress, sdCardBufferRed);
                        while((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);               
                        printf("Write block:\r\n");
                        printf("    Address:    ");
                        printf("%04x", sdCardAddress>>16); printf(":");  printf("%04x", sdCardAddress&0X0000FFFF);    printf("\r\n");
                        sdCardAddress += BLOCK_SIZE;
                        redBufferFull = false;
                    }else if(blueBufferFull){
                        SDCARD_WriteBlock(sdCardAddress, sdCardBufferBlue);
                        while((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);               
                        printf("Write block:\r\n");
                        printf("    Address:    ");
                        printf("%04x", sdCardAddress>>16); printf(":");  printf("%04x", sdCardAddress&0X0000FFFF);    printf("\r\n");
                        sdCardAddress += BLOCK_SIZE;
                        blueBufferFull = false;
                    }
                }
                (void) EUSART1_Read();
                collectSamples = false;
                break;
                                          
            //--------------------------------------------
            // s: spool memory to a csv file                
            //--------------------------------------------
            case 's':
                readBlocks = 0;
                
                printf("You may terminate spooling at anytime with a keypress.\r\n");
                printf("To spool terminal contents into a file follow these instructions:\r\n");
                printf("\r\n");
                printf("Right mouse click on the upper left of the PuTTY window\r\n");
                printf("Select:     Change settings...\r\n");
                printf("Select:     Logging\r\n");
                printf("Select:     Session logging: All session output\r\n");
                printf("Log file name: Browse and provide a .csv extension to your file name\r\n");
                printf("Select:     Apply\r\n");
                printf("Press any key to start\r\n");
                
                (void) EUSART1_Read();
                while (!EUSART1_DataReady);
                
                (void) EUSART1_Read();
                while (!EUSART1_DataReady) {
                    SDCARD_ReadBlock(sdCardAddress, sdCardBufferRed);
                    
                    for (i = 0; i < BLOCK_SIZE; i++) {
                        printf("%d\r\n", sdCardBufferRed[i]);
                    }
                    
                    readBlocks++;
                }
                
                printf("Spooled %d out of the %d blocks.\r\n", readBlocks, MAX_NUM_BLOCKS);
                printf("To close the file follow these instructions:\r\n");
                printf("\r\n");
                printf("Right mouse click on the upper left of the PuTTY window\r\n");
                printf("Select:     Change settings...\r\n");
                printf("Select:     Logging\r\n");
                printf("Session logging: None\r\n");
                printf("Select:     Apply\r\n");

                break;

			//--------------------------------------------
			// If something unknown is hit, tell user
			//--------------------------------------------
			default:
				printf("Unknown key %c\r\n",cmd);
				break;
			} // end switch
            
		}	// end if
	} // end while 
} // end main

void myTMR0ISR(void){
    static activeBuffer_t   activeBuffer = RED;
    static uint16_t bufferIndex = 0;
    
    if(collectSamples){
        switch(activeBuffer){
            case RED:
                sdCardBufferRed[bufferIndex] = ADRESH;
                break;
            case BLUE:
                sdCardBufferBlue[bufferIndex] = ADRESH;
                break;
                
        }
        bufferIndex += 1;
        if(bufferIndex == 512){
            if(activeBuffer == RED){
                activeBuffer = BLUE;
                redBufferFull = true;
            }else if(activeBuffer == BLUE){
                activeBuffer = RED;
                blueBufferFull = true;
            }
            bufferIndex = 0;
        }
    }
    
    TMR0_WriteTimer(TMR0_ReadTimer() + (0x10000 - sampleRate));   // More accurate
    ADCON0bits.GO_NOT_DONE = 1;
    INTCONbits.TMR0IF = 0;  
}