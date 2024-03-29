/****************************************************
*                                                   *
*---------------------------------------------------*
*  PDA Drivers                                      *
*---------------------------------------------------*
*                                                   *
* Copyright (C) 2019, Gkagkanis Efstratios,         *
*    				all rights reserved.            *
*                                                   *
* Redistribution and use in source and binary forms,*
* with or without modification, are permitted       *
* provided that the following condition is met:     *
*                                                   *
* 1. Redistributions of source code must retain the *
* the above copyright notice, this condition and    *
* the following disclaimer.                         *
*                                                   *
*                                                   *
* This software is provided by the copyright holder *
* and any warranties related to this software       *
* are DISCLAIMED.                                   *
* The copyright owner or contributors be NOT LIABLE *
* for any damages caused by use of this software.   *
*                                                   *
****************************************************/

#include "pda_drivers.h"
#include "sample_file.h"
#include "debug.h"
#include "gpio.h"

/**********/
/* MACROS */
/**********/
#define RED_TIME 5 /* Turn on RED LED for 5 seconds in case of error */

/************************/
/* Variable Definitions */
/************************/
uint32_t *pru_shared_ram = 0;	//Int pointer for RAM addressing.
uint16_t *ddr_casted = 0;
uint32_t sample_len;

double clkfreq;
uint16_t frames;
double intgr_time;
double fps;

uint16_t intgr_quarter_delay_instr;
uint16_t intgr_half_delay_instr;
uint16_t charge_instr;
uint16_t read_instr_full;
uint16_t read_instr_half;
uint32_t extra_instr;

/*************************/
/* Function Declarations */
/*************************/	
static uint32_t Argv_Handler(int *argc, char *argv[]);
static uint32_t Delay_Calculation(void);
static uint32_t Mem_Alloc(void);
static uint32_t Init_PRUSS(void);
static uint32_t Start_PRUs(void);
static void Wait_For_PRUs(void);
static uint32_t Deinit_PRU(void);

int main(int argc, char *argv[])
{
	uint32_t result, file_number;
		
	#if DEBUG_MODE == 1
		printf("Debug Mode ON\n");
	#endif
	
	result = Argv_Handler(&argc, argv);
	Debug_Print("Argv_Handler result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Init_GPIO();
	Debug_Print("Init_GPIO result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Write_LED(BLUE);
	Debug_Print("Write_LED result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Button_Wait_To_Press();
	Debug_Print("Button_Wait_To_Press result:", &result);
	if (result != NO_ERR) goto exit;

	result = Write_LED(GREEN);
	Debug_Print("Write_LED result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Create_File();
	Debug_Print("Make_File result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Delay_Calculation();
	Debug_Print("Delay_Calculation result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Init_PRUSS();
	Debug_Print("Init_PRUSS result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Mem_Alloc();
	Debug_Print("Mem_Alloc result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Start_PRUs();
	Debug_Print("Start_PRUs result:", &result);
	if (result != NO_ERR) goto exit;
	
	Wait_For_PRUs();
	
	file_number = Save_Samples();
	Debug_Print("Save_Samples result:", &file_number);
	if (file_number >= ARGU_ERR) goto exit;
	
	result = Deinit_PRU();
	Debug_Print("Deinit_PRU result:", &result);
	if (result != NO_ERR) goto exit;
	
	result = Write_LED(OFF);
	Debug_Print("Write_LED result:", &result);
	if (result != NO_ERR) goto exit;

	result = Button_Wait_To_Release();
	Debug_Print("Button_Wait_To_Release result:", &result);
	if (result != NO_ERR) goto exit;
	
	return file_number;
	
exit:
	Debug_Print("Error code:", &result);
	Write_LED(RED);
	sleep(RED_TIME);
	Write_LED(OFF);
	return result;
}

/********************/
/* HELPER FUNCTIONS */
/********************/

static uint32_t Argv_Handler(int *argc, char *argv[])
{
	uint32_t result = ARGU_ERR;
	
	if (*argc != ARGUMENTS_NUM) {
		printf("Incorrect arguments.\n");
		printf("Example: ./pda_drivers 10 120 50 100\n");
		printf("First argument: Number of Frames \n");
		printf("Second argument: Integration Time in us. Min = 33.75us, Max = 22020us \n");
		printf("Third argument: Frames per second (fps). \n");
		printf("Fourth argument: Clock Frequency. Min = 5 KHz, Max = 8000 KHz.\n");
		printf("Please try again.\n");
		goto exit;
	}
	
	clkfreq = abs(atof(argv[4]));
	frames = abs(atoi(argv[1]));
	intgr_time = abs(atof(argv[2]));
	fps = abs(atof(argv[3]));
	
	if(clkfreq < MIN_CLK_FREQ || clkfreq > MAX_CLK_FREQ) {
		goto exit;
	}
	if(intgr_time < MIN_INTGR_TIME || intgr_time > MAX_INTGR_TIME) {
		goto exit;
	}
	
	result = NO_ERR;
exit:
	return result;
}

static uint32_t Delay_Calculation(void)
{
	double intgr_pulse_quarter;
	double extra_time;
	double charge_time;
	
	/* (PIXELS+1)*4+1 = 517 || 3 = half_delay || 514 = full_delay */
	intgr_pulse_quarter = ((intgr_time - EXTRA_WAIT_B4_NEXT_SI) * uS_TO_S) / INTGR_PULSE_QUARTER;
	intgr_quarter_delay_instr = (int) (intgr_pulse_quarter / INSTRUCTION_DELAY);
	intgr_half_delay_instr = (int) (intgr_pulse_quarter * 2 / INSTRUCTION_DELAY);
	charge_time = ((intgr_pulse_quarter) - (intgr_quarter_delay_instr * INSTRUCTION_DELAY))*3;

	charge_time += (((intgr_pulse_quarter*2) - (intgr_half_delay_instr * INSTRUCTION_DELAY))*(INTGR_PULSE_QUARTER-3)/2);

	charge_instr = intgr_quarter_delay_instr*3 + intgr_half_delay_instr*(FIRST_18_PULSES*2-1) + intgr_half_delay_instr + (int) (charge_time /INSTRUCTION_DELAY);
	
	read_instr_full = (int) ((1/(clkfreq*KHZ_TO_MHZ)) / 2 / INSTRUCTION_DELAY);
	read_instr_half = read_instr_full / 2;
	
	extra_time = (1/fps) - (intgr_time/S_TO_uS) - ((1/(clkfreq * KHZ_TO_MHZ)/2*(PIXELS*2+1))+(1/(clkfreq*KHZ_TO_MHZ)/4*3));
	extra_instr = ((int)(extra_time / INSTRUCTION_DELAY)) - (intgr_quarter_delay_instr*3 + intgr_half_delay_instr*(FIRST_18_PULSES*2-1)) ;
		
	return NO_ERR;
}

static uint32_t Init_PRUSS(void)
{
	uint32_t result = PRU_INIT_ERR;
	int res;
	
	res = prussdrv_init();
	res |= prussdrv_open (PRU_EVTOUT_0);
	res |= prussdrv_open (PRU_EVTOUT_1);
	if (res != 0) goto exit;
	
	/* PRU Interrupt Setup */
	tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
	result = (uint32_t) prussdrv_pruintc_init(&pruss_intc_initdata);
	if (result != 0) goto exit;
	
	result = NO_ERR;
exit:
	return result;
}

static uint32_t Mem_Alloc(void)
{
	uint32_t result = MEM_ERR;
	void *pru_shared_ram_void;
	void *ddr_ram_void;
	uint32_t ddr_address;
	
	/* PRU Shared RAM & DDR Setup */
	prussdrv_map_prumem(PRUSS0_SHARED_DATARAM, &pru_shared_ram_void);
	pru_shared_ram = (uint32_t *) pru_shared_ram_void;
	prussdrv_map_extmem((void **) &ddr_ram_void);
	ddr_address = prussdrv_get_phys_addr((void *) ddr_ram_void);
	
	sample_len = (PIXELS * frames * sizeof(frames));
	if (sample_len > prussdrv_extmem_size()) {
		goto exit; /*If DDR memory allocated to PRUs is not enough, terminate. */
	}
	
	pru_shared_ram[Pixels_Offset] = PIXELS;
	pru_shared_ram[Frames_Offset] = frames;
	pru_shared_ram[DDR_Addr_Offset] = ddr_address; 
	pru_shared_ram[DDR_Size_Offset] = sample_len;
	pru_shared_ram[Intgr_Stage_Full_Offset] = intgr_half_delay_instr;
	pru_shared_ram[Intgr_Stage_Half_Offset] = intgr_quarter_delay_instr;
	pru_shared_ram[Intgr_Stage_Charge_Offset] = charge_instr;
	pru_shared_ram[Read_Stage_Full_Offset] = read_instr_full;
	pru_shared_ram[Read_Stage_Half_Offset] = read_instr_half;
	pru_shared_ram[ExtraTime_Stage_Offset] = extra_instr;

	//CLEAR MEMORY
	ddr_casted = (uint16_t *) ddr_ram_void;
	memset(ddr_casted, 0x00, (sample_len+1));
	
	result = NO_ERR;
exit:
	return result;
}

static uint32_t Start_PRUs(void)
{
	uint32_t result = PRU_START_ERR;
	int res;
	
	pru_shared_ram[Handshake0_Offset] = 0;
	pru_shared_ram[Handshake1_Offset] = 0;

	res = prussdrv_exec_program (PRU0, PRU0_BIN_PATH);
	res |= prussdrv_exec_program (PRU1, PRU1_BIN_PATH);
	if (res != 0) goto exit;
	
	while(pru_shared_ram[Handshake0_Offset] != 22522);	//PRU0 Handshake through pru_shared_ram

	prussdrv_pru_wait_event(PRU_EVTOUT_1);	//PRU1 Handshake through INTC
	prussdrv_pru_clear_event (PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);
	if (res != 0) goto exit;
	pru_shared_ram[Handshake0_Offset] = 111;
	pru_shared_ram[Handshake1_Offset] = 222;
	
	result = NO_ERR;
exit:
	return result;
}

static void Wait_For_PRUs(void)
{
	while(pru_shared_ram[Handshake0_Offset] != 55255);	//Wait for PRU0 to finish.
	prussdrv_pru_wait_event(PRU_EVTOUT_1);	//Wait for PRU1 to finish.
	prussdrv_pru_clear_event (PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);
}

static uint32_t Deinit_PRU(void)
{
	uint32_t result;
	int res;
	
	res = prussdrv_pru_disable(PRU0);
	res |= prussdrv_pru_disable(PRU1);
	res |= prussdrv_exit ();
	
	if (res == 0) {
		result = NO_ERR;
	}
	else {
		result = PRU_DEINIT_ERR;
	}

	return result;
}