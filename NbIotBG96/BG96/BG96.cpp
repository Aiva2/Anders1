/* SPWFInterface Example
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

#include "BG96.h"
#include "mbed_debug.h"

//device id settings
#define ID_ON_FLASH		0

#if ID_ON_FLASH	
const uint8_t DEVICE_ID[6] = {0x00,0x80,0xe1,0xb7,0xd3,0x47};
#endif

//debug messages enable 
#define DBG_MSG				0

#define BG96_CONNECT_TIMEOUT    15000
#define BG96_SEND_TIMEOUT       500
#define BG96_RECV_TIMEOUT       500 //some commands like AT&F/W takes some time to get the result back!
#define BG96_MISC_TIMEOUT       1000
#define BG96_SOCKQ_TIMEOUT      8000

//#define DEBUG

extern  Serial pc;			//(SERIAL_TX, SERIAL_RX); 

Timer main_timer;

DigitalOut BG96_reset(D7);

DigitalOut VBAT_3V8_EN(D11);

//DigitalOut BG96_W_DISABLE(D11);

DigitalOut BG96_PWRKEY(D10);

extern DigitalOut myled;

BG96::BG96(PinName tx, PinName rx, PinName reset, PinName wakeup, bool debug)
    : _serial(tx, rx, 1024), _parser(_serial),
      _reset(reset, PIN_OUTPUT, PullNone, 1),
      _wakeup(wakeup, PIN_OUTPUT, PullNone, 0),
      dbg_on(debug)
{
    _serial.baud(115200);  // LICIO  FIXME increase the speed
    _parser.debugOn(debug);
}

static void delay_ms(uint32_t del)
{
	do{
			wait_ms(1);
	}while (del--);
	
}


char pdp_string[500];

void BG96::at_cops(void)
{
	int i = 0;
	char* search_pt;
	
	i = 0;
	memset(pdp_string, 0, sizeof(pdp_string));
	#ifdef DEBUG
		pc.printf("AT+COPS?\r\n");
	#endif
	_parser.send("AT+COPS?");
	while(1){
		//read and store answer
		_parser.read(&pdp_string[i], 1);
		#ifdef DEBUG
			pc.printf("%c",pdp_string[i]); 
		#endif
		i++;
		//if OK rx, end string; if not, program stops
		search_pt = strstr(pdp_string, "OK\r\n");
		if (search_pt != 0)
		{
			#ifdef DEBUG
				pc.printf("DONE\r\n\r\n");
			#endif
			break;
		}
		//TODO: add timeout if no aswer from module!!
	}	
	
}


//char pdp_string[500];
//This function sends an arabitary AT Command
//It returns true if the message is sent correctly
//And false if it is not sent correctly
bool BG96::send_at(char str[], bool printEnabled)
{
	int i = 0;
	char* search_pt;
	char tst_str[500];
	
	Timer timer_s;
	uint32_t time;
	uint32_t end_time = BG96_MISC_TIMEOUT*10;
	
	i = 0;
	memset(tst_str, 0, sizeof(tst_str));
	#ifdef DEBUG
		pc.printf("%s\r\n", str);
	#endif
	_parser.send("%s", str);
	
	timer_s.reset();
	timer_s.start();
	while(1){
		//read and store answer
		_parser.read(&tst_str[i], 1);
		#ifdef DEBUG
			pc.printf("%c",tst_str[i]); 
		#endif
		
		if(printEnabled) // This is a quick fix for being able to print stuff byt bit for all messages
		{
			pc.printf("%c",tst_str[i]);
		}
		
		i++;
		//if OK rx, end string; if not, program stops
		search_pt = strstr(tst_str, "OK\r\n");
		if (search_pt != 0)
		{
			#ifdef DEBUG
				pc.printf("DONE\r\n\r\n");
			#endif
			return true;
		}
		
		time = timer_s.read_ms();
		
		if(time > end_time)
		{
			#ifdef DEBUG
				pc.printf("Timeout has occurred...\r\n\r\n");
			#endif
			return false;
		}

	}	
}


//Checks whether the device is connected to the network
//Returns true if is roaming and false if it is unconnected
bool BG96::at_creg()
{
	int i = 0;
	char* search_pt;
	char* search_pt2;
	char tst_str[500];
	
	Timer timer_s;
	uint32_t time;
	uint32_t end_time = BG96_MISC_TIMEOUT*10;
	
	i = 0;
	memset(tst_str, 0, sizeof(tst_str));
	#ifdef DEBUG
		pc.printf("at+creg?\r\n");
	#endif
	_parser.send("at+creg?");
	
	timer_s.reset();
	timer_s.start();
	while(1){
		//read and store answer
		_parser.read(&tst_str[i], 1);
		#ifdef DEBUG
			pc.printf("%c",tst_str[i]); 
		#endif
		i++;
		//if OK rx, end string; if not, program stops
		search_pt = strstr(tst_str, "OK\r\n");
		
		search_pt2 = strstr(tst_str, "+CREG: 0,2");
		if (search_pt != 0)
		{
			if(search_pt2 == 0)
			{
				#ifdef DEBUG
					pc.printf("DONE\r\n\r\n");
				#endif
				return true;
			}
			else
			{
				return false;
			}
		}
		
		time = timer_s.read_ms();
		
		if(time > end_time)
		{
			#ifdef DEBUG
				pc.printf("Timeout has occurred...\r\n\r\n");
			#endif
			return false;
		}

	}	
}

//Startup Routine..
//Define USE_GSM if you would like to use GSM
#define USE_GSM
bool BG96::startup(int mode)
{
    //int i = 0;
	//char* search_pt;
    //Timer timer_s;
	//uint32_t time;
    
    _parser.setTimeout(BG96_MISC_TIMEOUT);
    /*Test module before reset*/
    waitBG96Ready();
    
    wait(1.0);
    
    pc.printf("\r\nCard information:\r\n");
   	send_at("ati", true); //Get card info
	
	pc.printf("\r\nIMEI:\r\n");
   	send_at("AT+CGSN", true); //Get card info
	   	
#ifdef USE_GSM
	send_at("at+qcfg=\"nwscanmode\",1", false); //<scanmode>: 0 = Automatic, 1 = GSM only, 3 = LTE only
	send_at("AT+QCFG=\"roamservice\",2,1", false); //Enables roaming immediately
	send_at("AT+QCFG=\"band\",00000001,1", false);	//Runs GSM 900 band
	send_at("AT+CREG=2", false); //Enable network registration and location information
#else
	send_at("at+qcfg=\"nwscanmode\",3", false); //<scanmode>: 0 = Automatic, 1 = GSM only, 3 = LTE only
	send_at("at+qcfg=\"nwscanseq\",030201", false); //NB-IoT = 03 -> LTE-M = 02 -> GSM
	send_at("at+qcfg=\"iotopmode\",1", false); //0 = LTE-M, 1 = NB-IoT, 2 = both
#endif

#if 1
   		 pc.printf("waiting for network...\r\n");
		//wait for 10 seconds to connect provider network
		//green led lamps 0,2 seconds
		uint8_t wloop = 0;
		while(1)
		{
			if (wloop >= (10/0.2))
				break;
			myled=0;
			wait_ms(100);
			myled=1;
			wait_ms(100);
			wloop++;
		}
#endif
  	
	myled=0;
	//only to see cops data ... no matching or check 
	while(!at_creg())
	{
		wait(10.0); //Wait 10 seconds to try again
	}

	at_cops();

	pc.printf("Startup successful, BG96 is connected to network...\r\n"); //Implement condition for this


    /*Reset parser buffer*/
    _parser.flush();
    return true;
}

bool BG96::hw_reset(void)
{
	  BG96_reset = 0;
	  BG96_PWRKEY = 0;
	  VBAT_3V8_EN = 0;
		
		delay_ms(200);
	
	  pc.printf("HW reset BG96\r\n");

    BG96_reset = 1;

    VBAT_3V8_EN = 1;

//    BG96_W_DISABLE = 0;

    BG96_PWRKEY = 1;
	
		delay_ms(300);

    BG96_reset = 0;

		delay_ms(5);

    return 1;
}

bool BG96::reset(void)
{
    pc.printf("\n\rInit BG96 ...\r\n");
    hw_reset();
    return true;
}

void BG96::waitBG96Ready(void)
{
    reset();
    pc.printf("Wait for ready...");

		//after HW reset,if no RDY from BG96 program stops 
		//green led lamps 0.5 seconds
    while(1) 
		{
        if (_parser.recv("RDY")) 
				{
						pc.printf("BG96 ready\r\n");
						myled = 0;
						return ;
        }
				if (myled == 0)
					myled = 1;
				else
					myled = 0;
    }

}



bool BG96::connect(const char *apn, const char *username, const char *password)
{
	int i = 0;
	char* search_pt;

	Timer timer_s;
	uint32_t time;

	int pdp_retry = 0;

	memset(pdp_string, 0, sizeof(pdp_string));
			
#if 1
	pc.printf("Checking APN ...\r\n");
	
	#ifdef DEBUG
		pc.printf("AT+QICSGP=1\r\n");
	#endif
	_parser.send("AT+QICSGP=1");
	while(1){
				//read and store answer
				_parser.read(&pdp_string[i], 1);
				#ifdef DEBUG
					pc.printf("%c",pdp_string[i]); 
				#endif
				i++;
				//if OK rx, end string; if not, program stops
    			search_pt = strstr(pdp_string, "OK\r\n");
				if (search_pt != 0)
				{
						#ifdef DEBUG
							pc.printf("DONE\r\n\r\n");
						#endif
						break;
				}
				//TODO: add timeout if no aswer from module!!
	}
	
	//compare APN name, if match, no store is needed
	search_pt = strstr(pdp_string, apn);
	if (search_pt == 0)
	{
		//few delay to purge serial ...
		delay_ms(2);
		#ifdef DEBUG
			pc.printf("Storing APN %s ...\r\n", apn);
		#endif
		//program APN and connection parameter only for PDP context 1, authentication NONE
		//TODO: add program for other context
		#ifdef DEBUG
			pc.printf("AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",0", &apn[0], &username[0], &password[0]);
		#endif
		if (!(_parser.send("AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",0", &apn[0], &username[0], &password[0])
        && _parser.recv("OK"))) 
				{
					return false;
				}
	}
	#ifdef DEBUG
		pc.printf("End APN check\r\n\n");
	#endif
	
#endif

#if 0
	i = 0;
	memset(pdp_string, 0, sizeof(pdp_string));
	#ifdef DEBUG
		pc.printf("AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",0", &apn[0], &username[0], &password[0]);
	#endif
	//_parser.send("AT+QICSGP=1,\"%s\"", &apn[0]);
	_parser.send("AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",0", &apn[0], &username[0], &password[0]);
	while(1){
		//read and store answer
		_parser.read(&pdp_string[i], 1);
		#ifdef DEBUG
			pc.printf("%c",pdp_string[i]); 
		#endif
		i++;
		//if OK rx, end string; if not, program stops
		search_pt = strstr(pdp_string, "OK\r\n");
		if (search_pt != 0)
		{
				break;
		}
		//TODO: add timeout if no a swer from module!!
	}
#endif

		//send_at("AT+QICSGP?");
	
		//activate PDP context 1 ...
		pc.printf("PDP activating ...\r\n");
		memset(pdp_string, 0, sizeof(pdp_string));
		i = 0;
		int a = 1;
		while(a==1)
		{
				#ifdef DEBUG
					pc.printf("AT+QIACT=1\r\n");
				#endif
				_parser.send("AT+QIACT=1");
				timer_s.reset();
				timer_s.start();
				while (1)
				{
					_parser.read(&pdp_string[i], 1);
					#ifdef DEBUG
						pc.printf("%c",pdp_string[i]); 
					#endif
					i++;
					if (_parser.recv("OK")){
						a=0;
						break;
					}
						
					time = timer_s.read_ms();
					uint32_t end_time = (BG96_MISC_TIMEOUT*(5+(pdp_retry*5)));
					//pc.printf("End time: %d\r\n", end_time);
					
					if (time > end_time) 
					{
						pdp_retry++;
						if (pdp_retry > 3)
							{
								pc.printf("ERROR --->>> PDP not valid, program stopped!!\r\n");
								pc.printf("***********************************************\r\n");
								
								at_cops();
		
								pc.printf("PDP started\r\n\n");
								
								//while (1);
								return true;
							}
						break;
					}
				}		
		}
		
		at_cops();
		
		pc.printf("PDP started\r\n\n");
    return true;
}

bool BG96::disconnect(void)
{
    //TODO add needed further action after ACT close command ...
    if(!(_parser.send("AT+QIDEACT=%d\r", 1) && _parser.recv("OK"))) {
        //debug_if(dbg_on, "BG96> error while disconnet...\r\n");
        return false;
    }
    //reset();
    return true;
}

const char *BG96::getIPAddress(void)
{
		//TODO this function needs further debug ... now always returns IP 0.0.0.0
    char act_string[32];
    //IPV4 mode
    uint8_t n1, n2, n3, n4;

    if (!(_parser.send("AT+QIACT?")
						//NOTE the %s on parser.read function returns only the first character of string ... 
						//TODO verify parser.read  function
						//here used only to check if commands is well processed by BG96
            && _parser.recv("+QIACT: %s", &act_string[0])
            && _parser.recv("OK"))) {
        //debug_if(dbg_on, "BG96> getIPAddress error\r\n");
        return 0;
    }
    //TODO parse string to find IP ...
    //now this functions returns always IPV4 0.0.0.0
    n1 = n2 = n3 = n4 = 0;
    sprintf((char*)_ip_buffer,"%u.%u.%u.%u", 	n1, n2, n3, n4);

    return _ip_buffer;
}


#define IMEI_OFFS			2
#define	IMEI_LEN 			14+IMEI_OFFS

const char *BG96::getMACAddress(void)
{
    uint32_t n1, n2, n3, n4, n5, n6;
	
  #if ID_ON_FLASH
    n1=DEVICE_ID[0];		//0x00;
    n2=DEVICE_ID[1];		//0x0b;
    n3=DEVICE_ID[2];		//0x57;
    n4=DEVICE_ID[3];		//0x55;
    n5=DEVICE_ID[4];		//0xdb;
    n6=DEVICE_ID[5];		//0x45;
  #else
    char sn[32];
    memset (sn, 0, 32);
    if (!(_parser.send("AT+CGSN")
            && _parser.read(&sn[0], IMEI_LEN)
            && _parser.recv("OK"))) {
        pc.printf("BG96> IMEI error\r\n");
        return 0;
    }
    n1=0x00;
    n2=0x0b;
    n3=(uint8_t)(((sn[4+IMEI_OFFS]&0x0f)<<4 | (sn[5+IMEI_OFFS]&0x0f)) + ((sn[12+IMEI_OFFS]&0x0f)<<4));
    n4=(uint8_t)(((sn[6+IMEI_OFFS]&0x0f)<<4 | (sn[7+IMEI_OFFS]&0x0f)) + (uint8_t)((sn[0+IMEI_OFFS]&0x0f)<<4 | (sn[1+IMEI_OFFS]&0x0f)));
    n5=(uint8_t)(((sn[8+IMEI_OFFS]&0x0f)<<4 | (sn[9+IMEI_OFFS]&0x0f)) + ((sn[13+IMEI_OFFS]&0x0f)));
    n6=(uint8_t)(((sn[10+IMEI_OFFS]&0x0f)<<4 | (sn[11+IMEI_OFFS]&0x0f)) + (uint8_t)((sn[2+IMEI_OFFS]&0x0f)<<4 | (sn[3+IMEI_OFFS]&0x0f)));
  #endif
		sprintf((char*)_mac_buffer,"%02X%02X%02X%02X%02X%02X", n1, n2, n3, n4, n5, n6);
		pc.printf("DEVICE ID is %s\r\n",_mac_buffer);
    return _mac_buffer;
}

bool BG96::isConnected(void)
{
    return getIPAddress() != 0;
}
uint8_t sock_id = 0;
bool socket_opening = false;

bool BG96::open(const char *type, int* id, const char* addr, int port)
{
    Timer timer;
    timer.start();
    socket_closed = 0;
    uint32_t time;
    bool res;
    char send_type[16];
    const char *send_type_pt = send_type;
    int result;

		//check socket type ...
    switch(*type) {
        case 'u':
            send_type_pt = "UDP";
						//UDP on sock id = 1
            sock_id = 1;
            break;
        case 't':
            send_type_pt = "TCP";
						//TCP on sock id = 0
            sock_id = 0;
            break;
        default:
            send_type_pt = "TCP";
						sock_id = 0;
            break;
    };
    _parser.setTimeout(BG96_SOCKQ_TIMEOUT);

		//open socket for context 1
    while(1) {
		
		#ifdef DEBUG
			pc.printf("AT+QIOPEN=1,%d,\"%s\",\"%s\",%d,0,0\r\n", sock_id, send_type_pt, addr, port);
		#endif
        if((_parser.send("AT+QIOPEN=1,%d,\"%s\",\"%s\",%d,0,0\r", sock_id, send_type_pt, addr, port)) 
					&& (_parser.recv("OK")))

        {

            while(1) 
						{
                res = _parser.recv("+QIOPEN: %d,%d", id, &result);
                if (res == true) 
				{
					#ifdef DEBUG					
                    	pc.printf("Open socket type %s #%d result = %d\r\n", send_type_pt, *id, result);
                    #endif
                    if (result == 0)
					{	
							//wait for socket open result ... start RECV timeout timer
							socket_opening = true;
							main_timer.reset();
							main_timer.start();
						  return true;
					}
					else
					{
					int s_id = *id;
                    //if (result == 563) 
                    close(s_id);
                    return false;
										}
									}

                time = timer.read_ms();
                if (time > BG96_CONNECT_TIMEOUT)
									{
                    pc.printf("Timeout sock open!!\r\n");
                    return false;
									}

           }

        }
        return false;
    }

}
uint8_t err_counter = 0;

bool BG96::send(int id, const void *data, uint32_t amount)
{
    char _buf[32];

		char	result_string[100];
		char* search_pt;
    _parser.setTimeout(BG96_SEND_TIMEOUT);

		//pclog.printf("Sending %d len ...", amount);
	
		//check if previous sent is good, otherwise after 5 consecutive fails close socket and error!!
		if (err_counter > 5)
		{
				pc.printf("Closing socket #%d\r\n", id);
				close(id);
				socket_closed = 1;
				err_counter = 0;
				return false;
		}		
	
		#if DBG_MSG


		uint8_t* dt = (uint8_t*)data;
		//these tests and print is only for debug purposes
		if (amount == 2)
		{
			if ((dt[0] == 0xc0) && (dt[1] == 0x00))
				pc.printf("\r\n\nPING ");
		}
		
		_parser.send("AT+COPS?");
    while (1) 
		{
        if (_parser.recv("OK")){
            break;
            }
		}

		#endif

		_parser.send("AT+QISEND=%d,0", id);
		#if 0
				err_counter = 0;
				while(1)
				{
					if( _parser.recv("+QISEND: %s", &result_string[0])
            && _parser.recv("OK")) 
						{
							break;
						}	
				}
		#else
			
		int i = 0;
		memset(result_string, 0, sizeof(result_string));
		const char OK_str[] = {"OK\r\n"};
		const char OK0_str[] = {",0"};
		while(1){
						//read and store answer
						_parser.read(&result_string[i], 1);
						i++;
						//if OK rx, end string; if not, program stops
            search_pt = strstr(result_string, OK_str);
						if (search_pt != 0)
						{
								break;
						}
						//TODO: add timeout if no aswer from module!!
    }
		
		//if send fails, the string doesn't have ",0" sequence, then 
		search_pt = strstr(result_string, OK0_str);		
		if (search_pt == 0)
		{
				err_counter++;
		}
		else
		{
				err_counter = 0;
		}
		
		#endif

		
		//here send data to socket ...
    sprintf((char*)_buf,"AT+QISEND=%d,%d\r", id, amount);

    //May take more try if device is busy
    for (unsigned i = 0; i < 3; i++) {
        if (_parser.write((char*)_buf, strlen(_buf)) >=0) {
            if (_parser.recv("> ")) {
                if ((_parser.write((char*)data, (int)amount) >= 0)
                        && (_parser.recv("SEND OK"))) 
									{
										#if DBG_MSG
										//print for debug
										pc.printf("Sent!\r\n");
										#endif
                    return true;
									}
            }
        }
        //if send fails, wait before further attempt
        pc.printf("Send retry #%d\r\n", (i+1));
        wait_ms(2000);
    }
		
		//FAIL to send data to socket, after 3 retry!!
		#if DBG_MSG
		//these test and print only for debug
		_parser.send("AT+COPS?");
    while (1) {
        if (_parser.recv("OK")){
            break;
            }
    }
		_parser.send("AT+QISTATE?");
    while (1) {
        if (_parser.recv("OK")){
            break;
            }
    }
		#endif
		
	  pc.printf("Closing socket #%d\r\n", id);
		close(id);
		socket_closed = 1;
    return false;

}

uint8_t local_data_store[16];
uint8_t local_amount = 0;
uint8_t local_pt = 0;

int32_t BG96::recv(int id, void *data, uint32_t amount)
{
    uint32_t recv_amount=0;
		uint32_t now_time;
		#if DBG_MSG
		uint8_t* dt = (uint8_t*)data;
		#endif

		//received data pending??
    if (local_amount > 0) {
        memcpy(data, &local_data_store[local_pt], amount);
        local_amount = local_amount-amount;
        local_pt = local_pt+amount;
				//return pending data...
        return amount;
    }
		
    local_amount = 0;
    local_pt = 0;


    if (socket_closed) {
        socket_closed = 0;
        return -3;
    }

		//wait dato from open socket request ... check timeout
		if (socket_opening == true)
		{
			now_time  = main_timer.read_ms();
			if (now_time > BG96_CONNECT_TIMEOUT)
				{
					 socket_opening = false;
					 pc.printf("-->>> SOCK tout\r\n");
					 main_timer.stop();
					 return -3;
				}
		}
		
    /*
    int par_timeout = _parser.getTimeout();
    _parser.setTimeout(0);
    _parser.setTimeout(par_timeout);
    */
    switch (id) {
        case 0:
            if (!(_parser.recv("+QIURC: \"recv\",0")))
                return -1;
            break;
        case 1:
            if (!(_parser.recv("+QIURC: \"recv\",1")))
                return -1;
            break;
        default:
            return -1;
            //break;
    }

    if ((_parser.send("AT+QIRD=%d,1000\r", id))  //send a query (will be required for secure sockets)
            && (_parser.recv("+QIRD: %u", &recv_amount))) {

        _parser.read((char*)data, recv_amount);
        while( _parser.recv("OK"));
				
			  #if DBG_MSG
				if ((recv_amount == 2) && (dt[0] == 0xd0) && (dt[1] == 0x00))
					pc.printf("PING Recv!\r\n");
				#endif
							
				//if data amount received more than expected, store it for next call of recv function
        if(recv_amount > amount) {
            local_amount = recv_amount-amount;
            local_pt = local_pt+amount;
            memcpy(local_data_store, data, recv_amount);
            recv_amount = amount;
        }
				socket_opening = false;
        return recv_amount;

    }
    return -2;

}

bool BG96::close(int id)
{
    uint32_t recv_amount=0;
    void * data = NULL;

    _parser.setTimeout(BG96_SOCKQ_TIMEOUT*2);
    _parser.flush();
    /* socket flush */
    if(!(_parser.send("AT+QIRD=%d,1000\r", id) //send a query (will be required for secure sockets)
            && _parser.recv("+QIRD: %u", &recv_amount))
            //&& _parser.recv("OK"))
      ) {
        return -2;
    }

    if (recv_amount>0) {
        data = malloc (recv_amount+4);
//       printf ("--->>>Close flushing recv_amount: %d  \n\r",recv_amount);
        if(!((_parser.read((char*)data, recv_amount) >0)
                && _parser.recv("OK"))) {
            free (data);
            return -2;
        }
        free (data);
    } else {
        if (!(_parser.recv("OK")))
            return -2;
    }

    //May take a second try if device is busy or error is returned
    for (unsigned i = 0; i < 2; i++) {
        if ((_parser.send("AT+QICLOSE=%d", id))
                && _parser.recv("OK")) {
            socket_closed = 1;
            return true;
        } else {
            if(_parser.recv("ERROR")) {
                debug_if(dbg_on, "BG96> ERROR!!!!\r\n");
                return false;
            }
        }
        //TODO: Deal with "ERROR: Pending data" (Closing a socket with pending data)
    }
    return false;

}


bool BG96::readable()
{
    return _serial.readable();
}

bool BG96::writeable()
{
    return _serial.writeable();
}

void BG96::pwrOn()
{
	BG96_PWRKEY = 0;
	wait(2);
	BG96_PWRKEY = 1;
}

void BG96::enablePowerSavingMode()
{
	int i = 0;
	char* search_pt;
	/*
	memset(pdp_string, 0, sizeof(pdp_string));
	pc.printf("AT+CPSMS=1\r\n");
	_parser.send("AT+CPSMS=1");
	while(1){
		//read and store answer
		_parser.read(&pdp_string[i], 1);
		pc.printf("%c",pdp_string[i]); 
		i++;
		//if OK rx, end string; if not, program stops
		search_pt = strstr(pdp_string, "OK\r\n");
		if (search_pt != 0)
		{
			pc.printf("DONE\r\n\r\n");
			break;
		}
		//TODO: add timeout if no aswer from module!!
	}
	
	i=0;
	memset(pdp_string, 0, sizeof(pdp_string));
	pc.printf("AT+CPSMS?\r\n");
	_parser.send("AT+CPSMS?");
	while(1){
		//read and store answer
		_parser.read(&pdp_string[i], 1);
		pc.printf("%c",pdp_string[i]); 
		i++;
		//if OK rx, end string; if not, program stops
		search_pt = strstr(pdp_string, "OK\r\n");
		if (search_pt != 0)
		{
			pc.printf("DONE\r\n\r\n");
			break;
		}
		//TODO: add timeout if no aswer from module!!
	}
	*/
	
	#if 1
	BG96_PWRKEY = 1;
	//wait(1);
	#endif
	
	#if 1
	i=0;
	memset(pdp_string, 0, sizeof(pdp_string));
	pc.printf("AT+QPOWD=1\r\n");
	_parser.send("AT+QPOWD=1");
	while(1){
		//read and store answer
		_parser.read(&pdp_string[i], 1);
		#ifdef DEBUG
			pc.printf("%c",pdp_string[i]); 
		#endif
		i++;
		//if OK rx, end string; if not, program stops
		search_pt = strstr(pdp_string, "OK\r\n");
		if (search_pt != 0)
		{
			#ifdef DEBUG
				pc.printf("DONE\r\n\r\n");
			#endif
			break;
		}
		//TODO: add timeout if no aswer from module!!
	}
	#endif
}

