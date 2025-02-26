/*
 * This source code is for executing CPM I/O requests.
 *
 * Base source code is maked by @hanyazou
 *  https://twitter.com/hanyazou
 *
 * Redesigned by Akihito Honda(Aki.h @akih_san)
 *  https://twitter.com/akih_san
 *  https://github.com/akih-san
 *
 *  Target: PIC18F57QXX/PIC18F47QXX
 *  Date. 2024.4.20
 */

#include "../src/w65.h"
#include <stdio.h>
#include <assert.h>

#include "../fatfs/ff.h"
#include "../drivers/utils.h"

#define SECTOR_SIZE      512

// from unimon
#define CONIN_REQ	0x01
#define CONOUT_REQ	0x02
#define CONST_REQ	0x03
#define STROUT_REQ	0x04
#define REQ_DREAD	0x05
#define REQ_DWRITE	0x06
#define STRIN_REQ	0x07
#define WUP_REQ		0xff

#define RETURN_TBL 2		/* bytes of return parameter */

// request table
static crq_hdr req_tbl;
static uint8_t c_buf[U3B_SIZE];


static int seek_disk(void) {
	unsigned int n;
	FRESULT fres;
	FIL *filep;

	if (!drives[req_tbl.disk_drive].exist) return(-1);
	
	filep = drives[req_tbl.disk_drive].filep;
	if ((fres = f_lseek(filep, req_tbl.disk_lba * SECTOR_SIZE)) != FR_OK) {
		printf("f_lseek(): ERROR %d\n\r", fres);
		return(-1);
	}
	return 0;
}

static int write_sector(void) {
	unsigned int n;
	FRESULT fres;
	FIL *filep = drives[req_tbl.disk_drive].filep;
	uint8_t cnt;
	
	cnt = req_tbl.data_cnt;
	while( cnt-- ) {
		if (seek_disk()) return -1;

		// transfer write data from SRAM to the buffer
		read_sram((uint32_t)req_tbl.data_adr, &tmp_buf[0][0], SECTOR_SIZE);

		if (DEBUG_DISK_WRITE && DEBUG_DISK_VERBOSE && !(debug.disk_mask & (1 << req_tbl.disk_drive))) {
			util_hexdump_sum("buf: ", &tmp_buf[0][0], SECTOR_SIZE);
		}

		// write buffer to the DISK
		if ((fres = f_write(filep, &tmp_buf[0][0], SECTOR_SIZE, &n)) != FR_OK || n != SECTOR_SIZE) {
			printf("f_write(): ERROR res=%d, n=%d\n\r", fres, n);
			return -1;
		}
		else if ((fres = f_sync(filep)) != FR_OK) {
			printf("f_sync(): ERROR %d\n\r", fres);
			return -1;
		}
			// update next wrhite parameter
		req_tbl.data_adr += SECTOR_SIZE;
		req_tbl.disk_lba++;
	}
	return 0;
}

static int read_sector(void) {
	unsigned int n;
	FRESULT fres;
	FIL *filep = drives[req_tbl.disk_drive].filep;
	uint8_t cnt;
	
	cnt = req_tbl.data_cnt;
	while( cnt-- ) {
		if (seek_disk()) return -1;

		// read from the DISK
		if ((fres = f_read(filep, &tmp_buf[0][0], SECTOR_SIZE, &n)) != FR_OK || n != SECTOR_SIZE) {
			printf("f_read(): ERROR res=%d, n=%d\n\r", fres, n);
			return -1;
		}
		else {
			if (DEBUG_DISK_READ && DEBUG_DISK_VERBOSE && !(debug.disk_mask & (1 << req_tbl.disk_drive))) {
				util_hexdump_sum("buf: ", &tmp_buf[0][0], SECTOR_SIZE);
			}
			// transfer read data to SRAM
			write_sram((uint32_t)req_tbl.data_adr, &tmp_buf[0][0], SECTOR_SIZE);

			#ifdef MEM_DEBUG
			printf("f_read(): SRAM address(%08lx)\n\r", req_tbl.data_adr);
			read_sram(req_tbl.data_adr, &tmp_buf[0][0], SECTOR_SIZE);
			util_hexdump_sum("RAM: ", &tmp_buf[0][0], SECTOR_SIZE);
			#endif  // MEM_DEBUG
			
			// update next wrhite parameter
			req_tbl.data_adr += SECTOR_SIZE;
			req_tbl.disk_lba++;
		}
	}
	return 0;
}

static int setup_drive(void) {
	req_tbl.CBI_CHR = 0;		/* clear error status */
	if ( req_tbl.disk_drive >= NUM_DRIVES ) return( -1 );
	if ( drives[req_tbl.disk_drive].exist == 0 ) return( -1 );	// no disk exist
	
	req_tbl.disk_lba &= 0x0000ffff;
	return 0;
}

static void dsk_err(void) {
	req_tbl.UNI_CHR = 1;
}

static void unimon_console(void) {

	uint8_t *buf;
	uint16_t cnt;

	buf = &c_buf[0];
	switch (req_tbl.UREQ_COM) {
		// CONIN
		case CONIN_REQ:
			req_tbl.UNI_CHR = (uint8_t)(*get_char[cin_no])();
			break;
		// CONOUT
		case CONOUT_REQ:
			putch((char)req_tbl.UNI_CHR);		// Write data
			break;
		// CONST
		case CONST_REQ:
			if ( !cin_no ) req_tbl.UNI_CHR = (rx_cnt !=0) ? 255 : 0;
			else req_tbl.UNI_CHR = 255;
			break;
		case STROUT_REQ:
			cnt = (uint16_t)req_tbl.UNI_CHR;
			// get string
			read_sram(req_tbl.data_adr, buf, cnt);
			while( cnt ) {
				putch( *buf++);
				cnt--;
			}
			break;
		case STRIN_REQ:
			cnt = (uint16_t)(*strin_func[cin_no])((char *)buf, req_tbl.UNI_CHR);
			req_tbl.UNI_CHR = (uint8_t)cnt;
			if (cnt) write_sram(req_tbl.data_adr, buf, (unsigned int)cnt);
			break;
		case WUP_REQ:
			wup_flg = req_tbl.UNI_CHR;
	}
	req_tbl.UREQ_COM = 0;	// clear unimon request
}

//
// bus master handling
// this fanction is invoked at main() after HOLDA = 1
//
// bioreq_ubuffadr = top address of unimon
//
//  ---- request command to PIC
// UREQ_COM = 1 ; CONIN  : return char in UNI_CHR
//          = 2 ; CONOUT : UNI_CHR = output char
//          = 3 ; CONST  : return status in UNI_CHR
//                       : ( 0: no key, 1 : key exist )
//          = 4 ; STROUT : string address = (req_tbl.data_adr)
//          = 5 ; DISK READ
//          = 6 ; DISK WRITE
//          = 7 ; STRING INUT_REQ
//          = 0 ; request is done( return this flag from PIC )
//                return status is in UNI_CHR;

void bus_master_operation(void) {
	uint8_t *buf;
	uint16_t cnt;

	// read request from MEZ_CPU
	read_sram(bioreq_ubuffadr, (uint8_t *)&req_tbl, (unsigned int)sizeof(crq_hdr));

	if (req_tbl.UREQ_COM) {
		unimon_console();

		// write end request to SRAM for MEZ_CPU
		write_sram(bioreq_ubuffadr, (uint8_t *)&req_tbl, RETURN_TBL);	// 2bytes
	}
	else {
		buf = &c_buf[0];

		switch (req_tbl.CREQ_COM) {
			// CONIN
			case CONIN_REQ:
				req_tbl.CBI_CHR = (uint8_t)(*get_char[cin_no])();
				break;
			// CONOUT
			case CONOUT_REQ:
				putch((char)req_tbl.CBI_CHR);		// Write data
				break;
			// CONST
			case CONST_REQ:
				if ( !cin_no ) req_tbl.CBI_CHR = (rx_cnt !=0) ? 255 : 0;
				else req_tbl.CBI_CHR = 255;
				break;
			case STROUT_REQ:
				cnt = (uint16_t)req_tbl.CBI_CHR;
				// get string
				read_sram(req_tbl.data_adr, buf, cnt);
				while( cnt ) {
					putch( *buf++);
					cnt--;
				}
				break;
			case REQ_DREAD:
				if ( setup_drive() ) {
					dsk_err();
					break;
				}
				if ( read_sector() ) {
					dsk_err();
					break;
				}
				break;
			case REQ_DWRITE:
				if ( setup_drive() ) {
					dsk_err();
					break;
				}
				if ( write_sector() ) {
					dsk_err();
					break;
				}
				break;
			case STRIN_REQ:
				cnt = (uint16_t)(*strin_func[cin_no])((char *)buf, req_tbl.CBI_CHR);
				req_tbl.CBI_CHR = (uint8_t)cnt;
				if (cnt) write_sram(req_tbl.data_adr, buf, (unsigned int)cnt);
				break;
			case WUP_REQ:
				wup_flg = req_tbl.CBI_CHR;
		}
		req_tbl.CREQ_COM = 0;	// clear cbios request
		// write end request to SRAM for MEZ_CPU
		write_sram(bioreq_cbuffadr, (uint8_t *)&req_tbl.CREQ_COM, RETURN_TBL);	// 2bytes
	}

}
