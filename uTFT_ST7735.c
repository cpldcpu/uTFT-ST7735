/***********************************************************************
 * µTFT ST7735
 *
 * This is a size and speed optimized ANSI-C library to control 1.8" ST7735 TFTs
 *
 * This library is based on the "china_lcd" library by Tobias Weis, which is in turn based
 * on the Adafruit ST7735 Arduino Library. 
 *
 * Both a hardware and optimized software based SPI implementations are included,
 * which can be configured in the header file.
 *
 * Article by Tobias Weis
 * http://enerds.eu/blog/18-spi-lcd-display-10-eur-with-atmega.html
 *
 * Github repository of the Adafruit lib:
 * https://github.com/adafruit/Adafruit-ST7735-Library
 *
 * The library works with my display, which may be a "red" flag one. Modifications
 * are possibly required for other versions.
 *
 * v0.1 2013/10/06 - Initial release cpldcpu@gmail.com
 *
 */

#define __PROG_TYPES_COMPAT__

#include "uTFT_ST7735.h"
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <limits.h>

#include "glcdfont.c"

int16_t  cursor_x, cursor_y;
uint16_t textcolor, textbgcolor;
uint8_t  wrap; // If set, 'wrap' text at right edge of display


/*
 * myDelay value must be known at compile time,
 * so here's a dirty hack to circumvent this
 */
void myDelay(uint16_t ms) {
    for(int x=0 ; x < ms ; x+=5) {
        _delay_ms(5);
    }
}


/********************** START SPI STUFF *********************************/



#ifdef SPI_SOFTWARE

/* SPI Software implementation */ 
inline void SPI_begin(void) {

	SPIREG |= (1 << SCK);//out
	SPIREG |= (1 << MOSI);//out
	
	SPIPORT &= ~(1 << SCK);//lo
	SPIPORT |= (1 << MOSI); //hi - image flickers if MOSI stays low in between transmissions
	
	DDRB |= (1 << PB2);//out  SS
	PORTB |= (1 << PB2);//hi  SS	
}

inline void SPI_end(void) {
}

/*
inline void spiwrite(uint8_t c) {
	uint8_t i;
	uint8_t mask1=SPIPORT & ~( (1<<SCK) | (1<<MOSI) );
	uint8_t mask2=mask1 | (1<<MOSI);
	
	// Assumed state before call: SCK- Low, MOSI- High 
	for (i=0; i<8 ;i++)
	{
		if (!(c&0x80)) SPIPORT = mask1;  // set data low
		
		SPIPORT |=  (1<< SCK); // SCK hi , data sampled here
		SPIPORT = mask2;       // SCK low, MOSI hi
		c<<=1;						
	}
	// State after call: SCK Low, MOSI high 
}
*/

void spiwrite(uint8_t c) {
	uint8_t i;
	uint8_t mask1=SPIPORT & ~( (1<<SCK) | (1<<MOSI) );
	uint8_t mask2=mask1 | (1<<MOSI);
	
	// Assumed state before call: SCK- Low, MOSI- High
	
		asm volatile(
		
		"		sbrs	%0,7	\n\t"		// 1 bit7
		"		out		%1,%2	\n\t"		// 2
		"		sbi		%1,%4	\n\t"		// 4
		"		out		%1,%3	\n\t"		// 5
		
		"		sbrs	%0,6	\n\t"		// bit6
		"		out		%1,%2	\n\t"
		"		sbi		%1,%4	\n\t"
		"		out		%1,%3	\n\t"
		"		sbrs	%0,5	\n\t"		// bit5
		"		out		%1,%2	\n\t"
		"		sbi		%1,%4	\n\t"
		"		out		%1,%3	\n\t"
		"		sbrs	%0,4	\n\t"		// bit4
		"		out		%1,%2	\n\t"
		"		sbi		%1,%4	\n\t"
		"		out		%1,%3	\n\t"
		"		sbrs	%0,3	\n\t"		// bit3
		"		out		%1,%2	\n\t"
		"		sbi		%1,%4	\n\t"
		"		out		%1,%3	\n\t"
		"		sbrs	%0,2	\n\t"		// bit2
		"		out		%1,%2	\n\t"
		"		sbi		%1,%4	\n\t"
		"		out		%1,%3	\n\t"
		"		sbrs	%0,1	\n\t"		// bit1
		"		out		%1,%2	\n\t"
		"		sbi		%1,%4	\n\t"
		"		out		%1,%3	\n\t"
		"		sbrs	%0,0	\n\t"		// bit0
		"		out		%1,%2	\n\t"
		"		sbi		%1,%4	\n\t"
		"		out		%1,%3	\n\t"
		:	
		:	"r" (c), "I" (_SFR_IO_ADDR(SPIPORT)), "r" (mask1), "r" (mask2), "I" (SCK)
		);

	// State after call: SCK Low, MOSI high
}

	
/* SPI general support functions */

inline void writecommand(uint8_t c) {
	RSPORT &= ~(1 << RS);
	spiwrite(c);
}

inline void writedata(uint8_t c) {
	RSPORT |= (1 << RS);
	spiwrite(c);
}

inline void spistreampixel(uint16_t color) {
	spiwrite(color>>8);
	spiwrite(color&0xff);
}


/* SPI Hardware implementation */

#endif

#ifdef SPI_HARDWARE

inline void SPI_begin(void) {
	
	uint8_t tmp;
    SPIREG |= (1 << SCK);//out
    SPIREG |= (1 << MOSI);//out
    
    SPIPORT &= ~(1 << SCK);//lo
    SPIPORT &= ~(1 << MOSI);//lo

    DDRB |= (1 << PB2);//out  SS
    PORTB |= (1 << PB2);//hi  SS
		 
	tmp=SPCR;		// Move SPCR to temp register to allow better optimization (saves 16 bytes!)
    tmp |= (1 << MSTR) | (1 << SPE);
	tmp &= ~(1 << DORD);	// Set bit order MSB first
	tmp = (tmp & ~SPI_CLOCK_MASK) | (SPI_CLOCK_DIV2);
	tmp = (tmp & ~SPI_MODE_MASK) | SPI_MODE0;	
	SPCR=tmp;
	
	SPSR |= 1<<SPI2X;		
}

inline void SPI_end(void) {
    SPCR &= ~(1 << SPE);
}

inline void spiwrite(uint8_t c) {
    SPDR = c;
    while (!(SPSR & (1 << SPIF))) ;
}

/* SPI general support functions */

inline void writecommand(uint8_t c) {
    RSPORT &= ~(1 << RS);
    spiwrite(c);
}

inline void writedata(uint8_t c) {
    RSPORT |= (1 << RS);
    spiwrite(c);
}

inline void spistreampixel(uint16_t color) {
	spiwrite(color>>8);
	spiwrite(color&0xff);
}

#endif

/********************** END SPI STUFF *********************************/

// Rather than a bazillion writecommand() and writedata() calls, screen
// initialization commands and arguments are organized in these tables
// stored in PROGMEM.  The table may look bulky, but that's mostly the
// formatting -- storage-wise this is hundreds of bytes more compact
// than the equivalent code.  Companion function follows.
#define DELAY 0x80
const uint8_t PROGMEM  InitCmd[] = {                 // Init for 7735R, part 1 (red or green tab)
    21,                       // 15 commands in list:
    ST7735_SWRESET,   DELAY,  //  1: Software reset, 0 args, w/delay
    150,                    //     150 ms delay
    ST7735_SLPOUT ,   DELAY,  //  2: Out of sleep mode, 0 args, w/delay
    255,                    //     500 ms delay
    ST7735_FRMCTR1, 3      ,  //  3: Frame rate ctrl - normal mode, 3 args:
    0x01, 0x2C, 0x2D,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR2, 3      ,  //  4: Frame rate control - idle mode, 3 args:
    0x01, 0x2C, 0x2D,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6      ,  //  5: Frame rate ctrl - partial mode, 6 args:
    0x01, 0x2C, 0x2D,       //     Dot inversion mode
    0x01, 0x2C, 0x2D,       //     Line inversion mode
    ST7735_INVCTR , 1      ,  //  6: Display inversion ctrl, 1 arg, no delay:
    0x07,                   //     No inversion
    ST7735_PWCTR1 , 3      ,  //  7: Power control, 3 args, no delay:
    0xA2,
    0x02,                   //     -4.6V
    0x84,                   //     AUTO mode
    ST7735_PWCTR2 , 1      ,  //  8: Power control, 1 arg, no delay:
    0xC5,                   //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    ST7735_PWCTR3 , 2      ,  //  9: Power control, 2 args, no delay:
    0x0A,                   //     Opamp current small
    0x00,                   //     Boost frequency
    ST7735_PWCTR4 , 2      ,  // 10: Power control, 2 args, no delay:
    0x8A,                   //     BCLK/2, Opamp current small & Medium low
    0x2A,
    ST7735_PWCTR5 , 2      ,  // 11: Power control, 2 args, no delay:
    0x8A, 0xEE,
    ST7735_VMCTR1 , 1      ,  // 12: Power control, 1 arg, no delay:
    0x0E,
    ST7735_INVOFF , 0      ,  // 13: Don't invert display, no args, no delay
    ST7735_MADCTL , 1      ,  // 14: Memory access control (directions), 1 arg:
    0xC8,                   //     row addr/col addr, bottom to top refresh
    ST7735_COLMOD , 1      ,  // 15: set color mode, 1 arg, no delay:
    0x05,
    ST7735_CASET  , 4      ,  //  1: Column addr set, 4 args, no delay:
    0x00, 0x00,             //     XSTART = 0
    0x00, 0x7F,             //     XEND = 127
    ST7735_RASET  , 4      ,  //  2: Row addr set, 4 args, no delay:
    0x00, 0x00,             //     XSTART = 0
    0x00, 0x9F,
    ST7735_GMCTRP1, 16      , //  1: Magical unicorn dust, 16 args, no delay:
    0x02, 0x1c, 0x07, 0x12,
    0x37, 0x32, 0x29, 0x2d,
    0x29, 0x25, 0x2B, 0x39,
    0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      , //  2: Sparkles and rainbows, 16 args, no delay:
    0x03, 0x1d, 0x07, 0x06,
    0x2E, 0x2C, 0x29, 0x2D,
    0x2E, 0x2E, 0x37, 0x3F,
    0x00, 0x00, 0x02, 0x10,
    ST7735_NORON  ,    DELAY, //  3: Normal display on, no args, w/delay
    10,                     //     10 ms delay
    ST7735_DISPON ,    DELAY, //  4: Main screen turn on, no args w/delay
    100
};                  //     100 ms delay



// Companion code to the above tables.  Reads and issues
// a series of LCD commands stored in PROGMEM byte array.
void commandList(const uint8_t *addr) {
    uint8_t  numCommands, numArgs;
    uint16_t ms;

    numCommands = pgm_read_byte(addr++);   // Number of commands to follow
    while(numCommands--) {                 // For each command...
        writecommand(pgm_read_byte(addr++)); //   Read, issue command
        numArgs  = pgm_read_byte(addr++);    //   Number of args to follow
        ms       = numArgs & DELAY;          //   If hibit set, delay follows args
        numArgs &= ~DELAY;                   //   Mask out delay bit
        while(numArgs--) {                   //   For each argument...
            writedata(pgm_read_byte(addr++));  //     Read, issue argument
        }

        if(ms) {
            ms = pgm_read_byte(addr++); // Read post-command delay time (ms)
            if(ms == 255) ms = 500;
            myDelay(ms);
        }
    }
}

void init(void) {
 
    RSREG |= (1 << RS); //out
    RSTREG |= (1 << RST);//out

    SPI_begin();

    RSTPORT |= (1 << RST);
    myDelay(500);
    RSTPORT &= ~(1 << RST);
    myDelay(500);
    RSTPORT |= (1 << RST);
    myDelay(500);

    commandList(InitCmd);
	
    cursor_y = cursor_x = 0;
    textcolor =  0xFFFF;
	textbgcolor = 0x0000;
    wrap = 1;
}


void setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    writecommand(ST7735_CASET); // Column addr set
    writedata(0x00);
    writedata(x0);     // XSTART
    writedata(0x00);
    writedata(x1);     // XEND

    writecommand(ST7735_RASET); // Row addr set
    writedata(0x00);
    writedata(y0);     // YSTART
    writedata(0x00);
    writedata(y1);     // YEND

    writecommand(ST7735_RAMWR); // write to RAM
}


void fillScreen(uint16_t color) {
	fillRect(0, 0, _width, _height,color);
}


void drawPixel(int16_t x, int16_t y, uint16_t color) {
    if((x < 0) ||(x >= _width) || (y < 0) || (y >= _height)) return;

    setAddrWindow(x,y,x+1,y+1);

    RSPORT |= (1 << RS);
	spistreampixel(color);
}


void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    // Rudimentary clipping
    if((x >= _width) || (y >= _height)) return;
    if((y+h-1) >= _height) h = _height-y;
    setAddrWindow(x, y, x, y+h-1);

    RSPORT |= (1 << RS);

    while (h--) {
		spistreampixel(color);
    }
}

void drawFastHLine(int16_t x, int16_t y, int16_t w,
                   uint16_t color) {

    // Rudimentary clipping
    if((x >= _width) || (y >= _height)) return;
    if((x+w-1) >= _width)  w = _width-x;
    setAddrWindow(x, y, x+w-1, y);
 
    RSPORT |= (1 << RS);

    while (w--) {
		spistreampixel(color);
    }
}

// draw a rectangle
void drawRect(int16_t x, int16_t y,
int16_t w, int16_t h,
uint16_t color) {
	drawFastHLine(x, y, w, color);
	drawFastHLine(x, y+h-1, w, color);
	drawFastVLine(x, y, h, color);
	drawFastVLine(x+w-1, y, h, color);
}

// fill a rectangle
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,uint16_t color) {
    // rudimentary clipping (drawChar w/big text requires this)
    if((x >= _width) || (y >= _height)) return;
    if((x + w - 1) >= _width)  w = _width  - x;
    if((y + h - 1) >= _height) h = _height - y;

    setAddrWindow(x, y, x+w-1, y+h-1);

    RSPORT |= (1 << RS);

    for(y=h; y>0; y--) {
        for(x=w; x>0; x--) {
			spistreampixel(color);
        }
    }
}

void invertDisplay(unsigned char i) {
    writecommand(i ? ST7735_INVON : ST7735_INVOFF);
}

#define swap(a, b) { int16_t t = a; a = b; b = t; }

// bresenham's algorithm - thx wikpedia
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
	int16_t steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		swap(x0, y0);
		swap(x1, y1);
	}

	if (x0 > x1) {
		swap(x0, x1);
		swap(y0, y1);
	}

	int16_t dx, dy;
	dx = x1 - x0;
	dy = abs(y1 - y0);

	int16_t err = dx / 2;
	int16_t ystep;

	if (y0 < y1) {
		ystep = 1;
		} else {
		ystep = -1;
	}

	for (; x0<=x1; x0++) {
		if (steep) {
			drawPixel(y0, x0, color);
			} else {
			drawPixel(x0, y0, color);
		}
		err -= dy;
		if (err < 0) {
			y0 += ystep;
			err += dx;
		}
	}
}


void write(uint8_t c) {
	if (c == '\n') {
		cursor_y += 8;
		cursor_x = 0;
		} else if (c == '\r') {
		// skip em
		} else {
		drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor);
		cursor_x += 6;
		if (wrap && (cursor_x > (_width - 6))) {
			cursor_y += 8;
			cursor_x = 0;
		}
	}
}

void print(const char str[]) {
	int x=0;
	while(str[x]) {
		write(str[x]);
		x++;
	}
}

// draw a character
void drawChar(int16_t x, int16_t y, unsigned char c,
uint16_t color, uint16_t bg) {

	if((x >= _width)   || // Clip right
	(y >= _height)     || // Clip bottom
	((x + 5 - 1) < 0)  || // Clip left
	((y + 8 - 1) < 0))    // Clip top
	return;

	c=c-32;
	for (int8_t i=0; i<6; i++ ) {
		uint8_t line;
		if ((i == 5) || (c>(128-32)))   // All invalid characters will print as a space
			line = 0x0;
		else
			line = pgm_read_byte(font+(c*5)+i);

	    setAddrWindow(x+i, y, x+i, y+7);
	    RSPORT |= (1 << RS);

		for (int8_t j = 0; j<8; j++) {
			if (line & 0x1) {
				    spiwrite(color>>8);
					spiwrite(color&0xff);
				} else {
				    spiwrite(bg>>8);
				    spiwrite(bg&0xff);
			}
			line >>= 1; 
		}
	}
}

void setCursor(int16_t x, int16_t y) {
	cursor_x = x;
	cursor_y = y;
}

void setTextColor(uint16_t c, uint16_t b) {
	textcolor = c;
	textbgcolor = b;
}

void setTextWrap(uint8_t w) {
	wrap = w;
}

