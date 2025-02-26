/*
 *  This source is for PIC18F47Q43 UART, I2C, SPI and TIMER0
 *
 * Base source code is maked by @hanyazou
 *  https://twitter.com/hanyazou
 *
 * Redesigned by Akihito Honda(Aki.h @akih_san)
 *  https://twitter.com/akih_san
 *  https://github.com/akih-san
 *
 *  Target: MEZW65C02_RAM
 *  Date. 2024.6.21
*/

#define BOARD_DEPENDENT_SOURCE

#include "../w65.h"

// console input buffers
//#define U3B_SIZE 128
unsigned char rx_buf[U3B_SIZE];	//UART Rx ring buffer
unsigned int rx_wp, rx_rp, rx_cnt;

#define rom_org	0xffe7
#define cpu_type 0xfff9

const unsigned char rom[] = {
	0xA9,0x00,      //    lda  #$00
	0xA2,0xFF,      //    ldx  #$ff
	0x9A,           //    txs              ; set SP (W65C02 OPECODE)
	0x1B,           //    tas              ; set SP (W65C816 OPECODE)
	0xEA,           //    nop
	0xEA,           //    nop
	0xBA,           //    tsx              ; SP -> X (W65C02 OPECODE)
	0xE8,           //    inx
	0x8E,0xF9,0xFF, //    stx  cpu_type    ; 0:W65C02 1:W65C816
	0x18,           //    clc              ; set native mode
	0xFB,           //    xce              ; if cpu=W65C02 then xce = nop operation
	0xEA,           //    nop
	0xEA,           //    nop
	0xDB,           //    stp              ; stop CPU
// 0xfff9
	0xFF,           //cpu_type:	db	$FF
	0xF8,0xFF,      //    FDB  NMIBRK		; NMI
	0xE7,0xFF,      //    FDB  RESET		; RESET
	0xF8,0xFF      //    FDB  IRQBRK		; IRQ/BRK
};

// Make IRQ pulse
static void make_irq(void)
{
	CLCSELECT = 1;		// CLC2 select
	G2POL = 1;			// /IRQ = 0 at rising CLK edge
	while(!CLC8OUT){}	// check until IRQ=1
	G2POL = 0;			// /IRQ = 1
	CLCSELECT = 7;		// CLC8 select
	G3POL = 1;			//
	G3POL = 0;			// reset CLC8OUT = 0; release CLC2 reset
}

// ************ timer0 setup ******************
// intervel about 10ms
//  f = 500kHz
// p = 1/f = 1/500KHz = 0.002ms
// 8 bit free running mode
// 256 * 0.002ms = 0.512ms period

// Prescaler Rate Select = 1:2   (b'0001)
//TMR0 Output Postscaler (Divider) Select  1:10 (b'1001)
// target period time = 0.512ms * 10 * 2 = 10.24ms
//
void setup_tomer0(void) {

//	T0CON0 = 0x89;	// timer enable, 8bit mode , 1:10 Postscaler 10ms
//	T0CON0 = 0x80;	// timer enable, 8bit mode , 1:1 Postscaler  1ms
	T0CON0 = 0x84;	// timer enable, 8bit mode , 1:5 Postscaler  5ms
//	T0CON0 = 0x81;	// timer enable, 8bit mode , 1:2 Postscaler  2ms
//	T0CON0 = 0x82;	// timer enable, 8bit mode , 1:2 Postscaler  3ms
//	T0CON0 = 0x83;	// timer enable, 8bit mode , 1:2 Postscaler  3ms
	T0CON1 = 0xa1;	// sorce clk:MFINTOSC (500 kHz), 1:2 Prescaler
	MFOEN = 1;
	TMR0H = 0xff;
	TMR0L = 0x00;
	TMR0IF =0;	// Clear timer0 interrupt flag
	TMR0IE = 1;	// Enable timer0 interrupt
}
//
// define interrupt
//
// Never called, logically
void __interrupt(irq(default),base(8)) Default_ISR(){}

////////////// TIMER0 vector interrupt ////////////////////////////
//TIMER0 interrupt
/////////////////////////////////////////////////////////////////
void __interrupt(irq(TMR0),base(8)) TIMER0_ISR(){

//	if (!irq_flg) {
//		irq_flg = 1;
//		make_irq();
//	}
//	else irq_flg++;

	irq_flg++;

	TMR0IF =0; // Clear timer0 interrupt flag
}

////////////// UART3 Receive interrupt ////////////////////////////
// UART3 Rx interrupt
// PIR9 (bit0:U3RXIF bit1:U3TXIF)
/////////////////////////////////////////////////////////////////
void __interrupt(irq(U3RX),base(8)) URT3Rx_ISR(){

	unsigned char rx_data;

	rx_data = U3RXB;			// get rx data

	if ( rx_data == CTL_Q && fh.nmi_sw ) {
		nmi_sig = 1;
	}
	else if (rx_cnt < U3B_SIZE) {
		rx_buf[rx_wp] = rx_data;
		rx_wp = (rx_wp + 1) & (U3B_SIZE - 1);
		rx_cnt++;
	}
}

void wait_for_programmer()
{
    //
    // Give a chance to use PRC (RB6) and PRD (RB7) to PIC programer.
    //
    printf("\n\r");
    printf("wait for programmer ...\r\n");
    __delay_ms(1000);

    printf("\n\r");
}

// UART3 Transmit
void putch(char c) {
    while(!U3TXIF);			// Wait or Tx interrupt flag set
    U3TXB = c;				// Write data
}

// UART3 Recive ( PIC side )
int getch(void) {
	char c;

	while(!rx_cnt);
	U3RXIE = 0;					// disable Rx interruot
	c = rx_buf[rx_rp];
	rx_rp = (rx_rp + 1) & ( U3B_SIZE - 1);
	rx_cnt--;
	U3RXIE = 1;					// enable Rx interruot
    return c;               // Read data
}

// UART3 Recive( user side )
int u_getch(void) {
	char c;

	while(!rx_cnt) {
		if (nmi_sig) return(0);
	}
	U3RXIE = 0;					// disable Rx interruot
	c = rx_buf[rx_rp];
	rx_rp = (rx_rp + 1) & ( U3B_SIZE - 1);
	rx_cnt--;
	U3RXIE = 1;					// enable Rx interruot
    return c;               // Read data
}

unsigned int get_str(char *buf, uint8_t cnt) {
	unsigned int c, i;
	
	U3RXIE = 0;					// disable Rx interruot
	i = ( (unsigned int)cnt > rx_cnt ) ? rx_cnt : (unsigned int)cnt;
	c = i;
	while(i--) {
		*buf++ = rx_buf[rx_rp];
		rx_rp = (rx_rp + 1) & ( U3B_SIZE - 1);
		rx_cnt--;
	}
	U3RXIE = 1;					// enable Rx interruot
	return c;
}

void reset_cpu(void)
{
	int i;

	// write cpu emulation mode operation program
	bus_hold_req();
	cpu_flg = 0;

	for (;;) {
		write_sram(rom_org, (uint8_t *)rom, sizeof(rom));
		read_sram(rom_org, &tmp_buf[0][0], sizeof(rom));

		if (memcmp(rom, &tmp_buf[0][0], sizeof(rom)) != 0) {
			bus_release_req();
			printf("Memory Write Error\r\n");
			while(1) {}
		}

		start_cpu();

	    __delay_ms(100);

		CLCSELECT = 0;			// CLC1 select
		G2POL = 0;				// /BE = 0 rising CLK edge
		LAT(W65_RESET) = 0;		// cpu reset

	    __delay_ms(100);

		bus_hold_req();

		read_sram(cpu_type, &tmp_buf[0][0], 1);
		switch (tmp_buf[0][0]) {
			case 0:
				cpu_flg = 0;		// CPU : W65C02
				return;
			case 1:
				cpu_flg = 1;		// CPU : W65C816
				return;
		}
		printf("RESET CPU...\r\n");
	}
}

void port_init(void)
{
    // System initialize
    OSCFRQ = 0x08;      // 64MHz internal OSC

	// Disable analog function
    ANSELA = 0x00;
    ANSELB = 0x00;
    ANSELC = 0x00;
    ANSELD = 0x00;
    ANSELE0 = 0;
    ANSELE1 = 0;
    ANSELE2 = 0;

	// /BE output pin
	WPU(W65_BE) = 0;		// disable pull up
	LAT(W65_BE) = 0;		// BUS Hi-z
	TRIS(W65_BE) = 0;		// Set as output
	
    // /RESET output pin
	WPU(W65_RESET) = 0;	// disable pull up
	LAT(W65_RESET) = 0;	// Reset
	TRIS(W65_RESET) = 0;	// Set as output

	// /NMI (RA0)
	WPU(W65_NMI) = 0;	// disable pull up
	LAT(W65_NMI) = 1;		// disable NMI
	TRIS(W65_NMI) = 0;		// Set as output

	WPU(W65_CLK) = 0;	// disable week pull up
	LAT(W65_CLK) = 1;	// init CLK = 1
	TRIS(W65_CLK) = 0;	// set as output pin
	
	// IRQ (RA2)
	WPU(W65_IRQ) = 1;		// Week pull up
	TRIS(W65_IRQ) = 0;		// set as output pin
	LAT(W65_IRQ) = 1;		// IRQ=1

	// RDY (RA5)
	WPU(W65_RDY) = 0;	// disable pull up
	TRIS(W65_RDY) = 1;	// Set as input

	// DCK (RA1)
	WPU(W65_DCK) = 0;	// disable pull up
	LAT(W65_DCK) = 1;	// BANK REG CLK = 1
	TRIS(W65_DCK) = 0;	// Set as output

	// SRAM_R/(/W) (RA4)
	WPU(W65_RW) = 1;	// week pull up
	LAT(W65_RW) = 1;		// SRAM R/(/W) disactive
	TRIS(W65_RW) = 0;		// Set as output

	// Address bus A7-A0 pin
    WPU(W65_ADR_L) = 0xff;	// Week pull up
    LAT(W65_ADR_L) = 0x00;
    TRIS(W65_ADR_L) = 0x00;	// Set as output

	// Address bus A15-A8 pin
	WPU(W65_ADR_H) = 0xff;	// Week pull up
	LAT(W65_ADR_H) = 0x00;
	TRIS(W65_ADR_H) = 0x00;	// Set as output

	// Data bus D7-D0 pin
	WPU(W65_ADBUS) = 0xff;	// Week pull up
	LAT(W65_ADBUS) = 0x00;	// PLD FF init
	TRIS(W65_ADBUS) = 0xff;	// Set as input
	
	// SPI data and clock pins slew at maximum rate

	SLRCON(SPI_SD_PICO) = 0;
	SLRCON(SPI_SD_CLK) = 0;
	SLRCON(SPI_SD_POCI) = 0;

	// SPI_SS
	WPU(SPI_SS) = 1;		// SPI_SS Week pull up
	LAT(SPI_SS) = 1;		// set SPI enable for SD card init
	TRIS(SPI_SS) = 0;		// Set as onput

}

void uart_init(void)
{
	// UART3 initialize
//	U3BRG = 416;	// 9600bps @ 64MHz
//	U3BRG = 208;	// 19200bps @ 64MHz
//	U3BRG = 104;	// 38400bps @ 64MHz
	U3BRG = 34;		// 115200bps @ 64MHz
	U3RXEN = 1;			// Receiver enable
	U3TXEN = 1;			// Transmitter enable

 	// UART3 Receiver
	TRISA7 = 1;			// RX set as input
	U3RXPPS = 0x07;		// RA7->UART3:RXD;

	// UART3 Transmitter
	LATA6 = 1;			// Default level
	TRISA6 = 0;			// TX set as output
	RA6PPS = 0x26;		// UART3:TXD -> RA6;

	U3ON = 1;			// Serial port enable

	rx_wp = 0;
	rx_rp = 0;
	rx_cnt = 0;
	U3RXIE = 1;          // Receiver interrupt enable
}

void clk_init(void)
{
	// Clock(RA3) by NCO FDC mode

	NCO1INC = 65536;		// set 2MHz for reset cpu
	NCO1CLK = 0x00;		// Clock source Fosc
	NCO1PFM = 0;			// FDC mode
	NCO1OUT = 1;			// NCO output enable

	NCO1EN = 1;			// NCO enable
	PPS(W65_CLK) = 0x3f;	// RA3 assign NCO1
}

void reset_clk(void)
{
	NCO1EN = 0;					// NCO disable
	NCO1OUT = 0;					// NCO output disable
	NCO1INC = (__uint24)clk_fs;		// set CLK frequency parameters
	NCO1OUT = 1;					// NCO output enable
	NCO1EN = 1;					// NCO enable
}

void write_sram(uint32_t addr, uint8_t *buf, unsigned int len)
{
    union address_bus_u ab;
    unsigned int i;

	ab.w = addr;
	i = 0;

	if (cpu_flg) {
		// W65C816 native mode
		while( i < len ) {
		    LAT(W65_ADR_L) = ab.ll;
			LAT(W65_ADR_H) = ab.lh;

	        LAT(W65_RW) = 0;			// activate /WE
			TRIS(W65_ADBUS) = 0x00;		// Set as output
			LAT(W65_DCK) = 0;			// bank addres data setup
		    LAT(W65_ADBUS) = ab.hl;		// set bank address to data bas
			LAT(W65_DCK) = 1;			// assert bank address

			LAT(W65_ADBUS) = ((uint8_t*)buf)[i];
	        LAT(W65_RW) = 1;			// deactivate /WE
			TRIS(W65_ADBUS) = 0xff;		// Set as input

			i++;
			ab.w++;
		}
	}
	else {
		// W65C02 mode
		while( i < len ) {
		    LAT(W65_ADR_L) = ab.ll;
			LAT(W65_ADR_H) = ab.lh;

	        LAT(W65_RW) = 0;					// activate /WE
			TRIS(W65_ADBUS) = 0x00;		// Set as output
	        LAT(W65_ADBUS) = ((uint8_t*)buf)[i];
	        LAT(W65_RW) = 1;					// deactivate /WE
			TRIS(W65_ADBUS) = 0xff;		// Set as input

			i++;
			ab.w++;
	    }
	}
}

void read_sram(uint32_t addr, uint8_t *buf, unsigned int len)
{
    union address_bus_u ab;
    unsigned int i;

	ab.w = addr;
	i = 0;

	if (cpu_flg) {
		// W65C816 native mode
		while( i < len ) {
			LAT(W65_ADR_L) = ab.ll;
			LAT(W65_ADR_H) = ab.lh;

			LAT(W65_DCK) = 0;						// Set Bank register
			TRIS(W65_ADBUS) = 0x00;					// Set as output
		    LAT(W65_ADBUS) = ab.hl;
			LAT(W65_DCK) = 1;
			TRIS(W65_ADBUS) = 0xFF;					// Set as input

			ab.w++;									// Ensure bus data setup time from HiZ to valid data
			((uint8_t*)buf)[i] = PORT(W65_ADBUS);			// read data
			i++;
	    }
	}
	else {
		// W65C02 mode
		while( i < len ) {
			LAT(W65_ADR_L) = ab.ll;
			LAT(W65_ADR_H) = ab.lh;

			ab.w++;									// Ensure bus data setup time from HiZ to valid data
			((uint8_t*)buf)[i] = PORT(W65_ADBUS);	// read data
			i++;
	    }
	}
}

void start_cpu(void) {

	bus_release_req();

	CLCSELECT = 0;			// CLC1 select
	G2POL = 0;				// /BE = 0 rising CLK edge
	LAT(W65_RESET) = 0;		// cpu reset

    __delay_ms(100);
	
	G2POL = 1;				// /BE = 1 rising CLK edge

	CLCSELECT = 7;			// CLC8 select
	G3POL = 1;				//
	G3POL = 0;				// reset CLC8OUT = 0; release CLC2 reset

	LAT(W65_RESET) = 1;		// cpu release reset

	irq_flg = 0;
}

void timer_off(void) {
	TMR0IE = 0;	// Disable timer0 interrupt
}

void timer_on(void) {
	TMR0IE = 1;	// enable timer0 interrupt
}
