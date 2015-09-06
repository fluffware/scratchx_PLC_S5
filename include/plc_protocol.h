

/* Binary protocol

Short parameters
0	Command/reply byte
1-n	Parameters
n+1	CRC8

Long parameters
0	Command/reply byte
1	Number of parameter bytes
2-n	Parameters
n+1	CRC8



Command/reply byte
Bits
7-6	Number of parameter bytes (3 means next byte is number of parameters)
5	1 if reply
4-0	Command code

Codes				Parameters
0x41	Read digital input	Address (1 byte)
0xa1	Digital input reply	Address (1 byte), Result (1 byte)
0xc2	Write digital output	3,Address (1 byte), Value (1 byte), Mask(1 byte)
0xa2	Digital output reply	Address (1 byte), Result (1 byte)
0x43	Read analog input	Address (1 byte)
0xd3	Analog input reply	3,Address (1 byte), Result (2 byte)
0xc4	Write analog output	3,Address (1 byte), Value (2 byte)
0xd4	Analog output reply	3,Address (1 byte), Result (2 byte)
0x6f	Error reply
*/


#define PLC_MSG_REPLY 0x20
#define PLC_MSG_LEN0 0x00
#define PLC_MSG_LEN1 0x40
#define PLC_MSG_LEN2 0x80
#define PLC_MSG_LENBYTE 0xc0 /* Length of parameters in next byte */

#define PLC_CMD_LEN0(cmd) ((cmd) | PLC_MSG_LEN0) 
#define PLC_CMD_LEN1(cmd) ((cmd) | PLC_MSG_LEN1) 
#define PLC_CMD_LEN2(cmd) ((cmd) | PLC_MSG_LEN2) 
#define PLC_CMD_LEN3(cmd) ((cmd) | PLC_MSG_LEN3) 
#define PLC_CMD_LENBYTE(cmd) ((cmd) | PLC_MSG_LENBYTE)

#define PLC_REPLY_LEN0(reply) ((reply) | PLC_MSG_REPLY | PLC_MSG_LEN0) 
#define PLC_REPLY_LEN1(reply) ((reply) | PLC_MSG_REPLY | PLC_MSG_LEN1) 
#define PLC_REPLY_LEN2(reply) ((reply) | PLC_MSG_REPLY | PLC_MSG_LEN2) 
#define PLC_REPLY_LEN3(reply) ((reply) | PLC_MSG_REPLY | PLC_MSG_LEN3) 
#define PLC_REPLY_LENBYTE(reply) ((reply) | PLC_MSG_REPLY | PLC_MSG_LENBYTE)

#define PLC_CMD_READ_DIGITAL_INPUT PLC_CMD_LEN1(1)
#define PLC_REPLY_DIGITAL_INPUT PLC_REPLY_LEN2(1)
#define PLC_CMD_WRITE_DIGITAL_OUTPUT PLC_CMD_LENBYTE(2) /* Next byte is 3 */
#define PLC_REPLY_DIGITAL_OUTPUT PLC_REPLY_LEN2(2)
  
#define PLC_CMD_READ_ANALOG_INPUT PLC_CMD_LEN1(3)
#define PLC_REPLY_ANALOG_INPUT PLC_REPLY_LENBYTE(3) /* Next byte is 3 */
#define PLC_CMD_WRITE_ANALOG_OUTPUT PLC_CMD_LENBYTE(4) /* Next byte is 3 */
#define PLC_REPLY_ANALOG_OUTPUT PLC_REPLY_LENBYTE(4) /* Next byte is 3 */
#define PLC_REPLY_ERROR PLC_REPLY_LEN1(0xf)

#define PLC_REPLY_ERROR_UNKNOWN_COMMAND 0x01
#define PLC_REPLY_ERROR_CRC 0x02
