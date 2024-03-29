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

extern uint32_t Create_File(void);
extern uint32_t Save_Samples(void);

/**********/
/* MACROS */
/**********/

/* DIRECTORY RELATED MACROS */
#define SAMPLES_PATH "/home/debian/SGK_PDA/Samples/"
#define SAMPLES_NAME "Sample_"
#define SAMPLES_NAME_LEN 7
#define SAMPLES_COMPL_NAME_LEN 11

/* CODES */
#define END_OF_SAMPLES (59595)