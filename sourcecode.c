// Lab 5

#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>

// The next line clears the "C51 command line options:" field when compiling with CrossIDE
// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L
#define TIMER_2_FREQ 7200L

#define LCD_RS P1_7
// #define LCD_RW Px_x // Not used in this code.  Connect to GND
#define LCD_E  P2_0
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0
#define CHARS_PER_LINE 16

#define TOGGLE_BUTTON P2_4
#define HOLD_BUTTON P3_7
#define LED_OUT P2_1
#define SWAP_BUTTON P2_5

unsigned char overflow_count;

float v1_rms = 0; float v2_rms = 0;
float v1 = 0; float v2 = 0;

char _c51_external_startup (void)
{

	//WDTCN = 0xDE; //First key
	//WDTCN = 0xAD; //Second key
  
	VDM0CN=0x80;       // enable VDD monitor
	RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD

	#if (SYSCLK == 48000000L)	
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif
	
	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)	
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif
	
	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)                     
	XBR1     = 0X00;
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	// Configure Uart 0
	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	SCON0 = 0x10;
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
  	
	return 0;
}

void InitADC (void)
{
	SFRPAGE = 0x00;
	ADEN=0; // Disable ADC
	
	ADC0CN1=
		(0x2 << 6) | // 0x0: 10-bit, 0x1: 12-bit, 0x2: 14-bit
        (0x0 << 3) | // 0x0: No shift. 0x1: Shift right 1 bit. 0x2: Shift right 2 bits. 0x3: Shift right 3 bits.		
		(0x0 << 0) ; // Accumulate n conversions: 0x0: 1, 0x1:4, 0x2:8, 0x3:16, 0x4:32
	
	ADC0CF0=
	    ((SYSCLK/SARCLK) << 3) | // SAR Clock Divider. rms is 18MHz. Fsarclk = (Fadcclk) / (ADSC + 1)
		(0x0 << 2); // 0:SYSCLK ADCCLK = SYSCLK. 1:HFOSC0 ADCCLK = HFOSC0.
	
	ADC0CF1=
		(0 << 7)   | // 0: Disable low power mode. 1: Enable low power mode.
		(0x1E << 0); // Conversion Tracking Time. Tadtk = ADTK / (Fsarclk)
	
	ADC0CN0 =
		(0x0 << 7) | // ADEN. 0: Disable ADC0. 1: Enable ADC0.
		(0x0 << 6) | // IPOEN. 0: Keep ADC powered on when ADEN is 1. 1: Power down when ADC is idle.
		(0x0 << 5) | // ADINT. Set by hardware upon completion of a data conversion. Must be cleared by firmware.
		(0x0 << 4) | // ADBUSY. Writing 1 to this bit initiates an ADC conversion when ADCM = 000. This bit should not be polled to indicate when a conversion is complete. Instead, the ADINT bit should be used when polling for conversion completion.
		(0x0 << 3) | // ADWINT. Set by hardware when the contents of ADC0H:ADC0L fall within the window specified by ADC0GTH:ADC0GTL and ADC0LTH:ADC0LTL. Can trigger an interrupt. Must be cleared by firmware.
		(0x0 << 2) | // ADGN (Gain Control). 0x0: PGA gain=1. 0x1: PGA gain=0.75. 0x2: PGA gain=0.5. 0x3: PGA gain=0.25.
		(0x0 << 0) ; // TEMPE. 0: Disable the Temperature Sensor. 1: Enable the Temperature Sensor.

	ADC0CF2= 
		(0x0 << 7) | // GNDSL. 0: reference is the GND pin. 1: reference is the AGND pin.
		(0x1 << 5) | // REFSL. 0x0: VREF pin (external or on-chip). 0x1: VDD pin. 0x2: 1.8V. 0x3: internal voltage reference.
		(0x1F << 0); // ADPWR. Power Up Delay Time. Tpwrtime = ((4 * (ADPWR + 1)) + 2) / (Fadcclk)
	
	ADC0CN2 =
		(0x0 << 7) | // PACEN. 0x0: The ADC accumulator is over-written.  0x1: The ADC accumulator adds to results.
		(0x0 << 0) ; // ADCM. 0x0: ADBUSY, 0x1: TIMER0, 0x2: TIMER2, 0x3: TIMER3, 0x4: CNVSTR, 0x5: CEX5, 0x6: TIMER4, 0x7: TIMER5, 0x8: CLU0, 0x9: CLU1, 0xA: CLU2, 0xB: CLU3

	ADEN=1; // Enable ADC
}

// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter
	
	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
	CKCON0|=0b_0100_0000;
	
	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow
	
	TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN0 & 0x80));  // Wait for overflow
		TMR3CN0 &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN0 = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) Timer3us(250);
}

#define VDD 3.3058 // The measured value of VDD in volts

void InitPinADC (unsigned char portno, unsigned char pinno)
{
	unsigned char mask;
	
	mask=1<<pinno;

	SFRPAGE = 0x20;
	switch (portno)
	{
		case 0:
			P0MDIN &= (~mask); // Set pin as analog input
			P0SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 1:
			P1MDIN &= (~mask); // Set pin as analog input
			P1SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 2:
			P2MDIN &= (~mask); // Set pin as analog input
			P2SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		default:
		break;
	}
	SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
	ADC0MX = pin;   // Select input from pin
	ADINT = 0;
	ADBUSY = 1;     // Convert voltage at the pin
	while (!ADINT); // Wait for conversion to complete
	return (ADC0);
}


float Volts_at_Pin(unsigned char pin)
{
	return ((ADC_at_Pin(pin)*VDD)/0b_0011_1111_1111_1111);
}

void TIMER0_Init(void)
{
	TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
	TMOD|=0b_0000_0001; // Timer/Counter 0 used as a 16-bit counter
	TR0=0; // Stop Timer/Counter 0
}


void LCD_pulse (void)
{
	LCD_E=1;
	Timer3us(40);
	LCD_E=0;
}

void LCD_byte (unsigned char x)
{
	// The accumulator in the C8051Fxxx is bit addressable!
	ACC=x; //Send high nible
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	LCD_pulse();
	Timer3us(40);
	ACC=x; //Send low nible
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	LCD_pulse();
}

void WriteData (unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand (unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E=0; // Resting state of LCD's enable is zero
	// LCD_RW=0; // We are only writing to the LCD in this program
	waitms(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finsih.
}

void LCDprint(char * string, unsigned char line, bit clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' '); // Clear the rest of the line
}

void LCDprint2(char * string, unsigned char line, unsigned char col)
{
	unsigned int j;

	WriteCommand(line==2?0xc0|col:0x80|col); // Move cursor to line and column
	for(j=0; string[j]!=0; j++){ 
		WriteData(string[j]); // Write the message
	}
}


//--------------------------//
//			Main			//
//--------------------------//
void main (void)
{
	float period;
	float Phase_Shift;
	float time_difference;
	float frequency;
	float angfrequency;
	unsigned int bonus_counter = 0;
	unsigned int count = 0; 

	unsigned char str_frequency[4];
	unsigned char str_vref[5];
	unsigned char str_vtest[5];
	unsigned char str_phase[4];
	unsigned char str_period[4]; 
	unsigned char str_angfrequency[4]; 

	TIMER0_Init();

	// Configure the LCD
	LCD_4BIT();

    waitms(500); // Give PuTTy a chance to start before sending
	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
	
	printf ("ADC test program\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);

	
	
	InitPinADC(2, 1); // Configure P2.1 as analog input
	InitPinADC(2, 2); // Configure P2.2 as analog input
	
    InitADC();


	// Check if a signal is connected
	while (1){
		if ((Volts_at_Pin(QFP32_MUX_P2_2) < 3.3) && (Volts_at_Pin(QFP32_MUX_P2_1) < 3.3)){
			LCDprint2("                   ", 1, 0); // clear the screen
			LCDprint2("                   ", 2, 0);
			break;
		}
		count++;
		if (count > 100){
			LCDprint2("Connect ref and    ", 1, 0); //string, row, column
			LCDprint2("  test signals     ", 2, 0); //string, row, column
			count = 100; // keep count at 100 it will reset when a signal is detected
		}
		
	}

	// Starting message on LCD, looks like:
	// 'F : X X H z         P : _ X X X'
	// 'V R : X . X X     V T : X . X X'
	LCDprint2("F:XXHz   P:  .XXX", 1, 0); //string, row, column
	LCDprint2("VR:X.XX  VT:X.XX", 2, 0); //string, row, column

	// Enable Watchdog
	WDTCN = 0b_0001_0101;

	while(1)
	{
    	// Reset the counter
		TL0 = 0; 
		TH0 = 0;
		TF0 = 0;
		overflow_count = 0;

		//Feed the dog
		WDTCN = 0xA5; // reload and restart wdt


		
		while(Volts_at_Pin(QFP32_MUX_P2_2) > 0); // Wait for the signal to be zero
		while(Volts_at_Pin(QFP32_MUX_P2_2) == 0); // Wait for the signal to be one
		TR0 = 1; // Start the timer
		while (Volts_at_Pin(QFP32_MUX_P2_2) > 0) { // Wait for the signal to be zero
			if (TF0 == 1) { // Did the 16-bit timer overflow?
				TF0 = 0;
				overflow_count++;
			}
		}
		while (Volts_at_Pin(QFP32_MUX_P2_2) > 0) { // Wait for the signal to be one
			if (TF0 == 1) { // Did the 16-bit timer overflow?
				TF0 = 0;
				overflow_count++;
			}
		}
		TR0 = 0; // Stop timer 0, the 24-bit number [overflow_count-TH0-TL0] has the period!
		period = (overflow_count*65536.0+TH0*256.0+TL0)*(12.0/SYSCLK)*(2);

		frequency = 1.0/period;
		angfrequency = frequency * 2 * 3.1415926535; 

		while(Volts_at_Pin(QFP32_MUX_P2_2) > 0);
		while(Volts_at_Pin(QFP32_MUX_P2_2) == 0);
		waitms(period*1000/4);
		v1_rms = Volts_at_Pin(QFP32_MUX_P2_2) / 1.41321356237;

		while(Volts_at_Pin(QFP32_MUX_P2_1) > 0); //wait for zero cross of other signal
		while(Volts_at_Pin(QFP32_MUX_P2_1) == 0);
		waitms(period*1000/4);
		v2_rms = Volts_at_Pin(QFP32_MUX_P2_1) / 1.41321356237;
		

		// Find phase shift between signals
		TR0=0; // Stop timer 0
		overflow_count = 0;
		TH0=0; TL0=0; TF0 = 0; 		// Reset the timer

		while (Volts_at_Pin(QFP32_MUX_P2_2) != 0); 	
		while (Volts_at_Pin(QFP32_MUX_P2_2) == 0); 			// Wait for reference signal to be zero
		TR0=1; // start timer
		while (Volts_at_Pin(QFP32_MUX_P2_1) != 0) {
			if (TF0 == 1) { // Did the 16-bit timer overflow?
				TF0 = 0;
				overflow_count++;
			}
		}
		while (Volts_at_Pin(QFP32_MUX_P2_1) == 0) { // Wait for test signal to hit zero
			if (TF0 == 1) { // Did the 16-bit timer overflow?
				TF0 = 0;
				overflow_count++;
			}
		}
		TR0=0; // stop timer


		// Do some math to find phase shift
		time_difference = (overflow_count*65536.0+TH0*256.0+TL0)*(12.0/SYSCLK);
		Phase_Shift = (time_difference * 360.0) / period;   // we now have the phase shift   

		printf("T=%fms, Phase: %f, v1_Rms:%f, v2_Rms:%f\r", 
		period*1000.0, Phase_Shift, v1_rms, v2_rms);

		if (Phase_Shift > 180) {
			Phase_Shift = 360 - Phase_Shift;
			LCDprint2("-", 1, 11);
		}
		else {
			LCDprint2(" ", 1, 11);
		}
		
		/////////////////BONUS SWAP BUTTON///////////////
		
		if (SWAP_BUTTON == 0) {  // Check if the BOOT button between P3.7 and ground is pressed
			waitms(50); // De-bounce
			if (SWAP_BUTTON == 0) {
				while(SWAP_BUTTON == 0); // Wait for push-button release
				bonus_counter++;
			}
		}

		if (bonus_counter == 1) {
			sprintf(str_period, "%3.1f", period*1000);
			LCDprint2(str_period, 1, 2); //string, row, column
			LCDprint2("T:", 1, 0);
			LCDprint2("ms", 1, 7);
		} 
		else if (bonus_counter == 2) {
			sprintf(str_angfrequency, "%3.0f", angfrequency);
			LCDprint2(str_angfrequency, 1, 2); //string, row, column
			LCDprint2("w:", 1, 0);
			LCDprint2("rad ", 1, 5);
		} 
		else {
			bonus_counter = 0;
			sprintf(str_frequency, "%2.0f", frequency);
			LCDprint2(str_frequency, 1, 2); //string, row, column
			LCDprint2("Hz  ", 1, 4);
			LCDprint2("F:", 1, 0);
		}
		
		/////////////////////////////////////////////////


		/////////////////LCD DISPLAYING/////////////////
		
		// Convert floats to strings
//		sprintf(str_frequency, "%2.0f", frequency);
		sprintf(str_vref, "%1.2f", v1_rms);	
		sprintf(str_vtest, "%1.2f", v2_rms); 
		sprintf(str_phase, "%3f", Phase_Shift);
		
		// Print values on screen 
//		LCDprint2(str_frequency, 1, 2); //string, row, column
		LCDprint2(str_phase, 1, 12); //string, row, column
		LCDprint2(str_vref, 2, 3); //string, row, column
		LCDprint2(str_vtest, 2, 12); //string, row, column

		////////////////////////////////////////////////
		
	 }
}
