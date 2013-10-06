#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <avr/io.h>
#include "uTFT_ST7735.h"

void myDelay(int16_t ms);

int main(void){
	// init the 1.8 lcd display
	init();
	while(1){
		// COLORS AND 'T'
		fillScreen(Color565(0,0,0));
		fillRect(0,0,127,50,Color565(255,0,0));
		setTextColor(Color565(255,255,255),Color565(255,00,00));
		setCursor(55,20);
		print("red");
		fillRect(0,50,127,100,Color565(0,255,0));
		setTextColor(Color565(255,255,255),Color565(0,255,00));
		setCursor(50,70);
		print("green");
		fillRect(0,100,127,159,Color565(0,0,255));
		setTextColor(Color565(255,255,255),Color565(0,00,255));
		setCursor(55,120);
		print("blue");
		
		drawRect(5,5,118,150,Color565(255,255,255));
		
		myDelay(5000);
 
		fillScreen(Color565(0,0,0));

		for (uint8_t y=0; y<160; y+=8)
		{
			drawLine(0, 0, 127, y, Color565(255,0,0));	
			drawLine(0, 159, 127, y, Color565(0,255,0));
			drawLine(127, 0, 0, y, Color565(0,0,255));
			drawLine(127, 159, 0, y, Color565(255,255,255));
		}
		myDelay(5000);
		
		// TEXT		
		fillScreen(Color565(0,0,0));
		setCursor(0,0);
		setTextWrap(1);

		setTextColor(Color565(255,255,255),Color565(0,0,255));
		print("All available chars:\n\n");
		setTextColor(Color565(200,200,255),Color565(50,50,50));
		unsigned char i;
		char ff[]="a";
		for (i=32; i<128; i++) 
		{
			ff[0]=i;
			print(ff);
		}
		myDelay(5000); 
	}

	return 0;
}