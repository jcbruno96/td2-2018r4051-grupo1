#include "../inc/main.h"

xQueueHandle colarx;
xQueueHandle colatx;
xQueueHandle colaConexion;

xTaskHandle vUartReadHandle;
xTaskHandle vProcessConectionHandle;

int flag = 0;
char resp[BUFFERSIZE] = {0};
char respuesta[BUFFERSIZE] = {0};

unsigned char inicializado = 0;
unsigned int comandoCargado = 0;
unsigned char revisaInic = 0;

int mode = 0;

char codigoHTML[HTML_CODE_SIZE] = "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\" /><title>Compost TDII</title>"
		"<style>.sangria{text-indent: 30px;}</style><style>.textoAzul{color: blue; font-style: italic; font-weight: bold}"
		"</style></head><body><center><h1>Proyecto Técnicas Digitales II - Sistema de Compost</h1></center><br />"
		"<h2 class=\"textoAzul\">Temperatura:</h2><h1 class=\"sangria\">24ºC</h1><br /><h2 class=\"textoAzul\">Humedad:</h2>"
		"<h1 class=\"sangria\">56%</h1><br /></body></html>\r\n";

char connectionHTML1[313] = "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\"/><title>Compost TDII</title>"
		"<style>.sangria{text-indent: 30px;}</style><style>.textoAzul{color: blue; font-style: italic; font-weight: bold}"
		"</style></head><body><center><h1>Conexión exitosa</h1></center><br/>"
		"<center><h3 class=\"textoAzul\">Dirijase a <a>";

char connectionHTML2[32] = "</a></h3></center></body></html>\r\n";

char formHTML[HTML_FORM_SIZE] = "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>Conexión</title></head><body>"
		"<div class=\"container mt-5\"><h1>Conexión a la red</h1><form id=\"formulario\">"
		"<input type=\"text\" name=\"net\" placeholder=\"Ingrese la red\" class=\"form-control mt-3\"><br><br>"
		"<input type=\"password\" name=\"pass\" placeholder=\"Ingrese la contraseña\" class=\"form-control mt-3\"><br>"
		"<br><button class=\"btn btn-primary mt-4\" type=\"submit\">Conectarse</button></form></div></body></html>\r\n";

char connectionHTML[358] = {0};


void vConfigEsp8266(void *parametros){

	espStatus_e rv = ESP8266_NO_ANSWER;
	static char respuesta[BUFFERSIZE] = {0};
	int i = 0;


	while(1){

		/*Se elimina el ECO*/
		if(esp8266SendCommand("ATE0\r\n") == -1){
			//Ocurrio un error
		}

		/*Desconectar del AP*/
		//if(esp8266SendCommand("AT+CWQAP\r\n") == -1){
			//Ocurrio un error
		//}

		/*Se activan ambos modos: AP-ST*/
		if(esp8266SendCommand("AT+CWMODE=3\r\n") == -1){
			//Ocurrio un error
		}

		/*Habilito multiples conexiones*/
		if(esp8266SendCommand("AT+CIPMUX=1\r\n") == -1){
			//Ocurrio un error
		}

		/*Creo el server en el puerto 80*/
		if(esp8266SendCommand("AT+CIPSERVER=1,80\r\n") == -1){
			//Ocurrio un error
		}

		/*Se consulta si esta conectado a una red*/
		esp8266Command("AT+CWJAP?\r\n");
		while(1){
			if(xQueueReceive(colarx,&(respuesta[i]), portMAX_DELAY) == pdTRUE){
				rv = esp8266ValidateResponse(respuesta);
				resp[i] = respuesta[i];
				if(rv == ESP8266_NO_AP){
					mode = ACCESS_POINT_MODE;
				}
				else if(rv == ESP8266_AP){
					mode = STATION_MODE;
				}

				if(rv == ESP8266_OK)
					break;

				if(i>BUFFERSIZE-1)
					i = 0;

				i++;
			}
		}
		i=0;
		rv = ESP8266_NO_ANSWER;
		inicializarVector(BUFFERSIZE, respuesta);


		vTaskResume(vUartReadHandle);
		vTaskResume(vProcessConectionHandle);
		vTaskSuspend(NULL);

	}

}

void vAnswerProcess(void *parametros){

	espAnswer respuesta;
	respuesta.tipo = ESP8266_NOK;
	respuesta.id = -1;
	char sendCommand[100] = {0};
	int tam = 0;
	portBASE_TYPE xQueue;

	while(1){

		if(uxQueueMessagesWaiting(colaConexion) > 0){

			xQueue = xQueueReceive(colaConexion,&respuesta, portMAX_DELAY);
			//vTaskDelay(10/portTICK_RATE_MS);
			if(xQueue == pdTRUE){

				switch(respuesta.tipo){

					case ESP8266_NOK:
						break;

					case ESP8266_AP_CONNECTION:
						esp8266Command("AT+CIFSR\r\n");
						break;

					case ESP8266_ACCESS_POINT:
						vTaskDelay(1000/portTICK_RATE_MS);
						connectionToAccessPoint(respuesta);
						break;

					case ESP8266_CONNECTED:

						switch(mode){
							case STATION_MODE:
								tam = sizeof(codigoHTML)-1;
								break;
							case ACCESS_POINT_MODE:
								tam = sizeof(formHTML)-1;
								break;
							case CONNECTION_OK:
								tam = sizeof(connectionHTML)-1;
								break;

						}

						if(respuesta.id >= 0){
							sprintf(sendCommand, "AT+CIPSEND=%d,%d\r\n", respuesta.id, tam);
							esp8266Command(sendCommand);
						}

						break;

					case ESP8266_RESPONSE:

						switch(mode){

							case STATION_MODE:
								esp8266Command(codigoHTML);
								break;
							case ACCESS_POINT_MODE:
								esp8266Command(formHTML);
								break;
							case CONNECTION_OK:
								mode = STATION_MODE;
								esp8266Command(connectionHTML);
								break;
						}

					break;

					case ESP8266_CLOSE:
						if(respuesta.id >= 0){
							sprintf(sendCommand, "AT+CIPCLOSE=%d\r\n", respuesta.id);
							esp8266Command(sendCommand);
						}
						break;
				}

			}
		}
	}

}

//Creo una tarea que cada 10ms se fija si hay datos para leer.
void vUartRead(void *parametros)
{
	int i = 0, j = -1;
	espAnswer response;
	response.tipo = ESP8266_NOK;
	response.id = -1;
	inicializarVector(50, response.net);
	inicializarVector(40, response.pass);
	inicializarVector(40, response.ip);
	portBASE_TYPE xQueue;

	while(1)
	{

		if(!inicializado) continue;

		xQueue = xQueueReceive(colarx,&(respuesta[i]), 60000/portTICK_RATE_MS);

		if(xQueue == pdTRUE){

			if(i < BUFFERSIZE){
				resp[i] = respuesta[i];

				//Detecto el id del dispositivo conectado
				esp8266ProcessConnection(respuesta[i], &(response.id));

				if(response.id >= 0){

					j = esp8266JoinAccessPoint(respuesta[i], &response);

					//favicon - conexion invalida
					if(j == 0){
						response.tipo = ESP8266_NOK;
						response.id = -1;
					}
					//Conexion sin GET
					else if(j == 1){
						response.tipo = ESP8266_CONNECTED;
						xQueueSend(colaConexion, &response, portMAX_DELAY);
					}
					//Conexion con GET
					else if(j == 2){
						response.tipo = ESP8266_ACCESS_POINT;
						xQueueSend(colaConexion, &response, portMAX_DELAY);
					}

					//Esperando datos para enviar
					else if(respuesta[i] == '>'){
						response.tipo = ESP8266_RESPONSE;
						xQueueSend(colaConexion, &response, portMAX_DELAY);
					}

					//Recibe WIFI GOT IP\r\n\r\nOK\r\n
					else if(esp8266SuccessfulConnection(respuesta[i]) > 0){
						//Se consulta la STA-IP a la cual conectarse
						response.tipo = ESP8266_AP_CONNECTION;
						xQueueSend(colaConexion, &response, portMAX_DELAY);
					}

					//Detecta STAIP,"192...."
					else if(esp8266GetStationIP(response.ip, respuesta[i]) > 0){
						response.tipo = ESP8266_CONNECTED;
						sprintf(connectionHTML, "%s%s%s", connectionHTML1, response.ip, connectionHTML2);
						mode = CONNECTION_OK;
						xQueueSend(colaConexion, &response, portMAX_DELAY);
					}
					//Detecta SEND OK
					else if(esp8266DataSent(respuesta[i]) > 0){
						response.tipo = ESP8266_CLOSE;
						i = 0;
						flag = 0;
						inicializarVector(BUFFERSIZE, respuesta);
						xQueueSend(colaConexion, &response, portMAX_DELAY);
						inicializarVector(50, response.net);
						inicializarVector(40, response.pass);
						response.id = -1;
					}


				}

			}
			else{
				i = 0;
				flag=0;
				inicializarVector(BUFFERSIZE, respuesta);
			}

			i++;
			flag++;

		}
		else if(xQueue == errQUEUE_EMPTY){
			response.id = -1;
			inicializarVector(50, response.net);
			inicializarVector(40, response.pass);
			i = 0;
			flag = 0;
			inicializarVector(BUFFERSIZE, respuesta);
		}

	}

}

int main(void)
{
	initHardware();

	colarx = xQueueCreate(BUFFERSIZE, sizeof(char));
	colatx = xQueueCreate(BUFFERSIZE, sizeof(char));
	colaConexion = xQueueCreate(10, sizeof(espAnswer));

	//Tarea que se fija si hay datos para leer.
	xTaskCreate(vUartRead, (const unsigned char * ) "Leer UART", 5*configMINIMAL_STACK_SIZE,
					0, tskIDLE_PRIORITY+2, &vUartReadHandle );

	//Tarea que se fija si hay datos para leer.
	xTaskCreate(vAnswerProcess, (const unsigned char * ) "Procesar respuesta", 5*configMINIMAL_STACK_SIZE,
					0, tskIDLE_PRIORITY+1, &vProcessConectionHandle );


	xTaskCreate(vConfigEsp8266, (const unsigned char * ) "Config esp8266", 5*configMINIMAL_STACK_SIZE,
					0, tskIDLE_PRIORITY+3, 0 );

	vTaskSuspend(vProcessConectionHandle);
	vTaskSuspend(vUartReadHandle);

	vTaskStartScheduler();

	while (1);
}
