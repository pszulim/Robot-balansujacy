/**
  ******************************************************************************
  * @file	 MPU6050.c
  * @author  PS,MK
  * @version V1.0.0
  * @date    22.03.2018
  * @brief   Moduł dostarcza podstawowego interfejsu komunikacyjnego z czujnikiem MPU6050
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "MPU/MPU6050.h"
#include "MPU/inv_mpu.h"
#include "MPU/inv_mpu_dmp_motion_driver.h"
#include "math.h"
#include "stdio.h"
#include "string.h"
/* Private typedef -----------------------------------------------------------*/

typedef struct{
	tMPUMeasuremenet measurement;
	xQueueHandle measurementQueue;
	xQueueHandle syncQueue;
	xTaskHandle task;
}tMPU6050;
/* Private define ------------------------------------------------------------*/
#define MPUREF()	((tMPU6050*)h)
#define DEFAULT_MPU_HZ  (200)
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static signed char gyro_orientation[9] = {-1, 0, 0,
                                           0,-1, 0,
                                           0, 0, 1};

/* Private function prototypes -----------------------------------------------*/
void MPU6050_Thread(tMPUHandler h);
int MPU6050_DMPConfig(tMPU6050 *mpu);
void MPU6050_ReadDMPFIFO(tMPU6050 *mpu);
/* Public  functions ---------------------------------------------------------*/
/**
  * @brief
  * @param[out]  handler: wskaźnik do uchwytu do obiektu MPU
  * @param[in]   hwSettings: wskaźnik do struktury opisującej sprzetowe parametry łącza z czujnikiem
  * @param[in]	 config: wskaźnik do parametrów konfiguracyjnych modułu
  * @retval 0: brak błedów
  */
int MPU6050_Init(tMPUHandler *handler,tMPUHardwareSetting *hwSettings,tMPUConfiguration *config){
	tMPU6050 *mpu;
	//alokuje pamięc na zmienną
	mpu = pvPortMalloc(sizeof(tMPU6050));
	//alokuję kolejkę bufora odbiorczego
	mpu->measurementQueue = xQueueCreate(config->queueDepth,sizeof(tMPUMeasuremenet));
	//alokuje kolejkę do synchronizacji, za pomocą kolejki będą przekazywane informacje o czasach wykonania pomiaru
	mpu->syncQueue = xQueueCreate(20,sizeof(int));
	//inicjuje wątek przetwarzający
	xTaskCreate(MPU6050_Thread,"MPU",256,mpu,5,&mpu->task);
	return 0;
}
/**
  * @brief  Funkcja przekazuje informacje do modułu o konieczności odczytu danych z bufora pomiarowego czujnika
  * 		Zakłada się, ze funkcja wywoływana jest z przerwania
  * @param[in]  h: uchwyt do obiektu MPU
  * @param[in]	timestamp - opcjonalny stempel czasowy informujący o czasie przechwycenia zdażenia
  * @retval 0 - brak błędów
  */
int MPU6050_UpdateFromISR(tMPUHandler h, unsigned int timestamp){
	BaseType_t xHigherPriorityTaskWoken;
	xHigherPriorityTaskWoken = pdFALSE;
	xQueueSendFromISR( MPUREF()->syncQueue, &timestamp, &xHigherPriorityTaskWoken );
	/* Sprawdzam, czy inny wątek czeka na dane z tej kolejki. */
	if( xHigherPriorityTaskWoken )
	{
			/* zgłaszam koniecznośc przełączenia wątków */
			taskYIELD();
	}
}
/**
  * @brief  Funkcja odczytuje dane jeden rekord pomiarowy z bufora odbiorczego modułu
  * @param[in]  h: referencja do modułu
  * @param[out] measuremenet: wskaźnik do structury gdzie zapisane zostaną odczytane dane
  * @param[in]  timeout: maksymalny czas oczekiwania na pomiar [ms]
  * @retval None
  */
int MPU6050_GetMeasurement(tMPUHandler h,tMPUMeasuremenet *measuremenet,int timeout){
	if(xQueueReceive(MPUREF()->measurementQueue,measuremenet,timeout)==pdTRUE){
		return 0;
	}else{
		return 1;
	}
}
/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Główna funkcja wymiany danych z czujnikiem MPU
  * @param[in]  h: referncja do obiektu
  * @retval None
  */
void MPU6050_Thread(tMPUHandler h){
	tMPU6050 *mpu = MPUREF();
	unsigned int hwTimestamp;
	//inicjuje czujnik
	MPU6050_DMPConfig(mpu);
	while(1){
		//czekam na rozkaz z kolejki
		if(xQueueReceive(mpu->syncQueue,&hwTimestamp,100)==pdTRUE){
			//odebrał╩em rozkaz, odczytuje dane z fifo MPU

		}
	}
}
/**
  * @brief  Funkcja odczytuje dane z kolejki FIFO czujninka MPU i ładuje je do kolejki pomiarów
  * @param[in]  mpu: wskaźnik do obiektu mpu
  * @retval None
  */
void MPU6050_ReadDMPFIFO(tMPU6050 *mpu){

	unsigned long sensor_timestamp;
    short gyro[3], accel[3], sensors;
    unsigned char more;
    long quat[4];
    char b[64];
    float quatf[4];

	if( !dmp_read_fifo(gyro, accel, quat, &sensor_timestamp, &sensors,&more) )
	{
		double x = (pow(quat[0],2) + pow(quat[1],2) + pow(quat[2],2) + pow(quat[3],2));

			double d = sqrt( x );

			for(uint8_t i=0; i<=3;i++)
			{
				quatf[i] = quat[i]/d;
			}

			double roll, pitch, yaw;
			char buf[40] = "Y,";

			roll = 57.2*atan2( -2*quatf[1]*quatf[3] + 2*quatf[0]*quatf[2] , pow(quatf[3],2) - pow(quatf[2],2) - pow(quatf[1],2) + pow(quatf[0],2) );
			sprintf(b, "%2.2f", roll);
			strcat(buf,b);
			strcat(buf,",");


			pitch = 57.2*asin(2*quatf[2]*quatf[3]+2*quatf[0]*quatf[1]);
			sprintf(b, "%2.2f", pitch);
		//        	uart_putstring(b);
		//        	uart_putstring("; ");
			strcat(buf,b);
			strcat(buf,",");

		    yaw = 57.2*atan2( -2*quatf[1]*quatf[2] + 2*quatf[0]*quatf[3] , pow(quatf[2],2) - pow(quatf[3],2) - pow(quatf[1],2) + pow(quatf[0],2) );
		    sprintf(b, "%2.2f", yaw);
		//  uart_putstring(b);
		//  uart_putstring("; ");
		    strcat(buf,b);
		    strcat(buf,"\r\n");

		    uart_putstring(buf);
	}
}

/**
  * @brief  Funkcja konfiguruje czujnik MPU
  * @param[in]  mpu: wskaźnik do obiektu MPU
  * @retval None
  */
int MPU6050_DMPConfig(tMPU6050 *mpu){
	/*
	 *  konfiguracja mpu
	 */
	// inr param ?? --  ??
	struct int_param_s int_param;
	if(mpu_init(&int_param)){
		return 1;//błąd konfiguracji MPU
	}
	///////////////////////////////////////////////////////////

	/*
	 *  wake up all sensors
	 */
	if(mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL )){
		return 2;//błąd wybudzenia czujników
	}
	///////////////////////////////////////////////////////////////////////////////

	/*
	 *  push both gyro and accel data into the FIFO
	 */
	if(mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL )){
		return 3;//bład konfiguracji fifo
	}
	////////////////////////////////////////////

	/*
	 * ustawienie sample rate
	 */
	if(mpu_set_sample_rate(DEFAULT_MPU_HZ)){
		return 4;//błąd ustawienia częstotliwosci próbkowania
	}
	//////////////////////////////////////
	/*
	 * dmp firmware loading
	 */
	int a = dmp_load_motion_driver_firmware();
	if(a){
		return a;//bład ładowania firmwareu
	}
	/*
	 * zapisywanie macierzy orientacji (czujnika w ukladzie ?)
	 */
	if(dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation))){
		return 5;//błąd inicjacji macierzy orientacji
	}
	////////////////////////////////////////////////

	/*
	 * konfiguracja DMP
	 */
	uart_putstring("Konfiguracja features DMP...\n");
	if(0==dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_SEND_RAW_ACCEL| DMP_FEATURE_SEND_CAL_GYRO | DMP_FEATURE_TAP)) uart_putstring("OK \r\n");
	////////////////////////////////////////////////////////////////////
	/*
	 *  czestotliwosc FIFO
	 */
	uart_putstring("Ustawienie fifo rate...\n");
	if(0==dmp_set_fifo_rate(DEFAULT_MPU_HZ)) uart_putstring("OK \r\n");
	/////////////////////////////////////////////////////

	/*
	 *  wlaczenie DMP
	 */
	uart_putstring("Wlaczenie DMP...\n");
	if(0==mpu_set_dmp_state(1)) uart_putstring("OK \r\n");
}




