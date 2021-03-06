/*
 * MotorInterface.h
 *
 *  Created on: 03.04.2018
 *      Author: Przemek
 */

#ifndef INC_MOTORINTERFACE_H_
#define INC_MOTORINTERFACE_H_

#include "CAN2UART.h"

typedef void* tMotorInterfaceHandler;
typedef enum{eInactiveMode=0,eActiveMode}tMotorInterfaceMode;
typedef struct{
	int canId;
	int numPolePairs;
	int reversMode;/**<tryb pracy odwróconej*/
	tCAN2UARTHandle c2u;
}tMotorInterfaceConfig;


int MotorInterface_Init(tMotorInterfaceHandler *h,tMotorInterfaceConfig *cfg);
int MotorInterface_SetMode(tMotorInterfaceHandler h,tMotorInterfaceMode mode);
int MotorInterface_UpdateControl(tMotorInterfaceHandler h,int ctrl);
int MotorInterface_GetSpeed(tMotorInterfaceHandler h,float *rpm);
int MotorInterface_GetPosition(tMotorInterfaceHandler h,float *angle);
int MotorInterface_GetCurrent(tMotorInterfaceHandler h,float *current);
int MotorInterface_GetVoltage(tMotorInterfaceHandler h,float *voltage);
int MotorInterface_GetState(tMotorInterfaceHandler h,tMotorInterfaceMode *mode);
void MotorInterface_NewMotorState(tMotorInterfaceHandler h,tMotorInterfaceMode mode);


#endif /* INC_MOTORINTERFACE_H_ */
