//*****************************************************************************
//
// MSP432 main.c - Dolan Clock - Myles Gavic, Thomas Harren, & Brian Mitchell
//
//****************************************************************************

#include "msp.h"

struct color{
	unsigned int red;
	unsigned int green;
	unsigned int blue;
};
struct color myColors[60];
unsigned int colorState=0;
static int cycles=0;
static int second=0;
static int hour=0;
static int hourBlink;
static int mode = 0;
//unsigned int setMode;
//unsigned int settingHour;


void addColor(unsigned int index, unsigned int red, unsigned int green, unsigned int blue) {
	myColors[index].red = red;
	myColors[index].green = green;
	myColors[index].blue = blue;
}

void selectPortFunction(int port, int line, int sel10, int sel1){
	//(p,l,0,0) will set port to Digital I/O
	if(port==1){
		if(P1SEL0 & BIT(line)!=sel10){
			if(P1SEL1 & BIT(line)!=sel1){
				P1SELC|=BIT(line);
			}else{
				P1SEL0^=BIT(line);
			}
		}else{
			if(P1SEL1 & BIT(line)!=sel1)
				P1SEL1^=BIT(line);
		}
	}else{
		if(P2SEL0 & BIT(line)!=sel10){
			if(P2SEL1 & BIT(line)!=sel1){
				P2SELC|=BIT(line);
			}else{
				P2SEL0^=BIT(line);
			}
		}else{
			if(P2SEL1 & BIT(line)!=sel1)
				P2SEL1^=BIT(line);
		}
	}
}

void initColors(void) {
	// to fill 60 minutes, we need 60 colors
	// with three base colors, we need to have each increase for 20 minutes, decrease for 20 minutes, and be off for 20 minutes
	// each color is offset by 20 minutes
	// we multiply every value by 10 so we can use integers for double precision
	// 64 is the magic number becuase 128/20 = 6.4 which is then multiplied by 10
	int i = 3840; // 128 * 3 * 10      128 for the max value that the LEDs can display
	unsigned int yred,ygreen,yblue = 0;
	while(i > 0) {
		if (i > 2560){ // the first third
			yred = (i * -1 + 3840) / 10; // increase from 0 to 128
			ygreen = (i - 2560) / 10; // decrease from 128 to 0
			yblue = 0;
		} else if(i > 1280) { // second third
			yred = (i - 1280) / 10; // decrease from 128 to 0
			ygreen = 0;
			yblue = (i * -1 + 2560) / 10; // increase from 0 to 128
		} else { // third third
			yred = 0;
			ygreen = (i * -1 + 1280) / 10; // increase from 0 to 128
			yblue = i / 10; // decrease from 128 to 0
		}
		addColor((i/64) * -1 + 60, yred, ygreen, yblue); // add each color at index 0-59
		i -= 64; // decrement the indexer
	}
}

void initButtons(void){
	P1DIR&=~(BIT1|BIT4); //0 aka "in" for button on lines 1,4
	P1REN|=BIT1|BIT4;  //enaling internal pull-up/pull-down resistors
	P1OUT|=BIT1|BIT4;  //default circuit to pull-ups
	selectPortFunction(1,1,0,0);
	selectPortFunction(1,4,0,0);
}

void initLEDs(void){
	P1DIR|=BIT0;
	P2DIR|=BIT0|BIT1|BIT2;
	selectPortFunction(1,0,0,0);
	selectPortFunction(2,0,0,0);
	selectPortFunction(2,1,0,0);
	selectPortFunction(2,2,0,0);
}

void setClockFrequency(void){
	CSKEY=0x695A;       //unlock
	CSCTL1=0x00000223;  //run at 128, enable stuff for clock
	CSCLKEN=0x0000800F;
	CSKEY=0xA596;       //lock
}

void configureTimer(void){
	TA0CTL=0x0100;   //Picks clock (above), count up mode, sets internal divider, shuts timer off.

	TA0CCTL0=0x2000; //Pick compare (not capture) register, interrupt off
	TA0CCR0=0x0080;  //(128)//sets the max time compare register (1,2,3 depend on this peak)
	//interrups every milisecond

	TA0CCTL1=0x2010; //Pick compare (not capture) register, interrupt on
	TA0CCR1=0x0080;   //sets the max time compare  for this capture, will wait until overflow (will be overwritten)
	TA0CCTL2=0x2010;
	TA0CCR2=0x0080;
	TA0CCTL3=0x2010;
	TA0CCR3=0x0080;

	TA0CTL=0x0116;   //Sets counter to 0, turns counter on, enables timeout (aka overflow) interrups
}

void setColor(unsigned int red, unsigned int green, unsigned int blue)
{
	if(red > 0x7F) // Take our max value, and set it to be 0x7E so it's not being interrupted when the timer starts to reset (or something like that)
		red = 0x7E;
	else if (red < 0x02) // Take our min value, and set it to be 0x90 so it's larger than the interrupt value, and thus will never be triggered, resulting in the LED always being off
		red = 0x90;
	if(green > 0x7F)
		green = 0x7E;
	else if (green < 0x02)
		green = 0x90;
	if(blue > 0x7F)
		blue = 0x7E;
	else if (blue < 0x02)
		blue = 0x90;
	TA0CCR1=0x80-red;
	TA0CCR2=0x80-green;
	TA0CCR3=0x80-blue;
}

void PortOneInterrupt(void) {
//	TA0CTL&=~0x0010;
//	TA0CCR0 = 0x0000;
	unsigned short iflag=P1IV;
	if (iflag == 0x04) {
		mode++;
		if(mode == 1) { // when setting, reset the hours and minutes
			hour = 0;
			second = 0;
		}
		if(mode > 2) {
			mode = 0;
		}
	}
	if((iflag == 0x0A) && (mode == 1)) { // setting hour
		if(++ hour > 11) { // sets overflow of hour back to 0
			hour = 0;
		}
		P2OUT|=(BIT0|BIT1|BIT2);
	} else if((iflag == 0x0A) && (mode == 2)) { // setting minute
		P2OUT|=(BIT0|BIT1|BIT2);
		second += 60;
		if(second >= 3600) { // sets overflow of second back to 0
			second = 0;
		}
		colorState = second/60;
	}
//	TA0CTL|=0x0014;
//	TA0CCR0 = 0x0080;
}

void resetHourBlink(void) {
	if(hour==0){
		hourBlink=24;
	}else{
		hourBlink = hour*2;
	}
}

void TimerA0Interrupt(void) {
	unsigned short intv=TA0IV;
	static int setHourCycles=0;
	static int setSecondCycles=0;
	if(mode == 1) { // setting hours
		if(intv==0x0E) {// OE is overflow interrupt
			if(++setHourCycles==200)
			{
				P2OUT&=~(BIT0|BIT1|BIT2);
				P1OUT^=BIT0;
				setHourCycles=0;
			}
		}
	} else if(mode == 2) { // setting minutes
		if(intv==0x0E) {// OE is overflow interrupt
			if(++setSecondCycles==100)
			{
				P1OUT^=BIT0;
			} else if(setSecondCycles == 200) {
				P2OUT&=~(BIT0|BIT1|BIT2);
				P1OUT^=BIT0;
				setSecondCycles = 0;
			}
		}
	} else {
		if(intv==0x0E){// OE is overflow interrupt
			P2OUT&=~(BIT0|BIT1|BIT2);
			setColor(myColors[colorState].red, myColors[colorState].green, myColors[colorState].blue);

			if((++cycles)%500 == 0) { //every half second
				if(cycles == 1000) { //every second
					cycles = 0;
					if((++second) % 300 == 0) { // every 5 mintues
						if(second == 3600) { // every hour
							second = 0;
							if(++hour == 12) { // every 12 hours
								hour = 0;
							}
						}
						colorState = second/60;
					}
				}//use every half second to display hour blinks
				if(second%15==0){
					resetHourBlink();
				}
				if((--hourBlink)>0){
					P1OUT^=BIT0;
				}else{
					P1OUT&=~BIT0;
				}
			}
		} else if(intv==0x02 ){//red
			P2OUT|=BIT0;
		} else if(intv==0x04) {//green
			P2OUT|=BIT1;
		} else if(intv==0x06) {//blue
			P2OUT|=BIT2;
		}
	}
}


void main(void){

	WDTCTL = WDTPW | WDTHOLD; //Stop watchdog timer
	initButtons();
	initColors();
	initLEDs();
	configureTimer();
	setClockFrequency();
	P1IE=(BIT1|BIT4);
	P1IES|=(BIT1|BIT4);
	NVIC_EnableIRQ(TA0_N_IRQn); //Enable TA0 interrupts using the NVIC
	//NVIC=nested vector interrupt controller
	NVIC_EnableIRQ(PORT1_IRQn); //Enable port one interrupt

	while(1){}
}
