/*====================================================================== *\
 *  <  文件名  >:     main.c
 *  <  描  述  >:     shi mai yong xin co.
 *  < 函数列表 >:     
                         
 *  <  版  本  >:     1.00
 *  <  作  者  >:     
 *  <  日  期  >:     
 \*======================================================================*/
 #include <math.h>

void delay(int tim);

extern cregister volatile unsigned int IE;
extern cregister volatile unsigned int IF;
extern cregister volatile unsigned int ST;
extern cregister volatile unsigned int IOF;
extern  volatile unsigned long int00;
extern  volatile unsigned long tint1;

#define busctrl              ((volatile unsigned long * )0x808064   )
#define time0            	((volatile unsigned long * )0x0808020   )  
#define time1            	((volatile unsigned long * )0x0808030   ) 
#define vect_src            ((volatile unsigned long * )0x0809fc0   )

//peripheral address
#define IQP_LOW 0xE0000004	//low freq positive flow address
#define IQN_LOW 0xE0000005	//low freq negtive flow address
#define IQP_HIGH 0xE0000006	//low freq positive flow address
#define IQN_HIGH 0xE0000007	//low freq positive flow address
#define SFIFO_DATA1 0xE0000020	//serial port 1 data address
#define SFIFO_BAUND1 0xE0000021	//serial port 1 baund address
#define SFIFO_CTL1 0xE0000020	//serial port 1 control address
#define SFIFO_DATA2 0xE0000030	//serial port 2 data address
#define SFIFO_BAUND2 0xE0000031	//serial port 2 baund address
#define SFIFO_CTL2 0xE0000030	//serial port 2 control address
#define FLASH_BASE_ADDR 0x400000	//flash base address
#define FLASH_PAR_ADDR 0x430000	//参数保存地址

//parameters related defination
#define SEND_DATA_OVER_TIME 100	//send data over time counter
#define SEND_DATA_MAX_CIRCLE 16909320	//max send data circle: 0x7fffffff / 0x7f
#define FLASH_PAR_HEAD1 0xaa
#define FLASH_PAR_HEAD2 0x55
#define FLASH_PAR_HEAD3 0x69
#define FLASH_PAR_HEAD4 0x96
#define FLASH_PAR_FRAME_LENGTH 27
#define DATA_FRAME_SEND_PORT_INDEX 0
#define WXYZ_READ_PORT_INDEX 1

//send frame defination
#define SEND_FRAME_LENGTH 28
//#define SEND_FRAME_TYPE_DATA 1	//data frame
//#define SEND_FRAME_TYPE_MSG 2	//message frame

//message code defination
#define MSGCODE_SYSTEM_INIT_FAIL 1
#define MSGCODE_SYSTEM_INIT_DATA_ERROR 2
#define MSGCODE_DATA_OUTPUT 4
//message data defination
#define ERRDATA_PARAMETER_TABLE_ERROR 1
#define ERRDATA_PARAMETER_SEND_CIRCLE_ERROR 2
#define ERRDATA_PARAMETER_WXYZ_READ_PORT_ERROR 4
#define ERRDATA_PARAMETER_DATA_FRAME_SEND_PORT_ERROR 8
#define ERRDATA_PARAMETER_SFIFO_CTL_ERROR 0x10
#define ERRDATA_PARAMETER_SFIFO_BAUND_ERROR 0x20
#define ERRDATA_INIT_INSTALL_ANGEL_FAIL 0x40

//global parameters
int g_lowf_data = 0;	//low frequnce data sum
int g_highf_data = 0;	//high frequnce data sum
int g_send_circle = 1;	//send circle--to control data sending frequnce, default: send data in each interrupt circle
int g_send_counter = 0;	//send circle counter, +1 in each interrupt
unsigned int g_msg_code = 0;	//global message code
unsigned int g_msg_data = 0;	//global message data
int g_wxyz_read_port = WXYZ_READ_PORT_INDEX;
int g_frame_send_port = DATA_FRAME_SEND_PORT_INDEX;
//system init flag, 0=undone, else=done
unsigned int g_system_init_flag = 0;
volatile int g_timer_flag = 0;
//device number
char g_device_num[2] = {'0', '0'};
//system key
char g_system_key[2] = {'0', '0'};
float g_device_install_angelw[3] = {0.0, 0.0, 0.0};
float g_flow_velocity[2] = {0.0, 0.0};

int g_send_buf[32] = {'I',//header
'0', '0',//device number
'0', '0',//system key
'0', '0', '0', '0',//low frequence data, mm/s
'0', '0', '0', '0',//high frequence data, mm/s
'0', '0', '0', '0',//place holder, first 2 decimal use as msg_type_byte, last 2 decimal use as msg_data_byte
'0', '1', '9', '9',//quality string, set as 0199
'0', '0', '0', '0',//checksum
';', '\r', '\n'}; //end
volatile unsigned int *sfifo_port_regs[2][3] = {
{(volatile unsigned int *)SFIFO_DATA1, (volatile unsigned int *)SFIFO_BAUND1, (volatile unsigned int *)SFIFO_CTL1}, 
{(volatile unsigned int *)SFIFO_DATA2, (volatile unsigned int *)SFIFO_BAUND2, (volatile unsigned int *)SFIFO_CTL2}};

int myStrlen(const char *cbuf){
	int retval = 0;
	
	while((*cbuf) != 0){
		++retval;
	}
	
	return retval;
}

int i2a(unsigned int value, char *cbuf, int radix){
	char tmpbuf[16];
	int retval = 0;
	unsigned int tmpui, rest;
	
	while(value > 0){
		tmpui = value / radix;
		rest = value - tmpui * radix;
		if(rest < 10){	//get value mod radix
			tmpbuf[retval++] = '0' + rest;
		}else tmpbuf[retval++] = 'A' + rest - 10;
		value = tmpui;	//value = value / 10
	}
	//copy reversely
	for(tmpui = 0; tmpui < retval; ++tmpui){
		cbuf[tmpui] = tmpbuf[retval-tmpui-1];
	}
	cbuf[retval] = 0;
	
	return retval;
}

void i2str(int value, int *strbuf, int radix, int buflen){
	char tmpbuf[16] = {0};
	int tmpi, i;
	
	i2a(value, tmpbuf, radix);
	tmpi = myStrlen(tmpbuf);
	if(tmpi < buflen){//fill '0'
		for(i = 0; i < buflen-tmpi; ++i){
			strbuf[i] = '0';
		}
	}else i = 0;
	for(tmpi = 0; i < buflen; ++i, ++tmpi){
		strbuf[i] = (int)tmpbuf[tmpi];
	}
}

int sfifo_init(const unsigned int *cbuf, int port_ix){
	volatile unsigned int *ctl_regp;
	volatile unsigned int *baund_regp;
	unsigned int tmpui;
	int retval = 0;

	ctl_regp = sfifo_port_regs[port_ix][2];
	baund_regp = sfifo_port_regs[port_ix][1];
	//TODO: check register meaning
	//set control register default value
	//read: even parity, 1 stop bit
	//write: no parity, 1 stop bit
	*ctl_regp = 0;
	//set parity bit
	if(cbuf[0] == 1){//odd
		tmpui = 0x30;
	}else if(cbuf[0] == 2){//even
		tmpui = 0x10;
	}else{//unknown, set default value
		tmpui = 0;
		if(cbuf[0] != 0)
			retval = ERRDATA_PARAMETER_SFIFO_CTL_ERROR;
	}
	//set stop bit
	if(cbuf[1] == 1){//2 stop bit
		tmpui |= 0x40;
	}else if(cbuf[1] != 0){//unknown, set default value
		//tmpui |= 0;
		retval = ERRDATA_PARAMETER_SFIFO_CTL_ERROR;
	}
	//set baund register
	//TODO: complete this part
	tmpui = cbuf[2] + (cbuf[3]<<8) + (cbuf[4]<<16) + (cbuf[5]<<24);
	switch(tmpui){
		case 9600:
		break;
		case 115200:
		break;
		default:
		retval |= ERRDATA_PARAMETER_SFIFO_BAUND_ERROR;
	}
	
	return retval << (port_ix << 1);
}

//TODO: complete this function
//parameters config
//parameters frame format:	total FLASH_PAR_FRAME_LENGTH bytes(see FLASH_PAR_FRAME_LENGTH defination)
//	0: frame header(FLASH_PAR_HEAD, 4 bytes)
//	4: data send circle(1~SEND_DATA_MAX_CIRCLE, 4 bytes)
//	8: angle velocity serial port NO.(1 or 2, 1 byte)
//	9: data send serial port NO.(1 or 2, 1 byte)
//	10: serial send port1 parity bit(1 byte, 0=no parity(default), 1=odd, 2=even)
//	11: serial send port1 stop bit(1 byte, default 1 bit)
//	12: serial send port1 baund rate(4 bytes)
//	16: serial send port2 parity bit(1 byte, 0=no parity(default), 1=odd, 2=even)
//	17: serial send port2 stop bit(1 byte, default 1 bit)
//	18: serial send port2 baund rate(4 bytes)
//	22: device number(2 bytes)
//	24: system key(2 bytes)
//	26: sum parity(1 byte)
int par_conf(){
	unsigned int dbuf[40];
	unsigned int tmpui;
	int i, j, retval = 0;
	
	for(i = 0; i < FLASH_PAR_FRAME_LENGTH; ++i){
		dbuf[i] = (*(volatile unsigned int *)(FLASH_PAR_ADDR+i)) & 0xff;
	}
	//frame header check
	if(dbuf[0] != FLASH_PAR_HEAD1 ||
		dbuf[1] != FLASH_PAR_HEAD2 ||
		dbuf[2] != FLASH_PAR_HEAD3 ||
		dbuf[3] != FLASH_PAR_HEAD4){
		return ERRDATA_PARAMETER_TABLE_ERROR;
	}
	//parity check
	for(i = 0, tmpui = 0; i < FLASH_PAR_FRAME_LENGTH-1; ++i){
		tmpui += dbuf[i];
	}
	if((tmpui&0xff) != dbuf[FLASH_PAR_FRAME_LENGTH-1])
		return ERRDATA_PARAMETER_TABLE_ERROR;
	
	//set send circle
	tmpui = dbuf[4] + (dbuf[5]<<8) + (dbuf[6]<<16) + (dbuf[7]<<24);
	if(tmpui != 0){
		if(tmpui <= SEND_DATA_MAX_CIRCLE){
			g_send_circle = tmpui;
		}else g_send_circle = tmpui;
	}else{
		g_send_circle = 1;
		retval |= ERRDATA_PARAMETER_SEND_CIRCLE_ERROR;
	}
	//set Wxyz read port NO.
	if((dbuf[8]==1) || (dbuf[8]==2)){
		g_wxyz_read_port = dbuf[8] - 1;
	}else{
		g_wxyz_read_port = WXYZ_READ_PORT_INDEX;
		retval |= ERRDATA_PARAMETER_WXYZ_READ_PORT_ERROR;
	}
	//set frame send port NO.
	if((dbuf[9]==1) || (dbuf[9]==2)){
		g_frame_send_port = dbuf[9] - 1;
	}else{
		g_frame_send_port = DATA_FRAME_SEND_PORT_INDEX;
		retval |= ERRDATA_PARAMETER_DATA_FRAME_SEND_PORT_ERROR;
	}
	//set sfifo
	retval |= sfifo_init(&dbuf[10], 0);
	retval |= sfifo_init(&dbuf[16], 1);
	
	//set device number
	g_device_num[0] = dbuf[22];
	g_device_num[1] = dbuf[23];
	//set system key
	g_system_key[0] = dbuf[24];
	g_system_key[1] = dbuf[25];
	
	return retval;
}

int sfifo_read(int port_no, char *outbuf, int buflen){
	volatile unsigned int *ctl_reg;	//control register
	volatile unsigned int *dat_reg;	//data register
	unsigned int tmpui;
	int byte_counter = 0, overtime = 100;
	
	ctl_reg = sfifo_port_regs[port_no][2];
	dat_reg = sfifo_port_regs[port_no][0];
	while(byte_counter < buflen){
		tmpui = *ctl_reg;
		if((tmpui&1u) != 0){//data exist
			outbuf[byte_counter] = (*dat_reg) & 0xff;
			++byte_counter;
		}else{
			if(overtime-- <= 0){
				break;
			}
		}
	}
	
	return byte_counter;
}

int sfifo_write(const int port_ix, const int *dbuf, const int dlen){
#define SEND_DATA_DELAY 100
	volatile unsigned int *ctl_reg;	//control register
	volatile unsigned int *dat_reg;	//data register
	int over_time = 0, i = 0;
	
	ctl_reg = sfifo_port_regs[port_ix][2];
	dat_reg = sfifo_port_regs[port_ix][0];
	while(i < dlen){
		//TODO: need modify
		if(((*ctl_reg)&0x10) == 0){//fifo no full
			*dat_reg = dbuf[i++];
		}else{
			if(over_time++ >= SEND_DATA_OVER_TIME)
				break;
		}
		delay(SEND_DATA_DELAY);
	}
	
	return i;
}

void system_init_fail_proc(int msgdat){
	int retval, checksum;
	
	g_msg_code = MSGCODE_SYSTEM_INIT_FAIL;
	i2str(MSGCODE_SYSTEM_INIT_FAIL, &g_send_buf[13], 16, 2);
	//g_send_buf[2] = MSGCODE_SYSTEM_INIT_FAIL;	//message type
	g_msg_data = msgdat;
	i2str(msgdat, &g_send_buf[15], 16, 2);
	//g_send_buf[3] = msgdat;	//message data
	//set data to "0000"
	g_send_buf[5] = '0';
	g_send_buf[6] = '0';
	g_send_buf[7] = '0';
	g_send_buf[8] = '0';
	for(retval = 0, checksum = 0; retval < SEND_FRAME_LENGTH-7; ++retval){
		checksum += g_send_buf[retval];
	}
	i2str(checksum, &g_send_buf[SEND_FRAME_LENGTH-7], 16, 4);
}

void system_init(){
	int retval;
	
	// DSP init
	ST = 0x80;	//enable OVM(over flow mode)
	*vect_src = int00;	//set reset irq
	// *(vect_src + 3) = tint1;
	*(vect_src + 4) = tint1;
	*busctrl = 0x10F8;
	IE = 0x3;	//enable int3
	retval = par_conf();	//init parameters
	if(retval == 0){//init OK
		g_msg_code = 0;
		g_msg_data = 0;
	}else{//error exist
		if((retval&ERRDATA_PARAMETER_TABLE_ERROR) == 0){//parameters table OK, parameter data error
			g_msg_code = MSGCODE_SYSTEM_INIT_DATA_ERROR;
			g_msg_data = retval;
		}else{//parameters table error
			system_init_fail_proc(ERRDATA_PARAMETER_TABLE_ERROR);
		}
	}
	
	IF = 0x0;	//clear IF
}

void init_install_angel(){
#define INIT_INSTALL_ANGEL_OVERTIME 200
	int overtime_counter = 0;
	
	for(;;){
		if(angel_data_read() > 0){//init ok
			break;
		}else{
			delay(100);
		}
		if(g_timer_flag != 0){
			g_timer_flag = 0;
			if(++overtime_counter > INIT_INSTALL_ANGEL_OVERTIME){//overtime
				system_init_fail_proc(ERRDATA_INIT_INSTALL_ANGEL_FAIL);
				break;
			}
		}
	}
}

void main()
{
	ST = 0x0;	//disable GIE
	system_init();
	ST |= 0x2000;	//enable GIE
	/*
	if((g_msg_code & MSGCODE_SYSTEM_INIT_FAIL) == 0){//init parameter ok
		init_install_angel();
	}
	*/
	g_system_init_flag = 1u;
	
	for(;;);
}

void angel_parse(const char *bufp){
	int i = 0, tmpi;
	
	for(i = 0; i < 3; ++i){
		tmpi = bufp[i] + (((int)bufp[i+1]) << 8);
		tmpi = ((int)(tmpi << 16)) >> 16;
		g_device_install_angelw[i] = ((float)tmpi) * 0.06103515625;	//0.06103515625 = 2000/32768
		bufp += 2;
	}
}

int check_sum(const char *bufp, const int len){
	int i, sum;
	
	for(i=0, sum=0; i < len-1; ++i){
		sum += bufp[i];
	}
	if(sum == bufp[i])
		return 0;
	return 1;
}

int angel_data_read(){
#define SINGLE_ANGEL_FRAME_LENGTH 11
	static char cbuf[100];
	static cur_buf_ix = 0;
	int i, tmpi;
	int retval = 0;
	
	tmpi = sfifo_read(g_wxyz_read_port, &cbuf[cur_buf_ix], 100-cur_buf_ix);
	if(tmpi > 0){//new data in
		cur_buf_ix += tmpi;
	}
	i = 0;
	while((cur_buf_ix-i) >= SINGLE_ANGEL_FRAME_LENGTH){
		tmpi = 1;
		if(cbuf[i] == 0x55){
			switch(cbuf[i+1]){
				//frame header found
				case 0x51:
				case 0x52:
				case 0x53:
				if(check_sum(&cbuf[i], SINGLE_ANGEL_FRAME_LENGTH) == 0){//check OK
					if(cbuf[i+1] == 0x52){//Wxyz frame
						angel_parse(&cbuf[i+2]);	//parse angel data into g_device_install_angelw buffer
						++retval;
					}
					tmpi = SINGLE_ANGEL_FRAME_LENGTH;
				}
				break;
			}
		}
		i += tmpi;
	}
	if(i > 0){
		//move rest bytes to the front of buffer
		for(tmpi = 0; i < cur_buf_ix; ++tmpi, ++i){
			cbuf[tmpi] = cbuf[i];
		}
		cur_buf_ix = tmpi;
	}
	
	return retval;
}

void radar_data_read(){
	int tmpi, tmpj;
	
	tmpi = *(volatile unsigned int *)IQP_LOW;
	tmpi <<= 24;
	tmpi >>= 24;
	tmpj = *(volatile unsigned int *)IQN_LOW;
	tmpj <<= 24;
	tmpj >>= 24;
	//g_lowf_data += tmpi + tmpj;
	g_lowf_data += tmpj - tmpi;
	
	tmpi = *(volatile unsigned int *)IQP_HIGH;
	tmpi <<= 24;
	tmpi >>= 24;
	tmpj = *(volatile unsigned int *)IQN_HIGH;
	tmpj <<= 24;
	tmpj >>= 24;
	//g_highf_data += tmpi + tmpj;
	g_highf_data += tmpj - tmpi;
}

//read radar and angel values
void data_read(){
	radar_data_read();
	angel_data_read();
}

//TODO: complete this function
//process velocity values
void data_process(){
	float tmpf = 10.0 * cos(g_device_install_angelw[2]);
	
	g_flow_velocity[0] = g_lowf_data * tmpf;
	g_flow_velocity[1] = g_highf_data * tmpf;
	
}

void disassemble_int(int *outbuf, int ivar){
	outbuf[0] = ivar & 0xff;
	outbuf[1] = (ivar >> 8) & 0xff;
	outbuf[2] = (ivar >> 16) & 0xff;
	outbuf[3] = (ivar >> 24) & 0xff;
}

//assemble byte stream
void data_assemble(){
	int i, sum, tmpi;
	
	g_msg_code |= MSGCODE_DATA_OUTPUT;
	i2str(g_msg_code, &g_send_buf[13], 16, 2);
	//g_send_buf[3] = g_msg_data;
	i2str(g_msg_data, &g_send_buf[15], 16, 2);
	tmpi = g_flow_velocity[0] * 1000;	// mm/s
	//low frequence data
	i2str(tmpi, &g_send_buf[5], 10, 4);
	//disassemble_int(&g_send_buf[4], tmpi);
	tmpi = g_flow_velocity[1] * 1000;	// mm/s
	i2str(tmpi, &g_send_buf[9], 10, 4);
	//disassemble_int(&g_send_buf[8], tmpi);
	//clear data
	g_lowf_data = 0;
	g_highf_data = 0;
	
	//sum parity
	for(i = 0, sum = 0; i < SEND_FRAME_LENGTH-7; ++i){
		sum += g_send_buf[i];
	}
	i2str(tmpi, &g_send_buf[SEND_FRAME_LENGTH-7], 16, 4);
}

interrupt void c_int03(){
	if(g_system_init_flag != 0){//system init done
		if((g_msg_code&MSGCODE_SYSTEM_INIT_FAIL) == 0)//init ok
			data_read();
		if(++g_send_counter >= g_send_circle){//time to send
			if((g_msg_code&MSGCODE_SYSTEM_INIT_FAIL) == 0){//init ok
				data_process();
				data_assemble();
			}
			sfifo_write(g_frame_send_port, g_send_buf, SEND_FRAME_LENGTH);
			g_send_counter = 0;	//clear send circle counter
		}
	}
	g_timer_flag = 1;
}


void delay(int tim)
{       
    while(tim-- > 0);
}
