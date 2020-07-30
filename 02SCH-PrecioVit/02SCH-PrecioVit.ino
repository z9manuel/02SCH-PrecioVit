/*
 Name:		_02SCH_PrecioVit.ino
 Created:	7/29/2020 9:17:35 PM
 Author:	mrodriguez
 Programa para despliegue de pracios en Vitrinas SUCAHERSA
*/


// Librreias SD
#include "FS.h"
#include "SD.h"
#include <SPI.h>
// Librerias Red
#include <ArduinoJson.h>
#include <DateTime.h>
#include <WiFi.h>
// Librerias para API
#include <HTTPClient.h>

#define SD_CS 5						// Define CS pin para modulo SD

File schFile;
boolean okSD = 0, okNET = 0;
boolean error = 0;
String servidorAPI;

String carnicareia;
byte tipo = 1;
String tiempo = "";

void setup() {
	Serial.begin(115200);
	iniciarMCU() == true ? Serial.println("MCU Listo!") : Serial.println("MCU Falla!");

}


void loop() {

}

boolean iniciarMCU() {
	/*
	Funcion responsable de realizar la configuración de red del MCU
	Toma como parametros las variables de micro SD para autoconfigurar SSID, Contrasena, MAC, Direcionamiento IP y el NTP
	*/
	byte  ipA, ipB, ipC, ipD;
	byte  gwA, gwB, gwC, gwD;
	byte  msA, msB, msC, msD;
	byte  dns1A, dns1B, dns1C, dns1D;
	byte  dns2A, dns2B, dns2C, dns2D;
	String red, contrred, apiser, cronos;
	String serNTP;

	// Iniciando SD
	Serial.println("Iniciando SD...");
	okSD = 1;
	SD.begin(SD_CS);
	if (!SD.begin(SD_CS)) {
		Serial.println("Error modulo SD!");
		okSD = 0;
	}
	uint8_t cardType = SD.cardType();
	if (cardType == CARD_NONE) {
		Serial.println("Error tarjeta SD!");
		okSD = 0;
	}
	if (!SD.begin(SD_CS)) {
		Serial.println("ERROR - Falla en tarjeta SD!");
		okSD = 0;
	}
	schFile = SD.open("/schconf.json", FILE_READ);
	if (schFile) {
		String confLine;
		while (schFile.available()) {
			confLine += schFile.readString();
		}
		schFile.close();
		char JSONMessage[confLine.length() + 1];
		confLine.toCharArray(JSONMessage, confLine.length() + 1);
		Serial.println(JSONMessage);

		DynamicJsonDocument doc(1024);
		DeserializationError error = deserializeJson(doc, JSONMessage);
		if (error) {
			Serial.print("Error en configuraciones!");
			Serial.println(error.c_str());
			okSD = 0;
		}

		ipA = int(doc["ipA"]); ipB = int(doc["ipB"]); ipC = int(doc["ipC"]); ipD = int(doc["ipD"]);
		gwA = int(doc["gwA"]); gwB = int(doc["gwB"]); gwC = int(doc["gwC"]); gwD = int(doc["gwD"]);
		msA = int(doc["msA"]); msB = int(doc["msB"]); msC = int(doc["msC"]); msD = int(doc["msD"]);
		dns1A = int(doc["dns1A"]); dns1B = int(doc["dns1B"]); dns1C = int(doc["dns1C"]); dns1D = int(doc["dns1D"]);
		dns2A = int(doc["dns2A"]); dns2B = int(doc["dns2B"]); dns2C = int(doc["dns2C"]); dns2D = int(doc["dns2D"]);
		String textwifi = doc["wifi"];
		String textpasswifi = doc["passwifi"];
		String textapi = doc["API"];
		String textntp = doc["NTP"];
		red = textwifi;
		contrred = textpasswifi;
		apiser = textapi;
		cronos = textntp;
		okSD = 1;
	}
	else {
		Serial.println("Error al abrir configuración!");
		return false;
	}


	if (okSD == 1) {
		okNET = 0;
		Serial.println("SD OK!");
		// Configuraciones de direccionamiento IP
		IPAddress local_IP(ipA, ipB, ipC, ipD);
		IPAddress gateway(gwA, gwB, gwC, gwD);
		IPAddress subnet(msA, msB, msC, msD);
		IPAddress DNS1(dns1A, dns1B, dns1C, dns1D);
		IPAddress DNS2(dns2A, dns2B, dns2C, dns2D);
		//uint8_t   mac[6]{ mac1, mac2, mac3, mac4, mac5, mac6 };
		char ssid[red.length() + 1];
		red.toCharArray(ssid, red.length() + 1);
		char password[contrred.length() + 1];
		contrred.toCharArray(password, contrred.length() + 1);
		char servidorNTP[cronos.length() + 1];
		cronos.toCharArray(servidorNTP, cronos.length() + 1);
		servidorAPI = apiser;

		WiFi.disconnect();
		delay(500);
		WiFi.begin(ssid, password);
		if (!WiFi.config(local_IP, gateway, subnet, DNS1, DNS2)) {
			Serial.println("Error al configurar IP...");
			okNET = 0;
		}
		delay(500);
		int i = 0;
		while (WiFi.status() != WL_CONNECTED) {
			WiFi.disconnect();
			Serial.println("Conectando a WiFi..");
			delay(500);
			WiFi.begin(ssid, password);
			if (!WiFi.config(local_IP, gateway, subnet, DNS1, DNS2))
				Serial.println("Error al configurar IP...");
			i == 9 ? ESP.restart() : delay(1000);
			i++;
		}
		Serial.print("WiFi OK: ");
		okNET = 1;
		Serial.println(local_IP);
		Serial.println("Buscando NTP...");
		DateTime.setTimeZone(-6);
		DateTime.setServer(servidorNTP);
		DateTime.begin();
		delay(500);
		int intento = 0;
		while (!DateTime.isTimeValid() && intento < 9) {
			Serial.println("NTP Error!");
			delay(500);
			intento++;
		}
		//Ajuste de horario de verano por mes
		String mes = "";
		String fecha = DateTime.toString();
		mes = mes + fecha[5]; mes = mes + fecha[6];
		((mes.toInt()) > 3 && (mes.toInt()) < 11) ? DateTime.setTimeZone(-5) : DateTime.setTimeZone(-6);
		DateTime.forceUpdate();
		Serial.print("Tiempo: ");
		Serial.println(DateTime.toString());
	}
	return okNET;
}

int enviar_a_API(String dato) {
	/*
	Funcion responsable de enviar paquetes formateados a servidor WebAPI mediante POST
	Toma de los parametros globales la dirección del servidor WebAPI
	Recibe como parametro una cadena de texto serializada en formato JSON
	*/
	boolean enviado = 0;
	int intentos = 9;
	String requestBody = dato;

	while (!enviado || intentos > 0)
	{
		if (WiFi.status() == WL_CONNECTED) {

			HTTPClient http;
			http.begin(servidorAPI);
			http.addHeader("Content-Type", "application/json");
			int httpResponseCode = http.POST(requestBody);
			delay(500);

			if (httpResponseCode > 0) {
				String response = http.getString();
				Serial.println(httpResponseCode);
				Serial.println(response);
				http.end();
				enviado = 1;
				intentos = 0;
				delay(1000);
			}
			else {
				Serial.println("Error al enviar HTTP POST");
				delay(500);
				if (intentos > 1) {
					Serial.println("Reintentando HTTP POST");
					delay(500);
					intentos--;
				}
				if (intentos == 1)
				{
					Serial.println("Almacenando en SD");
					iniciarMCU();
					intentos--;
				}
				http.end();
			}
		}
		else {
			Serial.println("WiFi no disponible!");
			Serial.println("Error al enviar HTTP POST");
			delay(500);
			iniciarMCU();
			delay(500);
			intentos--;
		}
	}

	return enviado ? 1 : 0;
}

bool SD_validar() {
	SD.begin(SD_CS);
	if (!SD.begin(SD_CS)) {
		Serial.println("Error modulo SD!");
		return false;
	}
	uint8_t cardType = SD.cardType();
	if (cardType == CARD_NONE) {
		Serial.println("Error tarjeta SD!");
		return false;
	}
	if (!SD.begin(SD_CS)) {
		Serial.println("ERROR - Falla en tarjeta SD!");
		return false;
	}
	return 1;
}

bool SD_leerLog() {
	if (SD_validar()) {
		File dataLog = SD.open("/log.txt", FILE_READ);
		if (dataLog) {
			bool enviado = 0;
			String linea;
			dataLog.position();
			while (dataLog.available()) {
				linea = dataLog.readStringUntil('\n');
				enviado = enviar_a_API(linea);
				if (enviado == false) {
					return false;
				}
			}
			dataLog.close();
			if (SD_borrarLog()) {
				return 1;
			}
		}
	}
	return false;
}

bool SD_escribirLog(String cadena) {
	if (SD_validar()) {
		SD.begin(SD_CS);
		File dataLog = SD.open("/log.txt", FILE_APPEND);
		if (dataLog) {
			dataLog.println(cadena);
			dataLog.close();
			Serial.println(cadena);
			return 1;
		}
	}
	return false;
}

bool SD_borrarLog() {
	if (SD_validar()) {
		SD.begin(SD_CS);
		if (SD.remove("/log.txt")) {
			Serial.println("Registro borrado.");
			schFile.close();
			return 1;
		}
	}
	return false;
}

String dato_a_JSON(String bfr_Tiempo[], float bfr_Temperatura[], int bfr_Humedad[]) {
	/*
	Funcion responsable de formatear los datos generados, modificar dependiendo de dispositivo
	Toma como parametros los datos generales de indentificación de la carnicería
	Y recibe como parametros los Buffers de tiempo, temperatura y humedad
	No utilizar Buffers de mas de 30 elementos
	*/
	String requestBody;
	delay(500);

	DynamicJsonDocument paquete(4096);
	StaticJsonDocument<1024> sensor;
	JsonArray cuerpoDatos = paquete.createNestedArray("cuerpoDatos");

	paquete["carniceria"] = carnicareia;
	paquete["fecha"] = DateTime.toString();;
	paquete["tipo"] = tipo;

	for (int i = 0; i < 30; i++)
	{
		sensor["hora"] = bfr_Tiempo[i];
		sensor["temperatura"] = bfr_Temperatura[i];
		sensor["humedad"] = bfr_Humedad[i];
		String cuerpo;
		serializeJson(sensor, cuerpo);
		cuerpoDatos.add(cuerpo);
	}
	serializeJson(paquete, requestBody);
	return requestBody;
}

