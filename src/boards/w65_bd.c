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

#include "../../src/w65.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../../drivers/SDCard.h"
#include "../../drivers/picregister.h"

#define SPI_PREFIX      SPI_SD
#define SPI_HW_INST     SPI1
#include "../../drivers/SPI.h"

#define W65_ADBUS		B
#define W65_ADR_L		C
#define W65_ADR_H		D

#define W65_NMI		E1
#define W65_DCK		A1
#define W65_IRQ		A2
#define W65_CLK		A3
#define W65_RW		A4
#define W65_RDY		A5

#define W65_RESET		E0
#define W65_BE			A0

// SPI
#define MISO			B2
#define MOSI			B0
#define SPI_CK			B1
#define SPI_SS			E2

//SD IO
#define SPI_SD_POCI		MISO
#define SPI_SD_PICO		MOSI
#define SPI_SD_CLK		SPI_CK
#define SPI_SD_SS       SPI_SS

static void bus_hold_req(void);
static void bus_release_req(void);
static void reset_ioreq(void);

static uint8_t irq_flg;

	#include "w65_cmn.c"

void clc_init()
{
//
// Setup CLC
//
	//========== CLC1 : make /BE  ==========

	CLCSELECT = 0;		// CLC1 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
    CLCnSEL1 = 127;		// NC
	CLCnSEL2 = 127;		// NC
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x02;	// NCO1 -> lcxg1
	CLCnGLS1 = 0x00;	// 0 -> lcxg2(DFF D)
    CLCnGLS2 = 0x00;	// 0 -> lcxg3(DFF R)
    CLCnGLS3 = 0x00;	// 0 -> lcxg4(DFF S)

    CLCnPOL = 0x00;		// POL=0: CLC1OUT = DFF Q
    CLCnCON = 0x84;		// 1-Input D Flip-Flop with S and R

	// Release wait (D-FF reset)
	// init DFF Q = 0 : Bus Hi-z

	G3POL = 1;
	G3POL = 0;

	PPS(W65_BE) = 0x01;	// output:CLC1OUT->/BE(RA0)

	//========== CLC2 : make IRQ  ==========

	CLCSELECT = 1;		// CLC2 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
	CLCnSEL1 = 0x38;	// CLC6OUT
	CLCnSEL2 = 0x3a;	// CLC8OUT : for reset DFF
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x02;	// NCO1 -> lcxg1
	CLCnGLS1 = 0x08;	// CLC6OUT -> lcxg2(DFF D)
	CLCnGLS2 = 0x20;	// CLC8OUT -> lcxg3(DFF R)
    CLCnGLS3 = 0x00;	// 0 -> lcxg4(DFF S)

    CLCnPOL = 0x80;		// POL=1: CLC2OUT = not DFF Q
    CLCnCON = 0x84;		// 1-Input D Flip-Flop with S and R

	//========== CLC3 : 1:shift /CLC2OUT  ==========

	CLCSELECT = 2;		// CLC3 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
    CLCnSEL1 = 0x34;	// CLC2OUT
	CLCnSEL2 = 0x3a;	// CLC8OUT : for reset DFF
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x02;	// NCO1 -> lcxg1
	CLCnGLS1 = 0x04;	// not CLC2OUT -> lcxg2(DFF D)
	CLCnGLS2 = 0x20;	// CLC8OUT -> lcxg3(DFF R)
    CLCnGLS3 = 0x00;	// 0 -> lcxg4(DFF S)

    CLCnPOL = 0x00;		// all POL=0
    CLCnCON = 0x84;		// 1-Input D Flip-Flop with S and R

	G3POL = 1;
	G3POL = 0;

	//========== CLC4 : 2:shift CLC3OUT  ==========

	CLCSELECT = 3;		// CLC4 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
    CLCnSEL1 = 0x35;	// CLC3OUT
	CLCnSEL2 = 0x3a;	// CLC8OUT : for reset DFF
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x02;	// NCO1 -> lcxg1
	CLCnGLS1 = 0x08;	// CLC3OUT -> lcxg2(DFF D)
	CLCnGLS2 = 0x20;	// CLC8OUT -> lcxg3(DFF R)
    CLCnGLS3 = 0x00;	// 0 -> lcxg4(DFF S)

    CLCnPOL = 0x00;		// all POL=0
    CLCnCON = 0x84;		// 1-Input D Flip-Flop with S and R

	G3POL = 1;
	G3POL = 0;

	//========== CLC5 : 3:shift CLC4OUT  ==========

	CLCSELECT = 4;		// CLC5 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
    CLCnSEL1 = 0x36;	// CLC4OUT
	CLCnSEL2 = 0x3a;	// CLC8OUT : for reset DFF
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x02;	// NCO1 -> lcxg1
	CLCnGLS1 = 0x08;	// CLC4OUT -> lcxg2(DFF D)
	CLCnGLS2 = 0x20;	// CLC8OUT -> lcxg3(DFF R)
    CLCnGLS3 = 0x00;	// 0 -> lcxg4(DFF S)

    CLCnPOL = 0x00;		// all POL=0
    CLCnCON = 0x84;		// 1-Input D Flip-Flop with S and R

	G3POL = 1;
	G3POL = 0;

	//========== CLC6 : 4:shift CLC5OUT  ==========

	CLCSELECT = 5;		// CLC6 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
    CLCnSEL1 = 0x37;	// CLC5OUT
	CLCnSEL2 = 0x3a;	// CLC8OUT : for reset DFF
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x02;	// NCO1 -> lcxg1
	CLCnGLS1 = 0x08;	// CLC5OUT -> lcxg2(DFF D)
	CLCnGLS2 = 0x20;	// CLC8OUT -> lcxg3(DFF R)
    CLCnGLS3 = 0x00;	// 0 -> lcxg4(DFF S)

    CLCnPOL = 0x00;		// all POL=0
    CLCnCON = 0x84;		// 1-Input D Flip-Flop with S and R

	G3POL = 1;
	G3POL = 0;

	//========== CLC7 : CLC5OUT and CLC6OUT ==========

	CLCSELECT = 6;		// CLC7 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
	CLCnSEL1 = 0x38;	// CLC6OUT
	CLCnSEL2 = 127;		// NC
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x02;	// NCO1 -> lcxg1
	CLCnGLS1 = 0x08;	// CLC6OUT -> lcxg2
    CLCnGLS2 = 0x00;	// 0 -> lcxg3
    CLCnGLS3 = 0x00;	// 0 -> lcxg4

    CLCnPOL = 0x00;		// all POL=0
    CLCnCON = 0x84;		// 1-Input D Flip-Flop with S and R

	G3POL = 1;
	G3POL = 0;

	//========== CLC8 : hold CLC7OUT ==========

	CLCSELECT = 7;		// CLC8 select

	CLCnSEL0 = 0x2a;	// NCO1 : CLK
	CLCnSEL1 = 0x39;	// CLC7OUT
	CLCnSEL2 = 0x3a;	// CLC8OUT
	CLCnSEL3 = 127;		// NC

    CLCnGLS0 = 0x01;	// not NCO1 -> lcxg1
	CLCnGLS1 = 0x08;	// CLC7OUT -> lcxg2(DFF D2)
    CLCnGLS2 = 0x00;	// 0 -> lcxg3(DFF R)
    CLCnGLS3 = 0x20;	// CLC8OUT -> lcxg4(DFF D1)

    CLCnPOL = 0x00;		// all POL=0
    CLCnCON = 0x85;		// 2-Input D Flip-Flop with R

	G3POL = 1;
	G3POL = 0;

	// Release /IRQ OUTPUT
	CLCSELECT = 1;		// CLC2 select

	G3POL = 1;
	G3POL = 0;

	PPS(W65_IRQ) = 0x02;	// output:CLC2OUT->/IRQ(RA2)

	irq_flg = 0;
}

void setup_sd(void) {
    //
    // Initialize SD Card
    //
    static int retry;

	for (retry = 0; 1; retry++) {
        if (20 <= retry) {
            printf("No SD Card?\n\r");
            while(1);
        }
//        if (SDCard_init(SPI_CLOCK_100KHZ, SPI_CLOCK_2MHZ, /* timeout */ 100) == SDCARD_SUCCESS)
        if (SDCard_init(SPI_CLOCK_100KHZ, SPI_CLOCK_4MHZ, /* timeout */ 100) == SDCARD_SUCCESS)
//        if (SDCard_init(SPI_CLOCK_100KHZ, SPI_CLOCK_8MHZ, /* timeout */ 100) == SDCARD_SUCCESS)
            break;
        __delay_ms(200);
    }
}

void start_W65(void)
{
	bus_release_req();

	// Unlock IVT
	IVTLOCK = 0x55;
	IVTLOCK = 0xAA;
	IVTLOCKbits.IVTLOCKED = 0x00;

	// Default IVT base address
	IVTBASE = 0x000008;

	// Lock IVT
	IVTLOCK = 0x55;
	IVTLOCK = 0xAA;
	IVTLOCKbits.IVTLOCKED = 0x01;

	// release /BE
	CLCSELECT = 0;		// CLC1 select
	G2POL = 1;			// /BE = 1 rising CLK edge

	// W65 start
	LAT(W65_RESET) = 1;	// Release reset
}

static void bus_hold_req(void) {
	// Set address bus as output
	TRIS(W65_ADR_L) = 0x00;	// A7-A0
	TRIS(W65_ADR_H) = 0x00;	// A8-A15

	LAT(W65_RW) = 1;			// SRAM READ mode
	TRIS(W65_RW) = 0;			// output
	TRIS(W65_DCK) = 0;		// Set as output
}

static void bus_release_req(void) {
	// Set address bus as input
	TRIS(W65_ADR_L) = 0xff;	// A7-A0
	TRIS(W65_ADR_H) = 0xff;	// A8-A15

	TRIS(W65_DCK) = 1;		// Set as input
	TRIS(W65_RW) = 1;			// input
}

//-----------------------------
// event loop ( PIC MAIN LOOP )
//-----------------------------
void board_event_loop(void) {

	for (;;) {
		while (R(W65_RDY)) {
			if ( irq_flg >= 1 ) {
				make_irq();
				GIE = 0;	// Disable interrupt
				irq_flg = 0;
				GIE = 1;	// enable interrupt
			}
		}
		// BUS -> Hi-z
		CLCSELECT = 0;			// CLC1 select
		G2POL = 0;				// /BE = 0 rising CLK edge
		bus_hold_req();				// PIC becomes a busmaster
		bus_master_operation();
		if(wup_flg) return;		// abort event loop. Return to main()
		bus_release_req();
		// Release BUS
		G2POL = 1;				// /BE = 1 rising CLK edge

		CLCSELECT = 1;			// CLC2 select
		G2POL = 1;				// /IRQ = 0 at rising CLK edge
		while(!CLC8OUT){}			// check until IRQ=1
		G2POL = 0;				// /IRQ = 1
		CLCSELECT = 7;			// CLC8 select
		G3POL = 1;				//
		G3POL = 0;				// reset CLC8OUT = 0; release CLC2 reset

		GIE = 0;				// Disable interrupt
		irq_flg = 0;			// release mask timer interrupt
		GIE = 1;				// enable interrupt
		
		if (nmi_sig) {
			LAT(W65_NMI) = 0;
			LAT(W65_NMI) = 1;
			nmi_sig = 0;
		}
	}
}

void board_event_loop1(void) {

	for (;;) {
		while (R(W65_RDY)) {}
		// BUS -> Hi-z
		CLCSELECT = 0;			// CLC1 select
		G2POL = 0;				// /BE = 0 rising CLK edge
		bus_hold_req();				// PIC becomes a busmaster
		bus_master_operation();
		if(wup_flg) return;		// abort event loop. Return to main()
		bus_release_req();
		// Release BUS
		G2POL = 1;				// /BE = 1 rising CLK edge

		CLCSELECT = 1;			// CLC2 select
		G2POL = 1;				// /IRQ = 0 at rising CLK edge
		while(!CLC8OUT){}			// check until IRQ=1
		G2POL = 0;				// /IRQ = 1
		CLCSELECT = 7;			// CLC8 select
		G3POL = 1;				//
		G3POL = 0;				// reset CLC8OUT = 0; release CLC2 reset

		if (nmi_sig) {
			LAT(W65_NMI) = 0;
			LAT(W65_NMI) = 1;
			nmi_sig = 0;
		}
	}
}

void continue_action(void) {

	CLCSELECT = 0;			// CLC1 select
	bus_release_req();
	// Release BUS
	G2POL = 1;				// /BE = 1 rising CLK edge

	CLCSELECT = 1;			// CLC2 select
	G2POL = 1;				// /IRQ = 0 at rising CLK edge
	while(!CLC8OUT){}			// check until IRQ=1
	G2POL = 0;				// /IRQ = 1
	CLCSELECT = 7;			// CLC8 select
	G3POL = 1;				//
	G3POL = 0;				// reset CLC8OUT = 0; release CLC2 reset

	if( fh.irq_sw ) {
		GIE = 0;				// Disable interrupt
		irq_flg = 0;			// release mask timer interrupt
		GIE = 1;				// enable interrupt
	}
}

#include "../../drivers/pic18f57q43_spi.c"
#include "../../drivers/SDCard.c"

