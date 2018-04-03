/**
  ******************************************************************************
  * @file	 CAN2UART.c
  * @author  Przemek
  * @version V1.0.0
  * @date    02.04.2018
  * @brief   Modu� dostarcza funkcji wirtualnego portu UART zbudowanego w oparciu o komunikacj� na UART
  * 		 Modu� rezerwuje do pracy jeden odbiorczy FIFO od CAN. Modu� tworzy abstrakcj� kana��w UART.
  * 		 Z ka�dym kana�em zwi�zany jest identyfikator ramki nadawzej oraz identyfikator ramki odbiorczej
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "OSCan.h"
#include "CAN2UART.h"
/* Private typedef -----------------------------------------------------------*/
typedef struct{
	int rxID;
	int txID;
	xQueueHandle rxQueue;
	xQueueHandle txQueue;
	xTaskHandle task;
}tCAN2UARTChannel;

typedef struct{
	tCAN2UARTChannel *channelArray;
	int maxNumberOfChannels;
	int channelCnt;
	int canFifoNumber;
	xTaskHandle task;
}tCAN2UART;
/* Private define ------------------------------------------------------------*/
#define CAN_CH_REF()	((tCAN2UARTChannel*)h)
#define CAN_H()			((tCAN2UART*)h)
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
void CAN2UART_RxTask(tCAN2UARTHandle h);
void CAN2UART_TxTask(tCAN2UARTChannelRef h);
void CAN2UART_Error(char* errorDescription);
/* Public  functions ---------------------------------------------------------*/
/**
  * @brief  Funkcja inicjuje modu�, tworzy w�tek odbiorczy odbieraj�cy ramki.
  * @param[in]  None
  * @retval None
  */
int CAN2UART_Init(tCAN2UARTHandle *h,tCAN2UARTConfig *cfg){
	//alokuje pami�c na modul
	tCAN2UART *c2u;
	c2u = pvPortMalloc(sizeof(tCAN2UART));
	//inicjuje zmienna
	c2u->channelCnt=0;
	c2u->maxNumberOfChannels=cfg->maxNumberOfChannels;
	c2u->canFifoNumber = cfg->canFifoNumber;
	c2u->channelArray = pvPortMalloc(sizeof(tCAN2UARTChannel)*cfg->maxNumberOfChannels);
	//inicjuje w�tek
	xTaskCreate(CAN2UART_RxTask,"can2uart",256,c2u,4,&c2u->task);
	*h = c2u;
	return 0;
}
/**
  * @brief  Funkcja inicjuje kana� komunikacyjny. Z kana�em zwi�zany jest w�tek nadawczy, pakuj�cy dane do ramki CAN i wysy�aj�cy je do CAN
  *         oraz rejestracja kolejki odbiorczej do przenoszenia danych odebranych na CAN a zwi�zanych z danym kana�em identyfikatorem ramki CAN
  * @note	Ka�dy kana� inicjuje jeden blok sprzetowych filtr�w CAN, wi�c mo�na powo�ac tylko okre�lon� ich liczb�
  * @param[in] h: referencja do obiektu modu�u
  * @param[out] chRef: referencja do uchwytu do kana�u
  * @param[in] cfg: wskaxnik do obiektu opisuj�cego konfiguracj� kana�u
  * @retval 0 - brak b��d�w
  */
int CAN2UART_CreateChannel(tCAN2UARTHandle h,tCAN2UARTChannelRef *chRef,tCAN2UARTChannelCfg *cfg){
	tCAN2UARTChannel *ch;
	tCanFilter f;
	int fid;
	//sprawdzam, czy mam wolne miejsce na kana�
	if(CAN_H()->channelCnt<CAN_H()->maxNumberOfChannels){
		//jest wolne miejsce
		ch = &CAN_H()->channelArray[CAN_H()->channelCnt++];
		*chRef = ch;
		//inicjuje filtr
		f.FIFOSelect = CAN_H()->canFifoNumber;
		f.enable=1;
		f.mode = eFilterListMode;
		f.scale = eStdFilterSize;
		f.filter[0].id.std=cfg->rxID;
		f.filter[0].ide=0;
		f.filter[0].rtr=0;
		f.filter[1].id.std=cfg->rxID;
		f.filter[1].ide=0;
		f.filter[1].rtr=0;
		f.mask[0].id.std=cfg->rxID;
		f.mask[0].ide=0;
		f.mask[0].rtr=0;
		f.mask[1].id.std=cfg->rxID;
		f.mask[1].ide=0;
		f.mask[1].rtr=0;
		if(OSCan_FilterInit(&f,&fid)){
			CAN2UART_Error("CAN2UART: B�ad inicjacji filtra warstwy CAN");
			return 2;
		}
		//ustawiam parametry
		ch->rxID = cfg->rxID;
		ch->txID = cfg->txID;
		//alokuje kolejki
		ch->rxQueue = xQueueCreate(cfg->rxQueueDepth,1);
		ch->txQueue = xQueueCreate(cfg->txQueueDepth,1);
		//tworze w�tek nadawczy
		xTaskCreate(CAN2UART_TxTask,"txCan2Uart",256,ch,4,&ch->task);
		return 0;
	}else{
		CAN2UART_Error("CAN2UART: Brak miejsca na kolejny kana�");
		return 1;
	}
	return -1;
}
/**
  * @brief  Funkcja odczytuje jeden znak z kolejki odbiorczej kana�u
  * @param[in] chRef: referencja do kana�u
  * @param[out] data: referencja do znaku
  * @param[in] timeut: maksymalny czas oczekiwania na znak
  * @retval 0 - brak b�edu
  */
int CAN2UART_Receive(tCAN2UARTChannelRef h,char* data,int timeout){
	if(xQueueReceive(CAN_CH_REF()->rxQueue,data,timeout)==pdTRUE){
		return 0;
	}else{
		return 1;
	}
}
/**
  * @brief  Funkcja �aduje dane do kolejki nadawczej kana�u
  * @param[in] chRef: referencja do kana�u
  * @param[in] data: referencja do danych
  * @param[in] dataSize: rozmiar danych
  * @param[in] timeut: maksymalny czas oczekiwania na wys�anie
  * @retval 0 - brak b�edu
  */
int CAN2UART_Transmit(tCAN2UARTChannelRef h,void* data,int dataSize,int timeout){
	char *tx=data;
	for(int i=0;i<dataSize;i++){
		if(xQueueSend(CAN_CH_REF()->txQueue,&tx[i],timeout)!=pdTRUE){
			//problem z wys�aniem znaku
			CAN2UART_Error("CAN2UART: Przepe�niono bufor nadawczy");
			return 1;
		}
	}
	return 0;
}
/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Funkcja w�tku odbiorczego. Funkcja odczytuje dane z CAN, a nast�pnie umieszcza je w odpowiedniej kolejce zwi�zanej z kana�em, o odpowiednim ID
  * @param[in] h: referencja do obiektu modu�u
  * @retval None
  */
void CAN2UART_RxTask(tCAN2UARTHandle h){
	tCAN2UART *c2u = (tCAN2UART*)h;
	tCanRxMessage rx;
	tCAN2UARTChannel *ch;
	while(1){
		//odczytuje dane z odpowiedniego fifo
		if(OSCan_ReceiveMessage(&rx,c2u->canFifoNumber,100)==0){
			//przeszukuje kana�y w poszukiwaniu odpowiedniego id
			for(int i=0;i<c2u->channelCnt;i++){
				ch = &c2u->channelArray[i];
				if(rx.id==ch->rxID){
					//znalaz�em odpowiednie ID, przekazuje dane
					for(int j=0;j<rx.dlc;j++){
						xQueueSend(ch->rxQueue,&rx.data[j],100);
					}
					break;
				}
			}
		}
	}
}
/**
  * @brief  Wielokrotna instacja w�tku odpowiedzialnego za odbieranie danych z kolejki nadawczej kana�u i �adowanie ich do ramki CAN
  * @param[in]  @param[in] h: referencja do obiektu modu�u
  * @retval None
  */
void CAN2UART_TxTask(tCAN2UARTChannelRef h){
	tCAN2UARTChannel *ch=(tCAN2UARTChannel*)h;
	int timeout = 100;
	int id=0;
	char c;
	tCanTxMessage tx;
	//konfiguruj� ramk�
	tx.id = ch->txID;
	tx.ide=0;
	tx.rtr=0;

	while(1){
		//odczytuje dane z kolejki
		if(xQueueReceive(ch->txQueue,&c,timeout)==pdTRUE){
			//odebra�em, umieszczam dane w ramce
			tx.data[id++]=c;
			if(id>=8){
				//skompletowa�em ramk�, mog� j� wysy�ac
				tx.dlc=8;
				OSCan_SendMessage(&tx,50);
				id = 0;
				timeout = 100;
			}else{
				//ramka nieskompletowana, ustawiam kr�tki czas oczekiwania
				timeout = 2;
			}
		}else{
			//timeout przekrocozny, sprawdzam, czy s� jakie� dane do wys�ania
			if(id>0){
				//s� jakie� dane do wys�lania
				tx.dlc = id;
				OSCan_SendMessage(&tx,50);
				id=0;
				timeout = 100;
			}
		}
	}
}

