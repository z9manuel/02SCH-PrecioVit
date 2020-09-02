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
#include <PubSubClient.h>
// Librerias para API
#include <HTTPClient.h>
#include <DHT.h>

#define SD_CS 5						// Define CS pin para modulo SD
#define PUERTA1 25
#define PUERTA2 33
#define PUERTA3 32
#define PUERTA4 35
bool p1Abierta = 0, p2Abierta = 0, p3Abierta = 0, p4Abierta = 0;
unsigned long millis_previos_p1 = 0, millis_previos_p2 = 0, millis_previos_p3 = 0, millis_previos_p4 = 0;

File schFile;
boolean okSD = 0, okNET = 0;
boolean error = 0;
String servidorAPI;
String servidorMQTT;
String servidorMQTTGlobal;
WiFiClient espClient;
WiFiClient espClientGlobal;
PubSubClient client(espClient);
PubSubClient clientGlobal(espClientGlobal);

String schAPI;
String carniceria;
String iddispositivo;
byte tipo = 1;
String tiempo = "";
String TopAvgTemp, topTemp1, topTemp2, topTemp3;
String TopAvgHum, topHum1, topHum2, topHum3;
String topPue1, topPue2, topPue3, topPue4;
bool debug = 0;

int ledRojo = 4;
int ledVerde = 2;
int ledAzul = 15;

String charola[30];
String articulo[30];
String nombre[30];
String menudeo[30];

#define BUZZER		12
#define DHTPIN1		14
#define DHTPIN2		27
#define DHTPIN3		26
#define DHTTYPE    DHT22
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);
int   h_avg, h1, h2, h3;                //Humedad
float t_avg, t1, t2, t3;                //Temperatura
int   pue1=0, pue2=0, pue3=0, pue4=0;  //Puertas



unsigned long millis_previos_precios = 0, millis_previos_activo = 0;
int inervalo_precios = 3600000, inervalo_activo = 60000;


void setup() {
	pinMode(ledRojo, OUTPUT);
	pinMode(ledVerde, OUTPUT);
	pinMode(ledAzul, OUTPUT);
	pinMode(BUZZER, OUTPUT);
	pinMode(PUERTA1, INPUT);
	pinMode(PUERTA2, INPUT);
	pinMode(PUERTA3, INPUT);
	pinMode(PUERTA4, INPUT);
	ledFalla();
	ledComunicacion();
	dht1.begin();
	dht2.begin();
	dht3.begin();
	Serial.begin(115200);
	debug = debugActivar();
	debug ? Serial.println("Debug activado!") : false;
	into();
	iniciarMCU() == true ? Serial.println("MCU Listo!") : Serial.println("MCU Falla!");
	ledOK();

	/*    QUITAR ESTE COMENTARIO	*/
	obtenerParametros();
	if (debug) {
		debug ? Serial.println("Productos seleccionados para busqueda de precio...\nARTICULO			NOMBRE			PRECIO") : false;
		for (int i = 0; i < 27; i++) {
			debug ? Serial.println(String(i + 1) + "\t" + articulo[i] + "\t\t\t" + nombre[i] + "\t\t\t" + menudeo[i]) : false;
		}
		Serial.println("Todos los productos  y precios actualizados con exito!");
	}
	
	leerTemperatura();
}

void loop() {

	revisarPuertas();
	// delay(10000);		//
	//leerTemperatura();	//
	//enviar_a_API(dato_a_JSON());	//
	//delay(1000);
	unsigned long millies_atcuales_activo = millis();
	if (millies_atcuales_activo - millis_previos_activo > inervalo_activo) {
		millis_previos_activo = millies_atcuales_activo;
		debug ? Serial.println("Ha pasado un minuto!") : false;
		leerTemperatura();

		if (!enviar_a_API(dato_a_JSON())) {
			debug ? Serial.println("Falla envio a API...") : false;
			ledFalla();
			SD_escribirLog(dato_a_JSON());
			debug ? Serial.println("Datos almacenados:") : false;
			if (debug)
				delay(5000);
		}
		else {
			debug ? Serial.println("Envio a API Ok!!!") : false;
			ledOK();
			debug ? Serial.println("Verificando existencia de registros sin enviar...") : false;
			SD_leerLog();
		}
	}

	unsigned long millies_atcuales_precios = millis();
	if (millies_atcuales_precios - millis_previos_precios > inervalo_precios) {
		millis_previos_precios = millies_atcuales_precios;
		
		obtenerParametros();
	}

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
		debug ? Serial.println(JSONMessage) : false;

		DynamicJsonDocument doc(1024);
		DeserializationError error = deserializeJson(doc, JSONMessage);
		if (error) {
			Serial.print("Error en configuraciones!");
			Serial.println(error.c_str());
			okSD = 0;
		}

		String carni = doc["carniceria"];
		byte dev_tipo = doc["tipodispositivo"];
		String device = doc["iddispositivo"];
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
		String mosquitto = doc["MQTT"];
		String mosquittoGlobal = doc["MQTTGlobal"];
		String tempAVG = doc["avgTemp"];
		String temp1 = doc["topTem1"];
		String temp2 = doc["topTem2"];
		String temp3 = doc["topTem3"];
		String humeAVG = doc["avgHum"];
		String hume1 = doc["topHum1"];
		String hume2 = doc["topHum2"];
		String hume3 = doc["topHum3"];
		String puerta1 = doc["topPue1"];
		String puerta2 = doc["topPue2"];
		String puerta3 = doc["topPue3"];
		String puerta4 = doc["topPue4"];
		carniceria = carni;
		tipo = dev_tipo;
		iddispositivo = device;
		servidorMQTT = mosquitto;
		servidorMQTTGlobal = mosquitto;
		TopAvgTemp= tempAVG, topTemp1 = temp1;  topTemp2 = temp2; topTemp3 = temp3;
		TopAvgHum= humeAVG, topHum1 = hume1; topHum2 = hume2; topHum3= hume3;
		topPue1 = puerta1; topPue2 = puerta2; topPue3 = puerta3; topPue4 = puerta4;
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

		char mqtt[servidorMQTT.length() + 1];
		servidorMQTT.toCharArray(mqtt, servidorMQTT.length() + 1);
		debug ? Serial.println(mqtt) : false;
		client.setServer(mqtt, 1883);

		client.connect("SUCAHERSA");
		client.setKeepAlive(180);
		Serial.print("Estado de MQTT de arranque: ");
		Serial.println(client.state());


		char mqttGlobal[servidorMQTTGlobal.length() + 1];
		servidorMQTTGlobal.toCharArray(mqttGlobal, servidorMQTTGlobal.length() + 1);
		debug ? Serial.println(mqttGlobal) : false;
		clientGlobal.setServer(mqttGlobal, 1883);
		
		clientGlobal.connect("SUCAHERSA_Global");
		clientGlobal.setKeepAlive(180);
		Serial.print("Estado de MQTT Global de arranque: ");
		Serial.println(clientGlobal.state());

		delay(5000);
	}
	return okNET;
}

String dato_a_JSON() {
	/*
	Funcion responsable de formatear los datos generados, modificar dependiendo de dispositivo
	Toma como parametros los datos generales de indentificación de la carnicería
	Y recibe como parametros los Buffers de tiempo, temperatura y humedad
	No utilizar Buffers de mas de 30 elementos
	https://httpbin.org/anything
	*/
	String requestBody;
	delay(500);

	StaticJsonDocument<4096> paquete;
	//DynamicJsonDocument paquete(4096);
	//StaticJsonDocument<1024> sensor;
	//JsonArray cuerpoDatos = paquete.createNestedArray("cuerpoDatos");

	paquete["carniceria"] = carniceria;
	paquete["tipodispositivo"] = tipo;
	paquete["iddispositivo"] = iddispositivo;
	paquete["fecha"] = DateTime.toString();

	paquete["h_avg"] = h_avg;
	paquete["h1"] = h1;
	paquete["h2"] = h2;
	paquete["h3"] = h3;
	paquete["t_avg"] = t_avg;
	paquete["t1"] = t1;
	paquete["t2"] = t2;
	paquete["t3"] = t3;
	paquete["pue1"] = pue1;
	paquete["pue2"] = pue2;
	paquete["pue3"] = pue3;
	paquete["pue4"] = pue4;

	/*
	for (int i = 0; i < 30; i++)
	{
		sensor["hora"] = bfr_Tiempo[i];
		sensor["temperatura"] = bfr_Temperatura[i];
		sensor["humedad"] = bfr_Humedad[i];
		String cuerpo;
		serializeJson(sensor, cuerpo);
		cuerpoDatos.add(cuerpo);
	}
	*/

	serializeJson(paquete, requestBody);
	debug ? Serial.println("Cadena a enviar: ") : false;
	debug ? Serial.println(requestBody) : false;
	return requestBody;
}

int enviar_a_API(String dato) {
	/*
	Funcion responsable de enviar paquetes formateados a servidor WebAPI mediante POST
	Toma de los parametros globales la dirección del servidor WebAPI
	Recibe como parametro una cadena de texto serializada en formato JSON
	*/
	debug ? Serial.println("Entrando a función enviar_a_API") : false;
	boolean enviado = 0;
	int intentos = 9;
	String requestBody = dato;

	while (!enviado || intentos > 0)
	{
		if (WiFi.status() == WL_CONNECTED) {
			debug ? Serial.print("Servidor API: ") : false;
			debug ? Serial.println(servidorAPI) : false;

			HTTPClient http;
			http.begin(servidorAPI);
			http.addHeader("Content-Type", "application/json");
			int httpResponseCode = http.POST(requestBody);
			delay(1000);

			debug ? Serial.print("\nhttpResponseCode: ") : false;
			debug ? Serial.println(httpResponseCode) : false;
			if (httpResponseCode > 0) {
				String response = http.getString();
				//Serial.println(httpResponseCode);
				debug ? Serial.println(response) : false;
				
				http.end();
				enviado = 1;
				intentos = 0;
				ledOK();
				delay(100);
			}
			else {
				debug ? Serial.println("Error al enviar HTTP POST") : false;
				beep(1);
				
				if (intentos > 1) {
					ledFalla();
					debug ? Serial.println("Reintentando HTTP POST") : false;
					delay(100);				
				}
				if (intentos == 1)
				{
					ledFalla();
					debug ? Serial.println("Almacenando en SD") : false;
					delay(500);
					if (httpResponseCode == -1)
					{
						ledFalla();
						http.end();
						return 0;
					}
				}

				intentos--;
				http.end();
				delay(500);
			}
		}
		else {
			debug ? Serial.println("WiFi no disponible!") : false;
			debug ? Serial.println("Error al enviar HTTP POST") : false;
			beep(1);
			delay(500);
			ledFalla();
			iniciarMCU();
			intentos--;
		}
	}

	return enviado ? 1 : 0;
}

boolean obtenerParametros() {
	/*
	Funcion responsable de obener precios oficiales SUCAHERSA
	Toma como parametros las variables de micro SD del archivo charola.json 
	*/

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
	schFile = SD.open("/charolas.json", FILE_READ);
	if (schFile) {
		String confLine;
		while (schFile.available()) {
			confLine += schFile.readString();
		}
		schFile.close();
		char JSONMessage[confLine.length() + 1];
		confLine.toCharArray(JSONMessage, confLine.length() + 1);
		debug ? Serial.println(JSONMessage) : false;

		
		DynamicJsonDocument doc(1024);
		DeserializationError error = deserializeJson(doc, JSONMessage);
		if (error) {
			Serial.print("Error en configuraciones!");
			Serial.println(error.c_str());
			okSD = 0;
		}
		String textapi = doc["schAPI"];
		schAPI = textapi;

		String art1 = doc["charola01"];
		articulo[0]= art1;
		String nomb1 = doc["nombre01"];
		nombre[0]= nomb1;

		String art2 = doc["charola02"];
		articulo[1] = art2;
		String nomb2 = doc["nombre02"];
		nombre[1] = nomb2;

		String art3 = doc["charola03"];
		articulo[2] = art3;
		String nomb3 = doc["nombre03"];
		nombre[2] = nomb3;

		String art4 = doc["charola04"];
		articulo[3] = art4;
		String nomb4 = doc["nombre04"];
		nombre[3] = nomb4;

		String art5 = doc["charola05"];
		articulo[4] = art5;
		String nomb5 = doc["nombre05"];
		nombre[4] = nomb5;

		String art6 = doc["charola06"];
		articulo[5] = art6;
		String nomb6 = doc["nombre06"];
		nombre[5] = nomb6;

		String art7 = doc["charola07"];
		articulo[6] = art7;
		String nomb7 = doc["nombre07"];
		nombre[6] = nomb7;

		String art8 = doc["charola08"];
		articulo[7] = art8;
		String nomb8 = doc["nombre08"];
		nombre[7] = nomb8;

		String art9 = doc["charola09"];
		articulo[8] = art9;
		String nomb9 = doc["nombre09"];
		nombre[8] = nomb9;

		String art10 = doc["charola10"];
		articulo[9] = art10;
		String nomb10 = doc["nombre10"];
		nombre[9] = nomb10;

		String art11 = doc["charola11"];
		articulo[10] = art11;
		String nomb11 = doc["nombre11"];
		nombre[10] = nomb11;

		String art12 = doc["charola12"];
		articulo[11] = art12;
		String nomb12 = doc["nombre12"];
		nombre[11] = nomb12;

		String art13 = doc["charola13"];
		articulo[12] = art13;
		String nomb13 = doc["nombre13"];
		nombre[12] = nomb13;

		String art14 = doc["charola14"];
		articulo[13] = art14;
		String nomb14 = doc["nombre14"];
		nombre[13] = nomb14;

		String art15 = doc["charola15"];
		articulo[14] = art15;
		String nomb15 = doc["nombre15"];
		nombre[14] = nomb15;

		String art16 = doc["charola16"];
		articulo[15] = art16;
		String nomb16 = doc["nombre16"];
		nombre[15] = nomb16;

		String art17 = doc["charola17"];
		articulo[16] = art17;
		String nomb17 = doc["nombre17"];
		nombre[16] = nomb17;

		String art18 = doc["charola18"];
		articulo[17] = art18;
		String nomb18 = doc["nombre18"];
		nombre[17] = nomb18;

		String art19 = doc["charola19"];
		articulo[18] = art19;
		String nomb19 = doc["nombre19"];
		nombre[18] = nomb19;

		String art20 = doc["charola20"];
		articulo[19] = art20;
		String nomb20 = doc["nombre20"];
		nombre[19] = nomb20;

		String art21 = doc["charola21"];
		articulo[20] = art21;
		String nomb21 = doc["nombre21"];
		nombre[20] = nomb21;

		String art22 = doc["charola22"];
		articulo[21] = art22;
		String nomb22 = doc["nombre22"];
		nombre[21] = nomb22;

		String art23 = doc["charola23"];
		articulo[22] = art23;
		String nomb23 = doc["nombre23"];
		nombre[22] = nomb23;

		String art24 = doc["charola24"];
		articulo[23] = art24;
		String nomb24 = doc["nombre24"];
		nombre[23] = nomb24;

		String art25 = doc["charola25"];
		articulo[24] = art25;
		String nomb25 = doc["nombre25"];
		nombre[24] = nomb25;

		String art26 = doc["charola26"];
		articulo[25] = art26;
		String nomb26 = doc["nombre26"];
		nombre[25] = nomb26;

		String art27 = doc["charola27"];
		articulo[26] = art27;
		String nomb27 = doc["nombre27"];
		nombre[26] = nomb27;
		okSD = 1;

		for (int i = 0; i < 27; i++)
			menudeo[i] = obtenerPrecio_API(articulo[i]);
	}
	else {
		Serial.println("Error al abrir configuración!");
		return false;
	}
	return okNET;
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
				debug ? Serial.println("Enviando registro en almacenamiento local...") : false;
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
		else
			debug ? Serial.println("Nada por enviar!") : false;
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

bool debugActivar() {
	if (SD_validar()) {
		File dataLog = SD.open("/debug", FILE_READ);
		if (dataLog) {
			debug ? Serial.println("El dispositivo esta en modo DEBUG.") : false;
			dataLog.close();
			ledOK();
			return true;
		}
	}
	return false;
}

String obtenerPrecio_API(String dato) {
	/*
	Funcion responsable de enviar paquetes formateados a servidor WebAPI mediante POST
	Toma de los parametros globales la dirección del servidor WebAPI
	Recibe como parametro una cadena de texto serializada en formato JSON
	*/

//				https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-get-2

	boolean enviado = 0;
	int intentos = 9;
	String response;
	String precio;
	String producto;
	dato.replace(" ", "%20");
	int httpResponseCode;
	String schServer = schAPI + dato;
	debug ? Serial.println("GET: " + schServer) : false;


	while (!enviado || intentos > 0)
	{
		if (WiFi.status() == WL_CONNECTED) {

			HTTPClient http;
			http.begin(schServer);
			//String payload = "{}";
			httpResponseCode = http.GET();
			ledComunicacion();
			delay(500);

			if (httpResponseCode > 0) {
				response = http.getString();
				debug ? Serial.println(httpResponseCode): false;
				debug ? Serial.println(response): false;
				http.end();
				enviado = 1;
				intentos = 0;
				delay(1000);
			}
			else {
				Serial.println("Error al enviar HTTP POST api SUCAHERSA");
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

	if (httpResponseCode == 200) {
		DynamicJsonDocument respuesta(1024);
		DeserializationError error = deserializeJson(respuesta, response);
		if (error) {
			debug ? Serial.print("Error en configuraciones!") : false;
			Serial.println(error.c_str());
			okSD = 0;
		}
		String textResponse = respuesta["response"];
		debug ? Serial.println(textResponse): false;

		error = deserializeJson(respuesta, textResponse);
		if (error) {
			Serial.print("Error en configuraciones!");
			Serial.println(error.c_str());
			okSD = 0;
		}
		String Menudeo = respuesta["Menudeo"];
		precio = "$ " + String(Menudeo.toFloat());
		debug ? Serial.println("El precio es: " + precio): false;


	}
	else {
		Serial.println("Error al abrir configuracion en un precio!");
		return "No disponible";
	}
	httpResponseCode == 200 ? ledComunicacion() : ledFalla();
	return httpResponseCode==200 ? precio : "No disponible";
}

void ledOK() {
	digitalWrite(ledRojo, LOW);
	digitalWrite(ledVerde, LOW);
	digitalWrite(ledAzul, LOW);
	for (int i = 1; i <= 3; i++) {
		digitalWrite(ledVerde, HIGH);
		delay(150);
		digitalWrite(ledVerde, LOW);
		delay(100);
	}
	digitalWrite(ledVerde, LOW);
}

void ledFalla() {
	digitalWrite(ledRojo, LOW);
	digitalWrite(ledVerde, LOW);
	digitalWrite(ledAzul, LOW);
	for (int i = 1; i <= 3; i++) {
		digitalWrite(ledRojo, HIGH);
		delay(150);
		digitalWrite(ledRojo, LOW);
		delay(100);
	}
	digitalWrite(ledRojo, LOW);
}

void ledComunicacion() {
	digitalWrite(ledRojo, LOW);
	digitalWrite(ledVerde, LOW);
	digitalWrite(ledAzul, LOW);
	for (int i = 1; i <= 5; i++) {
		digitalWrite(ledAzul, HIGH);
		delay(100);
		digitalWrite(ledAzul, LOW);
		delay(50);
	}
	digitalWrite(ledAzul, LOW);
}

bool leerTemperatura() {
	bool estatus = 0;
	h1 = dht1.readHumidity();
	t1 = dht1.readTemperature();
	h2 = dht2.readHumidity();
	t2 = dht2.readTemperature();
	h3 = dht3.readHumidity();
	t3 = dht3.readTemperature();
	if (isnan(h1) || isnan(t1)) {
		debug ? Serial.println("No se peuede leer temperatura/humedad sensor 01") : false;
		estatus = 0;
		delay(2000);
	}
	if (isnan(h2) || isnan(t2)) {
		debug ? Serial.println("No se peuede leer temperatura/humedad sensor 02") : false;
		estatus = 0;
		delay(2000);
	}
	if (isnan(h3) || isnan(t3)) {
		debug ? Serial.println("No se peuede leer temperatura/humedad sensor 03") : false;
		estatus = 0;
		delay(2000);
	}
	debug ? Serial.println("Tiempo		Humedad") : false;
	debug ? Serial.println(String(t1) + " °C\t\t" + String(h1) + " %") : false;
	debug ? Serial.println(String(t2) + " °C\t\t" + String(h2) + " %") : false;
	debug ? Serial.println(String(t3) + " °C\t\t" + String(h3) + " %") : false;
	estatus = 1;
	estatus ? ledOK() : ledFalla();
	

	debug ? Serial.print("Estado de MQTT: ") : false;
	debug ? Serial.println(client.state()) : false;
	

	if (client.state() != 0) {
		ledFalla();
		debug ? Serial.println("Reconectando MQTT...") : false;
		reconnect();
	}

	if (client.state() == 0) {
		char t1String[8];
		dtostrf(t1, 1, 2, t1String);
		char tempe1[topTemp1.length() + 1];
		topTemp1.toCharArray(tempe1, topTemp1.length() + 1);
		client.publish(tempe1, t1String);

		char t2String[8];
		dtostrf(t2, 1, 2, t2String);
		char tempe2[topTemp2.length() + 1];
		topTemp2.toCharArray(tempe2, topTemp2.length() + 1);
		client.publish(tempe2, t2String);

		char t3String[8];
		dtostrf(t3, 1, 2, t3String);
		char tempe3[topTemp3.length() + 1];
		topTemp3.toCharArray(tempe3, topTemp3.length() + 1);
		client.publish(tempe3, t1String);

		char h1String[8];
		dtostrf(h1, 1, 2, h1String);
		char humed1[topHum1.length() + 1];
		topHum1.toCharArray(humed1, topHum1.length() + 1);
		client.publish(humed1, h1String);

		char h2String[8];
		dtostrf(h2, 1, 2, h2String);
		char humed2[topHum2.length() + 1];
		topHum2.toCharArray(humed2, topHum2.length() + 1);
		client.publish(humed2, h2String);

		char h3String[8];
		dtostrf(h3, 1, 2, h3String);
		char humed3[topHum3.length() + 1];
		topHum3.toCharArray(humed3, topHum3.length() + 1);
		client.publish(humed3, h3String);

		h_avg = (h1 + h2 + h3) / 3;
		char HString[8];
		dtostrf(h_avg, 1, 2, HString);
		char humed[TopAvgHum.length() + 1];
		TopAvgHum.toCharArray(humed, TopAvgHum.length() + 1);
		clientGlobal.publish(humed, HString);

		t_avg = (t1+t2+t3)/3;
		char TString[8];
		dtostrf(t_avg, 1, 2, TString);
		char temper[TopAvgTemp.length() + 1];
		TopAvgTemp.toCharArray(temper, TopAvgTemp.length() + 1);
		clientGlobal.publish(temper, TString);
	}
	
	return estatus;
}

void beep(int nivel) {
	int tiempo = nivel * 100;
	debug ? Serial.println("En Beep " + String(nivel)): false;
	for (int i = 0; i < (3 * nivel); i++) {
		digitalWrite(ledRojo, HIGH);
		digitalWrite(BUZZER, HIGH);
		delay(tiempo);
		digitalWrite(ledRojo, LOW);
		digitalWrite(BUZZER, LOW);
		delay(tiempo);
	}
}

bool revisarPuertas() {
	bool p1, p2, p3, p4;

	/*
	bool p1Abierta = 0, p2Abierta = 0, p3Abierta = 0, p4Abierta = 0;;
	unsigned long millis_previos_p1 = 0, millis_previos_p2 = 0, millis_previos_p3 = 0, millis_previos_p4 = 0;
	int inervalo_precios = 3600000, inervalo_activo = 60000;
	*/

	unsigned long deltaP1 = 0, deltaP2 = 0, deltaP3 = 0, deltaP4 = 0;
	unsigned long millies_atcuales_p1 = millis(), millies_atcuales_p2 = millis(), millies_atcuales_p3 = millis(), millies_atcuales_p4 = millis();

	p1 = digitalRead(PUERTA1);
	p2 = digitalRead(PUERTA2);
	p3 = digitalRead(PUERTA3);
	p4 = digitalRead(PUERTA4);

	debug ? Serial.println("Estado de puertas.") : false;
	debug ? Serial.println(p1 ? "P1 CERRADO" : "P1 ABIERTO") : false;
	debug ? Serial.println(p2 ? "P2 CERRADO" : "P2 ABIERTO") : false;
	debug ? Serial.println(p3 ? "P3 CERRADO" : "P3 ABIERTO") : false;
	debug ? Serial.println(p4 ? "P4 CERRADO" : "P4 ABIERTO") : false;

	!p1 ? deltaP1 = (millies_atcuales_p1 - millis_previos_p1) / 1000 : millis_previos_p1 = millies_atcuales_p1;
	debug ? Serial.println("Segundos P1 abierta: " + String(deltaP1)) : false;
	!p2 ? deltaP2 = (millies_atcuales_p2 - millis_previos_p2) / 1000 : millis_previos_p2 = millies_atcuales_p2;
	debug ? Serial.println("Segundos P2 abierta: " + String(deltaP2)) : false;
	!p3 ? deltaP3 = (millies_atcuales_p3 - millis_previos_p3) / 1000 : millis_previos_p3 = millies_atcuales_p3;
	debug ? Serial.println("Segundos P3 abierta: " + String(deltaP3)) : false;
	!p4 ? deltaP4 = (millies_atcuales_p4 - millis_previos_p4) / 1000 : millis_previos_p4 = millies_atcuales_p4;
	debug ? Serial.println("Segundos P4 abierta: " + String(deltaP4)) : false;

	p1Abierta = p1;
	p2Abierta = p2;
	p3Abierta = p3;
	p4Abierta = p4;
	
	deltaP1 == 0 ? deltaP1++ : false;
	deltaP2 == 0 ? deltaP2++ : false;
	deltaP3 == 0 ? deltaP3++ : false;
	deltaP4 == 0 ? deltaP4++ : false;

	pue1 = deltaP1;
	pue2 = deltaP2;
	pue3 = deltaP3;
	pue4 = deltaP4;

	if ((deltaP1 % 300 == 0) || (deltaP2 % 300 == 0) || (deltaP3 % 300 == 0) || (deltaP4 % 300 == 0))
		beep(5);
	else if ((deltaP1 % 240 == 0) || (deltaP2 % 240 == 0) || (deltaP3 % 240 == 0) || (deltaP4 % 240 == 0))
		beep(4);
	else if ((deltaP1 % 180 == 0) || (deltaP2 % 180 == 0) || (deltaP3 % 180 == 0) || (deltaP4 % 180 == 0))
		beep(3);
	else if ((deltaP1 % 120 == 0) || (deltaP2 % 120 == 0) || (deltaP3 % 120 == 0) || (deltaP4 % 120 == 0))
		beep(2);
	else if ((deltaP1 % 60 == 0) || (deltaP2 % 60 == 0) || (deltaP3 % 60 == 0) || (deltaP4 % 60 == 0)) {
		beep(1);
		if ((deltaP1 > 305) || (deltaP2 > 305) || (deltaP3 > 305) || (deltaP4 > 305))
			beep(5);
	}
		
	if (client.state() != 0) {
		reconnect();
	}
	if (client.state() == 0) {

		char p1String[8];
		dtostrf(p1Abierta, 1, 2, p1String);
		char puert1[topPue1.length() + 1];
		topPue1.toCharArray(puert1, topPue1.length() + 1);
		client.publish(puert1, p1String);

		char p2String[8];
		dtostrf(p2Abierta, 1, 2, p2String);
		char puert2[topPue2.length() + 1];
		topPue2.toCharArray(puert2, topPue2.length() + 1);
		client.publish(puert2, p2String);

		char p3String[8];
		dtostrf(p3Abierta, 1, 2, p3String);
		char puert3[topPue3.length() + 1];
		topPue3.toCharArray(puert3, topPue3.length() + 1);
		client.publish(puert3, p3String);

		char p4String[8];
		dtostrf(p4Abierta, 1, 2, p4String);
		char puert4[topPue4.length() + 1];
		topPue4.toCharArray(puert4, topPue4.length() + 1);
		client.publish(puert4, p4String);
	}
	return 1;
}

void reconnect() {

	char mqtt[servidorMQTT.length() + 1];
	servidorMQTT.toCharArray(mqtt, servidorMQTT.length() + 1);
	debug ? Serial.println(mqtt) : false;
	client.setServer(mqtt, 1883);

	client.connect("SUCAHERSA");
	client.setKeepAlive(180);
	debug ? Serial.print("Estado de MQTT de arranque: ") : false;
	debug ? Serial.println(client.state()) : false;
	

	char mqttGlobal[servidorMQTTGlobal.length() + 1];
	servidorMQTTGlobal.toCharArray(mqttGlobal, servidorMQTTGlobal.length() + 1);
	debug ? Serial.println(mqttGlobal) : false;
	clientGlobal.setServer(mqttGlobal, 1883);

	clientGlobal.connect("SUCAHERSA_Global");
	clientGlobal.setKeepAlive(180);
	debug ? Serial.print("Estado de MQTT Global de arranque: ") : false;
	debug ? Serial.println(clientGlobal.state()) : false;
	
	

	int i = 0;
	/*

	Serial.println("Iniciando reconexión MQTT...");
	servidorMQTT.toCharArray(mqtt, servidorMQTT.length() + 1);
	char mqtt[servidorMQTT.length() + 1];
	debug ? Serial.println(mqtt) : false;
	client.setServer(mqtt, 1883);

	client.connect("SUCAHERSA");
	client.setKeepAlive(180);
	Serial.println(client.state());
	*/

	while (!client.connected()) {
		Serial.print("Intentando enlazar MQTT...");
		if (client.connect("SUCAHERSA")) {
			Serial.println("connected");
		}
		else {
			client.disconnect();
			Serial.println(client.state());
			Serial.print("Falla, rc=");
			Serial.println(client.state());
			Serial.println(" intentando en 5 seconds");
			client.disconnect();
			client.connect("SUCAHERSA");
			Serial.println(client.state());
			i++;
			if (i == 1) {
				Serial.println("Omitiendo conexión a MQTT...");
				delay(500);
				break;
			}
			delay(100);
		}
	}
}

void into() {
	Serial.println("Iniciando...");
	delay(2000);
	Serial.println("\n");
	Serial.println("CIATEC, A.C.");
	Serial.println("DIRECCION DE INVESTIGACION Y SOLUCIONES TECNOLOGICAS");
	Serial.println("SERVICIOS TECNOLOGICOS DE APOYO A LA SALUD");
	Serial.println("SALUD 4.0");
	Serial.println("www.ciatec.mx");
	Serial.println("\nSistema de monitoreo de estados en vitrina.");
	debug ? Serial.println("mrodriguez@ciatec.mx") : false;
	Serial.println("\n\n\n");
	delay(2000);
}
