/*
 * Based on main.c by Tetsuya Suzuki 
 * and emuz80_z80ram.c by Satoshi Okue
 * PIC18F47Q43/PIC18F47Q83/PIC18F47Q84 ROM image uploader
 * and UART emulation firmware.
 * This single source file contains all code.
 *
 * Base source code of this firmware is maked by
 * @hanyazou (https://twitter.com/hanyazou) *
 *
 *  Target: MEZW65C02_RAM
 *  Written by Akihito Honda (Aki.h @akih_san)
 *  https://x.com/akih_san
 *  https://github.com/akih-san
 *
 *  Date. 2024.8.30
 */

#define INCLUDE_PIC_PRAGMA
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../src/w65.h"
#include "../drivers/utils.h"

/* Previous function declaration */
static int disk_init(void);
static int load_program(uint8_t * );
static uint8_t del_space(char *);
static int load_config(void);
static int get_line(char *, int);

static FRESULT scan_files(void);
static FRESULT scan_files1(void);
static FRESULT change_directory(void);
static FRESULT load_file(void);
static FRESULT mon_prog(void);
static FRESULT wstart_prog(void);
static FRESULT restart_prog(void);
static FRESULT return_cpu(void);
static FRESULT print_reg(void);
static FRESULT print_com(void);
static FRESULT boot_file(void);
static FRESULT mem_dump(void);
static FRESULT drive_cpu(void);
static FRESULT see_file(void);
static FRESULT flash_apl(void);
static FRESULT open_dos65(void);
static FRESULT close_dos65(void);

static void set_arg(char *);
static int setup_monitor(void);
static int in_file(void);
static unsigned int str_inf(char *, uint8_t);

#define BS	0x08
#define CR	0x0d
#define BUF_SIZE TMP_BUF_SIZE * 2

#define INPUT_UART	0
#define INPUT_FILE	1

debug_t debug = {
    0,  // disk
    0,  // disk_read
    0,  // disk_write
    0,  // disk_verbose
    0,  // disk_mask
};

/* define structure type */
typedef struct {
	const TCHAR *conf;
	uint32_t *val;
} sys_param;

typedef struct {
	char *cmd_name;
	FRESULT (*func)(void);
} com_param;

static const char *mezID = "MEZW65C";
static char *board_name = "Mezzanine Card MEZW65C_RAM firmware Rev2.1_PLD";
static const TCHAR *conf02 = "MEZW02.CFG";
static const TCHAR *conf16 = "MEZW16.CFG";
static char *mon02 = "/MON02.SYS";
static char *mon16 = "/MON16.SYS";

static char *dosdir	= "DOS_DISK";
static char *dos65 = "/DOS65.SYS";

/* global values */
drive_t drives[NUM_DRIVES];

file_header fh;

uint8_t tmp_buf[2][TMP_BUF_SIZE];

int (*get_char[2])(void) = {
	u_getch,
	in_file
};

unsigned int (*strin_func[2])(char *, uint8_t) = {
	get_str,
	str_inf
};

uint8_t cin_no;			// 0 : INPUT_UART 1 : INPUT_FILE

uint32_t clk_fs;
uint32_t bioreq_ubuffadr;
uint32_t bioreq_cbuffadr;

// CPU flg 0:W65C02 1:W65C816
uint8_t	cpu_flg;
uint8_t	wup_flg;
uint8_t	nmi_sig;

/* local values */
static uint16_t frd_ptr;		// read pointer : cin_file[frd_ptr]
static uint16_t fin_cnt;
static uint16_t fin_size;
static char fin_name[13];		// redirect file name

static FATFS fs;
static FILINFO fileinfo;
static FIL rom_fl;
static FIL in_fl;
static FIL files[NUM_DRIVES];

static uint32_t raw_addr;
static uint16_t file_size;
static uint8_t load_flg;

#define line_size 81
static char line_buf[line_size];

#define arg_num 5
static char *arg[arg_num];

#define num_param 2
#define num_com 15
//#define num_com 14
static sys_param t_conf[num_param] = {
	{"CLK_INC", &clk_fs},
	{"REQ_HDR", &bioreq_ubuffadr}
};

static com_param cmd[num_com+1] = {
	{"LS", scan_files},
	{"DIR",scan_files1},
	{"CD",change_directory},
	{"LOAD",load_file},
	{"CSTART",restart_prog},
	{"WSTART",wstart_prog},
	{"RETI",return_cpu},
	{"REG",print_reg},
	{"MONITOR", mon_prog},
	{"MDUMP",mem_dump},
	{"SHOW",see_file},
	{"FLASH",flash_apl},
	{"DOS65",open_dos65},
	{"HELP",print_com},
	{"?",print_com},
	{"",boot_file}
};

#define BINV_SIZE 7
typedef struct {
//---transfer data to monitor mezID area---
	uint8_t		sw;		// +0 0: user program none 
						//    1: user program exist
	uint16_t	addr;	// +1 (copied caddr or waddr)
	uint8_t		pbnk;	// +3 PBR
	uint8_t		dbnk;	// +4 DBR
	uint16_t	dp;		// +5 DPR
//----------------------------------------
	uint16_t	caddr;	// user program cold start
	uint16_t	waddr;	// user program warm start
	uint8_t		sw_816;	// 0 : W65C02, 1: W65C816 native mode, 2:works in both modes
	TCHAR	fname[30];	// BIOS program faile name
} bios_inv;

static bios_inv binv;
static uint8_t mon;

// main routine

void main(void)
{
	int c, fr;
	char *buf, *res;
	FRESULT rs;
	
	cin_no	= INPUT_UART;	// input console (UART)
	wup_flg = 0;	// clear wakeup flag for NMI
	nmi_sig = 0;
	fh.nmi_sw = 0;
	raw_addr = 0;
	load_flg = 0;
	binv.sw = 0;

	port_init();
	uart_init();
	wait_for_programmer();

	setup_sd();
	if (disk_init() < 0) while (1);

	clk_init();		// initial CLK = 2MHz
	clc_init();

	reset_cpu();

	// memory test
	mem_init();

	if( load_config() < 0) while (1);
	printf("\r\nUse NCO1 %2.3fMHz\r\n\n",(double)clk_fs * 30.5175781 / 1000000);
	
	if (!cpu_flg) printf("CPU : W65C02\r\n");
	else printf("CPU : W65C816\r\n");

	reset_clk();
	clc_init();

	// Global interrupt enable
	GIE = 1;
	// command input

	if ( setup_monitor() ) {
		printf("System stop!!");
		while(1) {}
	}
		
	printf("\r\nBoard: %s\n\r", board_name);

	/* set root directory */
	f_getcwd(&line_buf[0], line_size);
	arg[0] = "";
	scan_files();
	
	for(;;) {
		buf = (char *)&tmp_buf[0][0];
		f_getcwd(buf, line_size);
		printf("%s/ ", buf);									/* pirnt prompt */
		for(c = 0; c < arg_num; c++) arg[c] = 0;

		if (!get_line(&line_buf[0], line_size)) continue;		/* console input */
		/* search command */
		for( c=0;  c < num_com; c++ ) {
			if ((res = strstr(&line_buf[0], cmd[c].cmd_name ))) {
				buf = (char *)(res+strlen(cmd[c].cmd_name));		/* buf = next point */
				set_arg(buf);
				break;
			}
		}
		// do command operation
		if ( c == num_com ) set_arg(&line_buf[0]);
		rs = (*cmd[c].func)();								/* call cmd function */

		if( rs == (FRESULT)-2 ) printf("\r\nAssert NMI interrupt.\r\n");
		else if ( rs != FR_OK ) printf("Invalid( command | file | directory )\r\n");
	}
	
}

//
// setup monitor at starting MESW65C_RAM
//
static int setup_monitor(void) {

	int	rs;
	uint8_t dat;
	
	printf("Install Monitor Program..........\r\n");

	if (cpu_flg) {
		arg[0] = mon16;
		mon = 1;
	}
	else {
		arg[0] = mon02;
		mon = 0;
	}
	rs = load_program((uint8_t *)arg[0]);
	if ( rs ) return rs;

	//
	// Start CPU
	//
	start_W65();
	setup_tomer0();		//Start IRQ timer
	drive_cpu();
	return 0;
}

//
// load program from SD card
//
static int load_program(uint8_t *fname) {
	
	FRESULT	fr;
	void	*rdbuf;
	UINT	btr, br;
	uint16_t	cnt;
	uint32_t	adr, adr0;
	file_header *header;
	
	rdbuf = (void *)&tmp_buf[0][0];		// program load work area(512byte)
	header = (file_header *)&tmp_buf[0][0];

	fr = f_open(&rom_fl, (TCHAR *)fname, FA_READ);
	if ( fr != FR_OK ) {
		printf("File Open Error.\r\n");
		return((int)fr);
	}
	adr = 0;
	cnt = file_size = (uint16_t)f_size(&rom_fl);				// get file size
	btr = BUF_SIZE;								// default 512byte
	while( cnt ) {
		fr = f_read(&rom_fl, rdbuf, btr, &br);
		if (fr == FR_OK) {
			if ( !adr ) {
				if ( raw_addr ) {
					adr = adr0 = raw_addr;
				}
				else {
					if (!strstr((const char *)header->mezID, mezID)) {
						printf("Invalid MEZW65C file.\r\n");
						fr = FR_INVALID_OBJECT;
						break;
					}
					if (header->sw_816==1 && !cpu_flg) {
						printf("Work on only W65C816..\r\n");
						fr = FR_INVALID_OBJECT;
						break;
					}
					adr = adr0 = header->load_p;
					if (!load_flg) {
						if ( header->bios_sw == 1 ) {	// file is user program
							binv.sw = header->bios_sw;
							binv.caddr = header->cstart_addr;
							binv.waddr = header->wstart_addr;
							binv.dp = header->direct_page;			//direct page
							binv.pbnk = header->op1;				//program bank
							binv.dbnk = header->op2;				//data bank
							binv.sw_816 = header->sw_816;
							sprintf((char *)binv.fname, "%s", fname);
						}
						else {
							// copy file header */
							fh.bios_sw = header->bios_sw;
							fh.load_p = header->load_p;
							fh.sw_816 = header->sw_816;
							fh.cstart_addr = header->cstart_addr;
							fh.wstart_addr = header->wstart_addr;
							fh.picif_p = header->picif_p;
							fh.irq_sw = header->irq_sw;
							fh.reg_tblp = header->reg_tblp;
							fh.reg_tsize = header->reg_tsize;
							fh.nmi_sw = header->nmi_sw;
							/* PIC common memory address */
							bioreq_ubuffadr = fh.picif_p;
							bioreq_cbuffadr = bioreq_ubuffadr + 2;
							if ( !fh.bios_sw ) binv.sw = 0;		// standalone(clear bios call flag)
						}
					}
				}
			}
			write_sram(adr, (uint8_t *)rdbuf, (unsigned int)br);
			adr += (uint32_t)br;
			cnt -= (uint16_t)br;
			if (btr > (UINT)cnt) btr = (UINT)cnt;
		}
		else break;
	}
	if (fr == FR_OK) {
		printf("Load %s : Adr = $%06lX, Size = $%04X\r\n", fname, adr0, file_size);
	}
	else {
		if (fr != FR_INVALID_OBJECT) printf("File Load Error.\r\n");
	}
	f_close(&rom_fl);
	return((int)fr);
}

//
// mount SD card
//
static int disk_init(void)
{
    if (f_mount(&fs, "0://", 1) != FR_OK) {
        printf("Failed to mount SD Card.\n\r");
        return -2;
    }

    return 0;
}

static uint8_t del_space(char *bytes) {
	uint8_t pos = 0;
	uint8_t i = 0;
	char c;
	
	while( (c = bytes[i++]) ) {
		if (c == '\r' || c == '\n' || c == ' ') {
			continue;
		}
		bytes[pos++] = c;
	}
	bytes[pos] = c;		// save NULL code
	return pos;
}

static int load_config(void)
{
	FRESULT	fr;
	char *buf, *a;
	const TCHAR *conf;
	uint16_t cnt, size;
	uint16_t adr;
	int i;
	TCHAR *str;
	
	str = (TCHAR *)&tmp_buf[0][0];
	conf = ( cpu_flg ) ? conf16 : conf02;
	
	printf("\r\nLoad %s\r\n",(const char *)conf);
	
	fr = f_open(&rom_fl, conf, FA_READ);
	if ( fr != FR_OK ) {
		printf("%s not found..\r\n", conf);
		return -1;
	}

	while ( f_gets(str, 80, &rom_fl) ) {	// max 80 characters

		// delete space
		del_space(str);

		if (str[0] == ';' || str[0] == 0 ) continue;

		// search keywoard
		for( i=0; i<num_param; i++ ) {
			if ( !strstr(str, t_conf[i].conf )) continue;
			if(str[strlen(t_conf[i].conf)] != '=') continue;

			// get value
			buf = &str[strlen(t_conf[i].conf)+1];
			if (!(*(t_conf[i].val) = (uint32_t)strtol((const char *)buf, &a, 0))) {
				printf("%s : invalid value!!\r\n",t_conf[i].conf);
				f_close( &rom_fl );
				return -1;
			}
			printf("%s : $%06lx\r\n", (const char *)t_conf[i].conf, *(t_conf[i].val));
		}
	}

	if (!f_eof(&rom_fl)) {
		printf("File read error!\r\n");
		f_close( &rom_fl );
		return -1;
	}
	f_close( &rom_fl );
	return 0;
}

static int get_line(char *s, int length) {
	char n;
	int c;
	
	for (c=0;;) {
		n = (char)getch();
		if ( n == BS ) {
			if ( c > 0) {
				putch(BS);
				putch(' ');
				putch(BS);
				c--;
				s--;
			}
			continue;
		}
		if ( n == 0x0d || n == 0x0a ) {
			*s = 0x00;
			printf("\r\n");
			return c;
		}
		if ( c <= line_size-1 ) {
			putch(n);
			if ( n >='a' && n <='z' ) n -= 0x20;		// lower to upper
			*s++ = n;
			c++;
		}
	}
	return c;
}

static FRESULT scan_files(void) {	/* *path = directory name "/": root */
	FRESULT res;
	FILINFO fno;
	DIR *dir;
	char *fn;
	TCHAR *p;

	p = (char *)&tmp_buf[0][0];
	res = f_getcwd( p, line_size);

	dir = (DIR *)&tmp_buf[1][0];
	res = f_opendir(dir, arg[0]);
	if (res == FR_OK) {		// directory
		if ( *arg[0] == '\0' ) printf("< %s >\r\n", p );
		else if ( !(*(char *)(p+1)) ) printf("< /%s >\r\n", arg[0] );
		else printf("< %s/%s >\r\n", p, arg[0] );

		for (;;) {
			res = f_readdir(dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) break;
			if (fno.fname[0] == '.') continue;				/* ignore '.'  */
			fn = fno.fname;
			if (fno.fattrib & AM_DIR) {					/* Directory */
				printf("  %12s\t<DIR>\r\n", fno.fname);
			}
			else {									/* file */
				printf("  %12s\t%ld bytes.\r\n", fn, fno.fsize);
			}
		}
		f_closedir(dir);
	}
	else {
		if (res == FR_NO_PATH) { // check file
			res = f_open(&rom_fl, (const TCHAR *)arg[0], FA_READ);
			if ( res == FR_OK ) {
				printf("  %12s\t%ld bytes.\r\n", arg[0], f_size(&rom_fl));
				f_close( &rom_fl );
			}
		}
	}
	return res;
}

static FRESULT scan_files1(void) {	/* *path = directory name "/": root */
	FRESULT res;
	FILINFO fno;
	DIR *dir;
	uint8_t	fcnt;
	
	char *fn, *p, *path;

	path = arg[0];
	dir = (DIR *)&tmp_buf[1][0];
	fcnt = 0;
	
	res = f_opendir(dir, path);
	if (res == FR_OK) {		// directory
		p = (char *)&tmp_buf[0][0];
		f_getcwd( p, line_size);
		
		if ( *path == '\0' ) printf("< %s >\r\n", p );
		else if ( !(*(char *)(p+1)) ) printf("< /%s >\r\n", path );
		else printf("< %s/%s >\r\n", p, path );

		for (;;) {
			res = f_readdir(dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) {
				if ( fcnt ) {
					printf("\r\n");
					break;
				}
			}
			if (fno.fname[0] == '.') continue;				/* ignore '.'  */
			fn = fno.fname;
			if (fno.fattrib & AM_DIR) {					/* Directory */
				printf("[%12s] ", fn);
			}
			else {									/* file */
				printf(" %12s  ", fn);
			}
			fcnt += 1;
			if (fcnt == 5 ) {
				printf("\r\n");
				fcnt = 0;
			}
		}
		f_closedir(dir);
	}
	else {
		if (res == FR_NO_PATH) { // check file
			res = f_open(&rom_fl, (const TCHAR *)path, FA_READ);
			if ( res == FR_OK ) {
				printf("  %12s\t%ld bytes.\r\n", path, f_size(&rom_fl));
				f_close( &rom_fl );
			}
		}
	}
	return res;
}

static void set_arg(char *buf) {

	int i;
	
	i=0;
	while( i < arg_num ) {
		while( *buf == ' ' ) buf++;
		arg[i] = buf;
		if ( !*buf ) break;
		while( *buf && *buf != ' ' ) buf++;
		if ( !*buf ) break;
		if ( *buf == ' ' ) *buf++ = 0;
		i++;
	}
}

static FRESULT drive_cpu(void) { 

	FRESULT rs;

	nmi_sig = 0;
	if( !fh.irq_sw ) {
		timer_off();
		board_event_loop1();
	}
	else {
		timer_on();
		board_event_loop();
	}

	// wup_flg = 0xFF : NMI, wup_flg = 1 : sleep( or terminate )
	rs = (FRESULT)(wup_flg - 1);		// get wakeup reason -2:NMI 
	wup_flg = 0;						// clear wakeup flag for NMI

	return rs;
}


static void prt_reload(void) { 
	printf("Reload monitor %s\r\n", arg[0]);
}

static FRESULT boot_file(void) { 

	FRESULT rs;
	uint8_t flag, m;
	UINT cnt;
	
	if ( strstr((const char *)arg[0], mon02+1) ) m = 0;
	else if ( strstr((const char *)arg[0], mon16+1) ) m = 1;
	else m = 2;
	if ( m != 2) {
		mon = m;
		prt_reload();
		binv.sw = 0;	//clear user program
		rs = (FRESULT)load_program((uint8_t *)arg[0]);
		if ( rs ) return 0;

		start_cpu();
		rs = drive_cpu();
		return rs;
	}
	
	// check mark '<' for FILE INPUT modde
	if ( *arg[1] == '<' ) {
		sprintf(&fin_name[0], "%s", arg[2]);
		fin_cnt = 0xffff;	// set open flag
		cin_no = INPUT_FILE;								// change console input UART to FILE
	}

	//check DOS65.SYS
	if ( strstr((const char *)arg[0], dos65+1) ) {
		rs = open_dos65();
		return rs;
	}
	
	flag = 0;
	printf("Flie(%s) loading...\r\n", arg[0]);
	
	rs = (FRESULT)load_program((uint8_t *)arg[0]);
	if ( rs ) return 0;

	if ( binv.sw ) {	// user program
		if ( cpu_flg ) {	// 65816
			if ( mon ) {	//mon16
				if ( !binv.sw_816 ) {
					arg[0] = mon02;
					mon = 0;
					flag = 1;
					printf("Reload %s for Emulation Mode...\r\n", arg[0]);
				}
			}
			else {	// mon02
				if ( binv.sw_816 ) {
					arg[0] = mon16;
					mon = 1;
					flag = 1;
					printf("Reload %s for Native Mode...\r\n", arg[0]);
				}
			}
			if ( flag ) {	// reroad monitor
				rs = (FRESULT)load_program((uint8_t *)arg[0]);
				if ( rs ) return 0;
			}
		}
		binv.addr = binv.caddr;
		write_sram( fh.load_p+mezID_off, (uint8_t *)&binv, BINV_SIZE );		// address of monitor's mezID
	}

	write_sram(0xFFFC, (uint8_t *)&fh.cstart_addr, 2);
	start_cpu();	//set monitor cold start
	rs = drive_cpu();
	if ( !rs ) {
		if ( !fh.bios_sw ) {	// if standalone prpgram is terminated, then need reload monitor
			if (cpu_flg) {
				arg[0] = mon16;
				mon = 1;
			}
			else {
				arg[0] = mon02;
				mon = 0;
			}
			prt_reload();
			rs = (FRESULT)load_program((uint8_t *)arg[0]);
			if ( rs ) return 0;
			start_cpu();
			rs = drive_cpu();
		}
		binv.sw = 0;	// terminate bios_call program
	}
	return rs;
}

static FRESULT return_cpu(void) { 
	
	FRESULT rs;

	continue_action();
	rs = drive_cpu();
	return rs;
}

static FRESULT change_directory(void) { return(f_chdir( arg[0] )); } 

static FRESULT print_reg(void) { 
	reg816 *reg_816;
	reg02 *reg_02;

	read_sram((uint32_t)fh.reg_tblp, &tmp_buf[0][0], (unsigned int)fh.reg_tsize);

	if (fh.sw_816) {	/* 65816 */
		reg_816 = (reg816 *)&tmp_buf[0][0];
		printf("A=$%04X X=$%04X Y=$%04X SP=$%04X PC=$%04X PSR=$%02X\r\n",
				reg_816->REGA, reg_816->REGX,reg_816->REGY,reg_816->REGSP,reg_816->REGPC,reg_816->REGPSR);
		printf("PBR=$%02X DBR=$%02X DPR=$%04X\r\n",reg_816->REGPB, reg_816->REGDB, reg_816->REGDP);
	}
	else {
		reg_02 = (reg02 *)&tmp_buf[0][0];
		printf("A=$%02X X=$%02X Y=$%02X SP=$01%02X PC=$%04X PSR=$%02X\r\n",
				reg_02->REGA, reg_02->REGX,reg_02->REGY,reg_02->REGSP,reg_02->REGPC,reg_02->REGPSR);
	}
	return FR_OK;
}

static FRESULT print_com(void) { 
	
	printf("\r\n<< MEZW65C_RAM Firmware Built-in command >>\r\n");
	printf("  DOS65  (Key CTL+T : terminate DOS65)\r\n");
	printf("  LS | DIR [file name | directory name\r\n");
	printf("  CD [directory name]\n\r");
	printf("  LOAD [L=load address(Hex)] file name\r\n");
	printf("  MDUMP address(Hex)\r\n");
	printf("  MONITOR [W]\r\n");
	printf("  CSTART\r\n");
	printf("  WSTART\r\n");
	printf("  RETI\r\n");
	printf("  REG\r\n");
	printf("  SHOW file name\r\n");
	printf("  FLASH\r\n");
	printf("  HELP | ?\r\n");
	return FR_OK;
}
static FRESULT load_file(void) {
	char *p;
	
	load_flg = 1;
	if ((p = strstr(arg[0], "L=" ))) {
		raw_addr = get_hex((char *)(p+2));
		load_program((uint8_t *)arg[1]);
	}
	else {
		raw_addr = 0;
		load_program((uint8_t *)arg[0]);
	}
	load_flg = 0;
	raw_addr = 0;
	return FR_OK;
}

static FRESULT wstart_prog(void) {
	FRESULT rs;

	rs = FR_NO_FILE;
	if ( binv.sw == 1) {
		binv.addr = binv.waddr;
		write_sram( fh.load_p+mezID_off, (uint8_t *)&binv, BINV_SIZE );		// address of monitor's mezID
		
		write_sram(0xFFFC, (uint8_t *)&fh.cstart_addr, 2);
		start_cpu();
		rs = drive_cpu();
	}
	return rs;
}

static FRESULT restart_prog(void) {
	FRESULT rs;

	rs = FR_NO_FILE;
	if ( binv.sw ==1 ) {
		binv.addr = binv.caddr;
		write_sram( fh.load_p+mezID_off, (uint8_t *)&binv, BINV_SIZE );		// address of monitor's mezID

		write_sram(0xFFFC, (uint8_t *)&fh.cstart_addr, 2);
		start_cpu();
		rs = drive_cpu();
	}
	return rs;
}

static FRESULT mon_prog(void) {
	FRESULT rs;
	char *p;
	uint8_t	sw;
	
	rs = FR_OK;

	sw = 0;					// monitor wakeup signal
	write_sram( fh.load_p+mezID_off, &sw, 1 );			// address of  fh.mezID

	if ((p = strstr(arg[0], "W" ))) write_sram(0xFFFC, (uint8_t *)&fh.wstart_addr, 2);
	else write_sram(0xFFFC, (uint8_t *)&fh.cstart_addr, 2);

	start_cpu();
	rs = drive_cpu();
	printf("\r\n");
	return rs;
}

static FRESULT mem_dump(void) {

	uint32_t addr;
	char *p;
	
	p = arg[0];
	addr = get_hex( p );

	read_sram(addr, (uint8_t *)&tmp_buf[0][0], 128);		//128 bytes read memory
	util_addrdump("Mem ", addr, (const void *)&tmp_buf[0][0], 128);
	return FR_OK;
}

static FRESULT see_file(void) {
	
	FRESULT	fr;
	void	*rdbuf;
	UINT	br;
	file_header *header;

	rdbuf = (void *)&tmp_buf[0][0];		// program load work area(512byte)
	header = (file_header *)&tmp_buf[0][0];

	fr = f_open(&rom_fl, (TCHAR *)arg[0], FA_READ);
	if ( fr != FR_OK ) {
		printf("\r\nFile Open Error.\r\n");
		return 0;
	}

	file_size = (uint16_t)f_size(&rom_fl);		// get file size
	fr = f_read(&rom_fl, rdbuf, BUF_SIZE, &br);
	if (fr == FR_OK) {

		printf("\r\n%s : Size = $%04X bytes.\r\n", arg[0], file_size);
		if (!strstr((const char *)header->mezID, mezID)) {	// not MEZW65C_RAM format
			printf("Not MEZW65C_RAM format file.\r\n");
		}
		else {
			printf("FIle load address : $%06lX\r\n", (unsigned long)header->load_p );
			printf("File Type : ");
			switch (header->bios_sw) {
				case 0:		// standalone program
				case 2:		// MONITOR program
					if ( !header->bios_sw ) printf("Stand-alone\n\r");
					else printf("MONITOR\n\r");
					printf("Operational Mode : ");
					if (!header->sw_816) printf("W65C02 (Emulation Mode)\n\r");
					else printf("W65C816 Native mode\n\r");
					printf("CSTART : $%04X\r\n",header->cstart_addr);
					printf("WSTART : $%04X\r\n",header->wstart_addr);
					if (header->sw_816 == 1) printf("DPR : $%04X PBR : $00 DBR : $00\r\n",header->direct_page);
					printf("PIC I/F Shared memory : $%06lX\r\n",(unsigned long)header->picif_p);
					if ( header->bios_sw == 2 ) {
						if( header->irq_sw ) printf("IRQ : Support, ");
						else printf("IRQ : No support, ");
						if( header->nmi_sw ) printf("MNI : Support\r\n");
						else printf("MNI : No support\r\n");
					}
					break;
				case 1:		// user program
					printf("User Program.\n\r");
					printf("Operational Mode : ");
					switch (header->sw_816) {
						case 0:
							printf("W65C02 (Emulation Mode)\n\r");
							break;
						case 1:
							printf("W65C816 Native mode\n\r");
							break;
						case 2:
							printf("Both W65C02 and W65C816\n\r");
					}
					printf("CSTART : $%04X\r\n",header->cstart_addr);
					printf("WSTART : $%04X\r\n",header->wstart_addr);
					if (header->sw_816) printf("DPR : $%04X PBR : $%02X DBR : $%02X\r\n",header->direct_page, header->op1, header->op2);
			}
		}
	}
	printf("\r\n");
	f_close(&rom_fl);
	return 0;
}

static FRESULT flash_apl(void) {
	if ( binv.sw ) {
		printf("Terminate %s\r\n",binv.fname);
		binv.sw = 0;
		// close disk images if DOS65 was suspended by NMI
		close_dos65();
	}
	else printf("No Program to terminate.\r\n");

	return FR_OK;
}

static int in_file(void) {

	uint8_t	chr, *buf;

	if ( fin_cnt == 0xffff ) { // init : open file
		// open input file & set console input from file
		if ( f_open(&in_fl, &fin_name[0], FA_READ) ) {
			printf("File Open Error. %s\r\n", &fin_name[0]);
			cin_no = INPUT_UART;						// change console input to UART
			return 0;
		}

		buf = &tmp_buf[0][0];
		fin_cnt=0;
		frd_ptr = 0;
		fin_size = (uint16_t)f_size(&in_fl);				// get file size
	}

next_char:

	if ( !fin_cnt ) {
		frd_ptr = 0;
		if ( f_read( &in_fl, buf, BUF_SIZE, (UINT *)&fin_cnt ) ) {
			printf("File read error.\r\n");
			f_close( &in_fl );
			cin_no = INPUT_UART;			// set console to UART
			return 0;
		}
	}

	chr = buf[frd_ptr++];
	fin_cnt--;
	fin_size--;
	
	if ( !fin_size ) {
		f_close( &in_fl );
		cin_no = INPUT_UART;			// set console to UART
		if ( chr < CR ) chr = CR;
		return chr;
	}

	if ( chr < CR ) goto next_char;
	return chr;
}

static unsigned int str_inf(char *buf, uint8_t cnt) {
	unsigned int c, i;
	char a;
	uint8_t *sbuf;
	
	if ( fin_cnt == 0xffff ) { // init : open file
		// open input file & set console input from file
		if ( f_open(&in_fl, &fin_name[0], FA_READ) ) {
			printf("File Open Error. %s\r\n", &fin_name[0]);
			cin_no = INPUT_UART;						// change console input to UART
			return 0;
		}

		sbuf = &tmp_buf[0][0];

		fin_cnt=0;
		frd_ptr = 0;
		fin_size = (uint16_t)f_size(&in_fl);				// get file size
	}

	if ( !fin_cnt ) {
		frd_ptr = 0;
		if ( f_read( &in_fl, sbuf, BUF_SIZE, (UINT *)&fin_cnt ) ) {
			printf("File read error.\r\n");
			f_close( &in_fl );
			cin_no = INPUT_UART;			// set console to UART
			return 0;
		}
	}

	i = ( (unsigned int)cnt > fin_cnt ) ? fin_cnt : (unsigned int)cnt;

	fin_cnt -= i;
	fin_size -= i;
	c = i;
	while(i--) {
		a = sbuf[frd_ptr++];
		if ( a < CR ) {
			c--;
			continue;
		}
		else *buf++ = a;
	}
	
	if ( !fin_size ) {
		f_close( &in_fl );
		cin_no = INPUT_UART;			// set console to UART
	}
	
	return c;
}

//
// check dsk
// 0  : No CPMDISKS directory
// 1  : CPMDISKS directory exist
//
static int chk_dsk(void)
{
    int selection;
    uint8_t c;
	DIR *fsdir;
	
	fsdir = (DIR *)&tmp_buf[1][0];

    //
    // Select disk image folder
    //
	selection = 0;
    if (f_opendir(fsdir, "/")  != FR_OK) {
        printf("Failed to open SD Card.\n\r");
		return selection;
    }

	f_rewinddir(fsdir);
	while (f_readdir(fsdir, &fileinfo) == FR_OK && fileinfo.fname[0] != 0) {
		if (strcmp(fileinfo.fname, dosdir) == 0) {
			selection = 1;
			printf("Detect %s\n\r", fileinfo.fname);
			break;
		}
	}
	f_closedir(fsdir);
	
	return(selection);
}

//
// Open disk images
//
static int open_dskimg(void) {
	
	uint16_t drv;
	char drive_letter;
	char *buf;
	
	for (drv = 0; drv < NUM_DRIVES; drv++) {
        drive_letter = (char)('A' + drv);
        buf = (char *)tmp_buf[0];
        sprintf(buf, "%s/DOS65%c.DSK", fileinfo.fname, drive_letter);
        if (f_open(&files[drv], buf, FA_READ|FA_WRITE) == FR_OK) {
        	printf("Image file %s/DOS65%c.DSK is assigned to drive %c\n\r",
                   fileinfo.fname, drive_letter, drive_letter);
        	drives[drv].filep = &files[drv];
        	drives[drv].exist = 1;
        }
		else drives[drv].exist = 0;
    }
	if ( !drives[0].exist ) return -4;
	return 0;
}

static FRESULT open_dos65(void) {

	FRESULT	rs;
	char drive_letter;

	uint16_t drv;

	if ( !chk_dsk() ) {
		printf("DOS_DISK directory not found.\r\n");
		return FR_OK;
	}
	if ( open_dskimg() < 0 ) {
		printf("Drive A not found.\n\r");
		return FR_OK;
	}

	// load DOS65.SYS
	if ( (FRESULT)load_program((uint8_t *)dos65) ) {
		printf("%s not found.\r\n", dos65);
		return FR_OK;
	}

	// check monitor
	if ( mon ) {
		mon = 0;
		printf("Reload %s for Emulation Mode...\r\n", mon02);
		if ( (FRESULT)load_program((uint8_t *)mon02) ) {
			printf("%s not found.\r\n", mon02);
			return FR_OK;
		}
	}

	binv.addr = binv.caddr;
	write_sram( fh.load_p+mezID_off, (uint8_t *)&binv, BINV_SIZE );		// address of monitor's mezID

	write_sram(0xFFFC, (uint8_t *)&fh.cstart_addr, 2);
	start_cpu();	//set monitor cold start
	rs = drive_cpu();
	if ( !rs ) {
		binv.sw = 0;	// terminate bios_call program
		// close disk images
		rs = close_dos65();
    }
	return rs;
}

// close DOS65 disk image
static FRESULT close_dos65(void) {
	FRESULT	rs;
	char drive_letter;

	uint16_t drv;

	rs = FR_OK;
	for (drv = 0; drv < NUM_DRIVES; drv++) {
        drive_letter = (char)('A' + drv);
		if ( drives[drv].exist ) {
			rs = f_close( drives[drv].filep );
       		printf("\r\nClose disk Image file /%s/DOS65%c.DSK", dosdir, drive_letter);
			drives[drv].exist = 0;
        }
	}
	printf("\r\n");
	
	return rs;
}
