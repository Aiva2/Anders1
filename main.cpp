/* BG96 NetworkSocketAPI Example Program
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "BG96Interface.h"
#include "TCPSocket.h"
#include "MQTTClient.h"
#include "MQTT_GSM.h"
#include <ctype.h>
//#include "x_nucleo_iks01a1.h"
#include "XNucleoIKS01A2.h"

#include "BG96.h"

#include "SLG46824Interface.h"
#include "SLG46824_driver.h"

//------------------------------------
// Hyperterminal default configuration
// 9600 bauds, 8-bit data, no parity
//------------------------------------
Serial pc(SERIAL_TX, SERIAL_RX); 
DigitalOut myled(LED1);
DigitalIn mybutton(USER_BUTTON);
bool quickstartMode = true;    

#define MQTT_MAX_PACKET_SIZE 300   
#define MQTT_MAX_PAYLOAD_SIZE 500 


#define ORG_QUICKSTART         							// comment to connect to play.internetofthings.ibmcloud.com
//#define SUBSCRIBE              							// uncomment to subscribe to broker msgs (not to be used with IBM broker) 

 // Configuration values needed to connect to IBM IoT Cloud
#define BROKER_URL ".messaging.internetofthings.ibmcloud.com";     
#ifdef ORG_QUICKSTART
	#define ORG "fw8hub"     								// connect to quickstart.internetofthings.ibmcloud.com/ For a registered connection, replace with your org 
	#define ID "NB-IoT-Test" //"NB-IoT-Sandwich"
	#define AUTH_TOKEN "utkVpEhUP85v+dElcH" //"@V6v55KkBkC@HsCyuG" //"P8V92!Dqg&BAjw_0!j" //"xC3Y+kp@?&IVvUUfCK"
	#define DEFAULT_TYPE_NAME "NB-IoT-Devices"
	#define TOPIC  "iot-2/evt/status/fmt/json" 
#else   // not def ORG_QUICKSTART
	#define ORG "pvko17"             						// connect to play.internetofthings.ibmcloud.com/ For a registered connection, replace with your org
	#define ID "testtype_112233445566"       		// For a registered connection, replace with your id
	#define AUTH_TOKEN "testtype_112233445566"	// For a registered connection, replace with your auth-token
	#define DEFAULT_TYPE_NAME "TestType"
	#define TOPIC   "iot-2/type/TestType/id/testtype_112233445566/evt/status/fmt/json" 
#endif


// network credential
#define APN   "prevas-internet01.com.attz" //"online.telia.se" //"m2m.com.attz"  //Telia: "online.telia.se", AT&T: "m2m.com.attz"
#define PASSW  ""
#define USNAME ""

#define TYPE DEFAULT_TYPE_NAME       // For a registered connection, replace with your type
#define MQTT_PORT 1883
#define MQTT_TLS_PORT 8883
#define IBM_IOT_PORT MQTT_PORT
    
char id[30] = ID;                 // mac without colons  
char org[12] = ORG;        
int connack_rc = 0; // MQTT connack return code
//const char* ip_addr = "11.12.13.14";
//char* host_addr = "11.12.13.14";
char sensor_id[50];
char type[30] = TYPE;
char auth_token[30] = AUTH_TOKEN; // Auth_token is only used in non-quickstart mode
bool netConnecting = false;
int connectTimeout = 1000;
bool mqttConnecting = false;
bool netConnected = false;
bool connected = false;
int retryAttempt = 0;
char subscription_url[MQTT_MAX_PAYLOAD_SIZE];

#define SENSOR_ENABLED		1
#define SENSOR_MODEL			2

#define FW_REV						"1.0b"

PressureSensor *pressure_sensor;
HumiditySensor *humidity_sensor;
TempSensor *temp_sensor1;

MQTT::Message message;
MQTTString TopicName={TOPIC};
MQTT::MessageData MsgData(TopicName, message);

/* Instantiate the expansion board */
static XNucleoIKS01A2 *mems_expansion_board = XNucleoIKS01A2::instance(D14, D15, D4, D5);

/* Retrieve the composing elements of the expansion board */
static LSM303AGRMagSensor *magnetometer = mems_expansion_board->magnetometer;
static HTS221Sensor *hum_temp = mems_expansion_board->ht_sensor;
static LPS22HBSensor *press_temp = mems_expansion_board->pt_sensor;
static LSM6DSLSensor *acc_gyro = mems_expansion_board->acc_gyro;
static LSM303AGRAccSensor *accelerometer = mems_expansion_board->accelerometer;

//Runi defined functions
int read_numinput(void);
int getMode(void);
void strInput(char str[], int nchars);
void continousTX(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE> client, MQTT_GSM ipstack);
void onDemandTX(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE> client, MQTT_GSM ipstack);
void enterATCommand(BG96Interface *bg96_if);


void subscribe_cb(MQTT::MessageData & msgMQTT) {
    char msg[MQTT_MAX_PAYLOAD_SIZE];
    msg[0]='\0';
    strncat (msg, (char*)msgMQTT.message.payload, msgMQTT.message.payloadlen);
    printf ("--->>> subscribe_cb msg: %s\n\r", msg);
}

int subscribe(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTT_GSM* ipstack)
{
    char* pubTopic = TOPIC;    
    return client->subscribe(pubTopic, MQTT::QOS1, subscribe_cb);
}

int connect(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTT_GSM* ipstack)
{ 
    const char* iot_ibm = BROKER_URL; 

    
    char hostname[strlen(org) + strlen(iot_ibm) + 1];
    sprintf(hostname, "%s%s", org, iot_ibm);
	

    // Construct clientId - d:org:type:id
    char clientId[strlen(org) + strlen(type) + strlen(id) + 5];  
	
		#ifdef ORG_QUICKSTART
    sprintf(clientId, "d:%s:%s:%s", org, type, id);  //@@
		#else
		sprintf(clientId, "g:%s:%s:%s", org, type, id);  //@@
		#endif
	
    sprintf(subscription_url, "%s.%s/#/device/%s/sensor/", org, "internetofthings.ibmcloud.com",id);
		
    netConnecting = true;
    ipstack->open(&ipstack->getGSM());
    int rc = ipstack->connect(hostname, IBM_IOT_PORT, connectTimeout);    
    if (rc != 0)
    {
        //WARN("IP Stack connect returned: %d\n", rc);    
        return rc;
    }
    pc.printf ("--->TCP Connected\n\r");
    netConnected = true;
    netConnecting = false;

    // MQTT Connect
    mqttConnecting = true;
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.struct_version=0;
    data.clientID.cstring = clientId;
 
    if (!quickstartMode) 
    {        
        data.username.cstring = "use-token-auth";
        data.password.cstring = auth_token;
    } 
        
    if ((rc = client->connect(data)) == 0) 
    {       
        connected = true;
        pc.printf ("--->MQTT Connected\n\r");
	#ifdef SUBSCRIBE
        if (!subscribe(client, ipstack)) printf ("--->>>MQTT subscribed to: %s\n\r",TOPIC);
	#endif           
    }
    else {
        //WARN("MQTT connect returned %d\n", rc);        
    }
    if (rc >= 0)
        connack_rc = rc;
    mqttConnecting = false;
    return rc;
}

int getConnTimeout(int attemptNumber)
{  // First 10 attempts try within 3 seconds, next 10 attempts retry after every 1 minute
   // after 20 attempts, retry every 10 minutes
    return (attemptNumber < 10) ? 3 : (attemptNumber < 20) ? 60 : 600;
}

void attemptConnect(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTT_GSM* ipstack)
{
    connected = false;
           
    while (connect(client, ipstack) != MQTT_CONNECTION_ACCEPTED) 
    {    
        if (connack_rc == MQTT_NOT_AUTHORIZED || connack_rc == MQTT_BAD_USERNAME_OR_PASSWORD) {
            printf ("File: %s, Line: %d Error: %d\n\r",__FILE__,__LINE__, connack_rc);        
            return; // don't reattempt to connect if credentials are wrong
        } 
        int timeout = getConnTimeout(++retryAttempt);
        //WARN("Retry attempt number %d waiting %d\n", retryAttempt, timeout);
        
        // if ipstack and client were on the heap we could deconstruct and goto a label where they are constructed
        //  or maybe just add the proper members to do this disconnect and call attemptConnect(...)        
        // this works - reset the system when the retry count gets to a threshold
        if (retryAttempt == 5){
						pc.printf ("\n\n\rFAIL!! system reset!!\n\n\r");
            NVIC_SystemReset();
				}
        else
            wait(timeout);
    }
}
float hum_global = 50.0;
uint32_t n_msg = 0;

int publish(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTT_GSM* ipstack)
{
    MQTT::Message message;
    char* pubTopic = TOPIC;
            
    char buf[MQTT_MAX_PAYLOAD_SIZE];
    float temp, temp1, temp2, press, hum;
	
	#if SENSOR_ENABLED
		pc.printf("A02 reading sensors...");

	    hum_temp->get_temperature(&temp1);
			hum_temp->get_humidity(&hum);
   
			press_temp->get_temperature(&temp2);
			press_temp->get_pressure(&press);
			temp = (temp1+temp2)/2;
		
	pc.printf(" DONE\r\n");
	#else
		temp=25.5;
		hum_global +=0.1;
		if (hum_global>99.0)
			hum_global = 50.0;
		hum=hum_global;
		press=999;
	#endif
		
	#ifdef ORG_QUICKSTART
    sprintf(buf,
     "{\"d\":{\"ST\":\"Nucleo-IoT-mbed\",\"Temp\":%0.4f,\"Pressure\":%0.4f,\"Humidity\":%0.4f}}",
              temp, press, hum);
	#else
		sprintf (buf, 
		"{\"%s\": {\"temp\":%0.4f,\"humidity\":%0.4f,\"pressure\":%0.4f,\"ambient\":0,\"uv\":0,\"accel_X\":0,\"accel_Y\":0,\"accel_Z\":0}}", 
				sensor_id, temp, hum, press);
	#endif
		
		message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf);
    
		//LOG("Publishing %s\n\r", buf);
		n_msg++;
    pc.printf("Publishing V%s #%d %s\n\r", FW_REV, n_msg, buf);
    return client->publish(pubTopic, message);
} 
    

int loop_count = 0;  
void test_sens(void);

int main()
{
    const char * apn = APN; // Network must be visible otherwise it can't connect
    const char * username = USNAME;
	const char * password = PASSW;
    BG96Interface bg96_if(D8, D2, false);
	//sprintf(sensor_id,"%s",bg96_if.get_mac_address()); 
	//Timer tyeld;
	
	//change serial baud to 115200
	pc.baud(115200);
	wait(0.1);
	
    myled=1;
		//wait(0.5);
    pc.printf("\r\n*************************************************");
	  wait( 0.1 ); 
		pc.printf("\r\nAvnet Silica NbIotBG96 A02 mbed-os application\r\n");  
    wait( 0.1 );   
    pc.printf("MBED online version %s\r\n", FW_REV);     
    wait( 0.1 );
    pc.printf("Software modified by Prevas...\r\n");
    wait( 0.5 );
    //pc.printf("\r\nwait for APN ready ...\r\n");  
    //wait( 0.1 );     
	
	 #if SENSOR_ENABLED
				/* Enable all sensors */
				hum_temp->enable();
				press_temp->enable();
				//magnetometer->enable();
				//accelerometer->enable();
				//acc_gyro->enable_x();
				//acc_gyro->enable_g();
	 #endif


   quickstartMode=false;
   if (strcmp(org, "quickstart") == 0){quickstartMode = true;}
   
   pc.printf("\r\nSetting up ipstack...\r\n");
   wait(1.0);
   
   MQTT_GSM ipstack(bg96_if, apn, username, password);
   MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE> client(ipstack);

   if (quickstartMode){
        char mac[50];  // remove all : from mac
        char *digit=NULL;
        pc.printf("quickstartMode\r\n");
        wait(1.0);  
        sprintf (id,"%s", "");                
        sprintf (mac,"%s",ipstack.getGSM().get_mac_address()); 
        digit = strtok (mac,":");
        while (digit != NULL)
        {
            strcat (id, digit);
            digit = strtok (NULL, ":");
        }     
   }
   
   pc.printf("Ipstack setup complete...\r\n");
   wait(1.0);
   
   pc.printf("attemptConnect...\r\n");
   wait(1);
   attemptConnect(&client, &ipstack);
   if (connack_rc == MQTT_NOT_AUTHORIZED || connack_rc == MQTT_BAD_USERNAME_OR_PASSWORD)    
   {
      while (true)
      wait(1.0); // Permanent failures - don't retry
   }
	myled=0;      
	sprintf(sensor_id,"%s",bg96_if.get_mac_address()); 
	pc.printf("while...\r\n"); 
	wait(1.0);
	
	while(true)
	{
		int mode = getMode(); //Command line interface
		
		if(mode == 1)
		{
		    continousTX(client, ipstack); //Enters continous transmission mode...
		}
		else if(mode == 2)
		{
		    onDemandTX(client, ipstack); //Enters transmission on demand mode...
		}
		else if(mode == 3)
		{
			enterATCommand(&bg96_if); //Forwards AT Commands to BG96 module... (send 'x' to exit)
		}
	}
}

void enterATCommand(BG96Interface *bg96_if)
{
	while(true)
	{
		pc.printf("\r\nEnter AT command:\r\n");
		
	    char str[150];
		strInput(str, 150);
		
		if(str[0] == 'x')
		{
			break;
		}
		
		bg96_if->send_at(str, true);
	}
}
	

void onDemandTX(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE> client, MQTT_GSM ipstack)
{
    pc.printf("\r\nPress (blue) button to send message...\r\n");

    while(1) 
    {
	    if(!mybutton) 
	    {
	        myled = !myled;

            if (publish(&client, &ipstack) != 0) { 
                myled=0;
                attemptConnect(&client, &ipstack);   // if we have lost the connection                
            }

		    wait(0.5);
		    pc.printf("\r\nPress (blue) button to send message...\r\n");
		}
	}
}

void continousTX(MQTT::Client<MQTT_GSM, Countdown, MQTT_MAX_PACKET_SIZE> client, MQTT_GSM ipstack)
{
    int num = 0;    
    
    pc.printf("\r\nChoose transmission interval between 0 and 9999\r\n");
    num = read_numinput();
    
    while (true) //Publishes every num seconds
    {
        if (++loop_count == num)
        {   	        
	        if (publish(&client, &ipstack) != 0) 
	        { 
                myled=0;
                attemptConnect(&client, &ipstack);   // if we have lost the connection                
        	} 
			myled=0;
            loop_count = 0;
        }        
		
    wait(1.0);
    //client.yield(1000);  // allow the MQTT client to receive messages
    
	pc.printf ("loop %d\r", (loop_count+1)); //Writes loop counter
	}
}

int getMode()
{
	while(true)
	{
		pc.printf("\r\nChoose mode:\r\n");
		pc.printf("1. Periodic transmission\r\n");
		pc.printf("2. Transmission on demand\r\n");
		pc.printf("3. Enter AT Command\r\n");
		//pc.printf("4. Get payload\r\n");
		//pc.printf("5. Set payload\r\n");
		
		int num = -1;
		
        num = read_numinput();
        
        if(num >= 1 && num <= 3)
        {
        	return num;
    	}
    	else
    	{
			pc.printf("Invalid input...\r\n");    		
		}	        	    
    }
}


int read_numinput(void)
{
	int i, sign;
	char c;
	char buff[5];
		
	i = 0;
	sign = 1;
	while(1)
	{
		if((c = getchar()) != 0x00)
		{
			if(c == 0x0D)
			{
				break;
			}
			else
			{
				if((c >= '0') && (c <= '9'))
				{
					buff[i++] = c;
					if(i >= 5) break;
				}
				else if(c == '-')
				{
					sign = -1;
				}
			}
			
		}
	};
	sscanf(buff,"%d",&i);
	return i*sign;
}

void strInput(char str[], int nchars) {
    int i = 0;

    char c;
    while((c = getchar()) != 0x0D && i <= (nchars-1)) 
    {
            str[i] = c;
            i++;
            if(c == '\b')
            {
            	i--;
        	}
    }
    str[i] = '\0';

    //printf("\r\n%s %d\r\n", str, (int)strlen(str));
}


/* Helper function for printing floats & doubles */
static char *print_double(char* str, double v, int decimalDigits=2)
{
  int i = 1;
  int intPart, fractPart;
  int len;
  char *ptr;

  /* prepare decimal digits multiplicator */
  for (;decimalDigits!=0; i*=10, decimalDigits--);

  /* calculate integer & fractinal parts */
  intPart = (int)v;
  fractPart = (int)((v-(double)(int)v)*i);

  /* fill in integer part */
  sprintf(str, "%i.", intPart);

  /* prepare fill in of fractional part */
  len = strlen(str);
  ptr = &str[len];

  /* fill in leading fractional zeros */
  for (i/=10;i>1; i/=10, ptr++) {
    if (fractPart >= i) {
      break;
    }
    *ptr = '0';
  }

  /* fill in (rest of) fractional part */
  sprintf(ptr, "%i", fractPart);

  return str;
}


		//for testing sensor board ...
void test_sens(void)
{	
		while(1)
		{
			
			  float value1, value2;
			  char buffer1[32], buffer2[32];
			  printf("\r\n");

			  hum_temp->get_temperature(&value1);
				hum_temp->get_humidity(&value2);
				printf("HTS221: [temp] %7s C,   [hum] %s%%\r\n", print_double(buffer1, value1), print_double(buffer2, value2));
    
				press_temp->get_temperature(&value1);
				press_temp->get_pressure(&value2);
				printf("LPS22HB: [temp] %7s C, [press] %s mbar\r\n", print_double(buffer1, value1), print_double(buffer2, value2));
				
				wait(2);
			
		}
}

