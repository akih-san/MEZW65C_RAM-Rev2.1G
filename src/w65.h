/* *
 *  Target: MEZW65C_RAM 512KB Rev2.0
 *  Written by Akihito Honda (Aki.h @akih_san)
 *  https://twitter.com/akih_san
 *  https://github.com/akih-san
 *
 *  Date. 2024.10.23
 */

#ifndef __SUPERMEZ80_H__
#define __SUPERMEZ80_H__

#include "../src/picconfig.h"
#include <xc.h>
#include <stdint.h>
#include "../fatfs/ff.h"

//
// Configlations
//

#define P64 15.625

#define ENABLE_DISK_DEBUG

#define TMP_BUF_SIZE     256
#define U3B_SIZE 128

#define MEM_CHECK_UNIT	TMP_BUF_SIZE * 16	// 4 KB
#define MAX_MEM_SIZE	0x00080000			// 512KB
extern uint32_t bioreq_ubuffadr;
extern uint32_t bioreq_cbuffadr;
extern uint32_t clk_fs;
extern void reassign_clk(void);

#define TIMER0_INITC	0x87e1	//Actual value
#define TIMER0_INITCH	0x87
#define TIMER0_INITCL	0xe1
//
// Constant value definitions
//

#define CTL_Q 0x11

//
// Type definitions
//

//;--------- MEZW65C_RAM file header --------------------------
//
typedef struct {

	/*
	*	*** NOTE***
	*
	*	When bios_sw=1, the following parameters are not used and are reserved.
	*
	*		picif_p
	*		irq_sw
	*		reg_tblp
	*		reg_tsize
	*		nmi_sw
	*/
	uint8_t		op1;			// if bios_sw=1, PROGRAM BANK:(W65C816), 0:(W65C02)
								// if bios_sw=0, JMP opecode
	uint16_t	cstart_addr;
	uint8_t		op2;			// if bios_sw=1, DATA BANK:(W65C816), 0:(W65C02)
								// if bios_sw=0, JMP opecode
	uint16_t	wstart_addr;
	uint16_t	direct_page;	// DIRECT PAGE : (W65C816), reserve : (W65C02)
	uint8_t		mezID[8];		// Unique ID "MEZW65C",0 (This area is used by monitor for invoking bios_call program)
	uint32_t	load_p;			// Load program address 24bit:(W65C816), 16bit:(W65C02)
	uint32_t	picif_p;		// pic i/o shared memory address
	uint8_t		sw_816;			// 0 : W65C02, 1: W65C816 native mode, 2:works in both modes
	uint8_t		irq_sw;			// 0 : no use IRQ console I/O
								// 1 : use IRQ timer interrupt driven console I/O
	uint16_t	reg_tblp;		// register save pointer( NMI )
	uint16_t	reg_tsize;		// register table size
	uint8_t		nmi_sw;			// 0 : No NMI support, 1: NMI support
	uint8_t		bios_sw;		// 0 : standalone program
								// 1 : user program (bios call program or DOS65)
								// 2 : monitor program( .SYS file)
} file_header;
//;--------- end MEZW65C_RAM file header --------------------------

#define mezID_off 8

typedef struct {
	uint16_t REGA;		//ds 2 ; Accumulator A
	uint16_t REGX;		//ds 2 ; Index register X
	uint16_t REGY;		//ds 2 ; Index register Y
	uint16_t REGSP;		//ds 2 ; Stack pointer SP
	uint16_t REGPC;		//ds 2 ; Program counter PC
	uint8_t  REGPSR;	//ds 1 ; Processor status register PSR
	uint8_t  REGPB;		//ds 1 ; Program Bank register
	uint8_t  REGDB;		//ds 1 ; Data Bank register
	uint16_t REGDP;		//ds 2 ; Direct Page register
} reg816;

typedef struct {
	uint8_t  REGA;		//ds 1 ; Accumulator A
	uint8_t  REGX;		//ds 1 ; Index register X
	uint8_t  REGY;		//ds 1 ; Index register Y
	uint8_t  REGSP;		//ds 1 ; Stack pointer SP
	uint16_t REGPC;		//ds 2 ; Program counter PC
	uint8_t  REGPSR;	//ds 1 ; Processor status register PSR
} reg02;

// Reqest Parameter Block(15 bytes)
typedef struct {
//-- monitor I/F block ------------------------------------------------------
	uint8_t  UREQ_COM;		// unimon CONIN/CONOUT request command
	uint8_t  UNI_CHR;		// charcter (CONIN/CONOUT)
//-- user I/F block ---------------------------------------------------------
	uint8_t   CREQ_COM;	// unimon CONIN/CONOUT request command
	uint8_t   CBI_CHR;		// charcter (CONIN/CONOUT) or number of strings
	uint8_t   disk_drive;
	uint32_t  disk_lba;		// LBA(Logical Block address) Lower 16 bits valid, Upper 16 bits are always 0
	uint16_t  data_adr;		// data buffer addres
	uint8_t   reserve;		// 24bit addressing foe 65816
	uint8_t  data_cnt;		//
} crq_hdr;

// Address Bus
union address_bus_u {
    uint32_t w;             // 32 bits Address
    struct {
        uint8_t ll;        // Address L low
        uint8_t lh;        // Address L high
        uint8_t hl;        // Address H low
        uint8_t hh;        // Address H high
    };
};

typedef struct {
    int exist;
    FIL *filep;
} drive_t;

typedef struct {
    uint8_t disk;
    uint8_t disk_read;
    uint8_t disk_write;
    uint8_t disk_verbose;
    uint16_t disk_mask;
} debug_t;

typedef struct {
    uint8_t *addr;
    uint16_t offs;
    unsigned int len;
} mem_region_t;

typedef struct {
	uint8_t  cmd_len;		// LENGTH OF THIS COMMAND
	uint8_t  unit;			// SUB UNIT SPECIFIER
	uint8_t  cmd;			// COMMAND CODE
	uint16_t status;		// STATUS
	uint8_t  reserve[8];	// RESERVE
	uint8_t  media;			// MEDIA DESCRIPTOR
	uint16_t trans_off;		// TRANSFER OFFSET
	uint16_t trans_seg;		// TRANSFER SEG
	uint16_t count;			// COUNT OF BLOCKS OR CHARACTERS
	uint16_t start;			// FIRST BLOCK TO TRANSFER
} iodat;

typedef struct {
	uint8_t  cmd_len;		// LENGTH OF THIS COMMAND
	uint8_t  unit;			// SUB UNIT SPECIFIER
	uint8_t  cmd;			// COMMAND CODE
	uint16_t status;		// STATUS
	uint8_t  reserve[8];	// RESERVE
	uint8_t  bpb1;			// number of support drives.
	uint16_t bpb2_off;		// DWORD transfer address.
	uint16_t bpb2_seg;
	uint16_t bpb3_off;		// DWORD pointer to BPB
	uint16_t bpb3_seg;
	uint8_t  bdev_no;		// block device No.
} CMDP;

typedef struct {
	uint8_t  UREQ_COM;		// unimon CONIN/CONOUT request command
	uint8_t  UNI_CHR;		// charcter (CONIN/CONOUT) or number of strings
	uint16_t STR_off;		// unimon string offset
	uint16_t STR_SEG;		// unimon string segment
	uint8_t  DREQ_COM;		// device request command
	uint8_t  DEV_RES;		// reserve
	uint16_t PTRSAV_off;	// request header offset
	uint16_t PTRSAV_SEG;	// request header segment
} PTRSAV;


typedef struct {
	uint8_t  jmp_ner[3];	// Jmp Near xxxx  for boot.
	uint8_t  mane_var[8];	// Name / Version of OS.
} DPB_HEAD;

typedef struct {
	DPB_HEAD reserve;
//-------  Start of Drive Parameter Block.
	uint16_t sec_size;		// Sector size in bytes.                  (dpb)
	uint8_t  alloc;			// Number of sectors per alloc. block.    (dpb)
	uint16_t res_sec;		// Reserved sectors.                      (dpb)
	uint8_t  fats;			// Number of FAT's.                       (dpb)
	uint16_t max_dir;		// Number of root directory entries.      (dpb)
	uint16_t sectors;		// Number of sectors per diskette.        (dpb)
	uint8_t  media_id;		// Media byte ID.                         (dpb)
	uint16_t fat_sec;		// Number of FAT Sectors.                 (dpb)
//-------  End of Drive Parameter Block.
	uint16_t sec_trk;		// Number of Sectors per track.
} DPB;

typedef struct {
	uint8_t  cmd_len;		// LENGTH OF THIS COMMAND
	uint8_t  unit;			// SUB UNIT SPECIFIER
	uint8_t  cmd;			// COMMAND CODE
	uint16_t status;		// STATUS
	uint8_t  reserve[8];	// RESERVE
	uint8_t  medias1;		//Media byte.
	uint8_t  medias2;		//Media status byte flag.
} MEDIAS;

typedef struct {
	uint8_t  cmd_len;		// LENGTH OF THIS COMMAND
	uint8_t  unit;			// SUB UNIT SPECIFIER
	uint8_t  cmd;			// COMMAND CODE
	uint16_t status;		// STATUS
	uint8_t  reserve[8];	// RESERVE
	uint8_t  media;			// MEDIA DESCRIPTOR
	uint16_t bpb2_off;		// DWORD transfer address.
	uint16_t bpb2_seg;
	uint16_t bpb3_off;		// DWORD pointer to BPB
	uint16_t bpb3_seg;
} BPB;

typedef struct {
	uint16_t TIM_DAYS;		//Number of days since 1-01-1980.
	uint8_t  TIM_MINS;		//Minutes.
	uint8_t  TIM_HRS;		//Hours.
	uint8_t  TIM_HSEC;		//Hundreths of a second.
	uint8_t  TIM_SECS;		//Seconds.
} TPB;

#define TIM20240101	16071	// 16071days from 1980

//I2C
//General Call Address
#define GeneralCallAddr	0
#define module_reset	0x06
#define module_flash	0x0e

//DS1307 Slave address << 1 + R/~W
#define DS1307			0b11010000	// support RTC module client address

//FT200XD Slave address << 1 + R/~W
#define FT200XD			0b01000100	// support USB I2C module client address

#define BUS_NOT_FREE	1
#define NACK_DETECT		2
#define NUM_DRIVES		8
//
// Global variables and function prototypes
//
extern file_header fh;
extern uint8_t	nmi_sig;
extern	uint8_t	wup_flg;
extern uint8_t	cpu_flg;
extern uint8_t tmp_buf[2][TMP_BUF_SIZE];
extern uint8_t cin_no;	// console No.
extern debug_t debug;

extern uint16_t chk_i2cdev(void);
extern void setup_clk_aux(void);
extern void dosio_init(void);
extern drive_t drives[];
extern uint32_t mem_init(void);
extern unsigned int get_str(char *buf, uint8_t cnt);

extern void write_sram(uint32_t addr, uint8_t *buf, unsigned int len);
extern void read_sram(uint32_t addr, uint8_t *buf, unsigned int len);
extern void board_event_loop(void);
extern void board_event_loop1(void);
extern void bus_master_operation(void);
extern void continue_action(void);
extern void start_cpu(void);
extern uint32_t get_hex(char *);
extern int (*get_char[])(void);
extern unsigned int (*strin_func[])(char *, uint8_t);
extern int u_getch(void);

//
// debug macros
//
#ifdef ENABLE_DISK_DEBUG
#define DEBUG_DISK (debug.disk || debug.disk_read || debug.disk_write || debug.disk_verbose)
#define DEBUG_DISK_READ (debug.disk_read)
#define DEBUG_DISK_WRITE (debug.disk_write)
#define DEBUG_DISK_VERBOSE (debug.disk_verbose)
#else
#define DEBUG_DISK 0
#define DEBUG_READ 0
#define DEBUG_WRITE 0
#define DEBUG_DISK_VERBOSE 0
#endif

extern unsigned char rx_buf[];				//UART Rx ring buffer
extern unsigned int rx_wp, rx_rp, rx_cnt;

// AUX: input buffers
extern unsigned char ax_buf[];				//UART Rx ring buffer
extern unsigned int ax_wp, ax_rp, ax_cnt;
extern void putax(char c);
extern int getax(void);

extern void reset_cpu(void);
extern void port_init(void);
extern void uart_init(void);
extern void clc_init(void);
extern void clk_init(void);
extern void reset_clk(void);
extern void start_W65(void);
extern void setup_sd(void);
extern void wait_for_programmer(void);
extern void setup_tomer0(void);
extern void timer_off(void);
extern void timer_on(void);

extern uint32_t mem_test(void);

#endif  // __SUPERMEZ80_H__
