//Compatible with VESC FW3.49

#include "VescUart.h"
#include <HardwareSerial.h>

VescUart::VescUart(void){
	nunchuck.valueX         = 127;
	nunchuck.valueY         = 127;
	nunchuck.lowerButton  	= false;
	nunchuck.upperButton  	= false;
}

void VescUart::setSerialPort(HardwareSerial* port)
{
	serialPort = port;
}

void VescUart::setDebugPort(Stream* port)
{
	debugPort = port;
}

int VescUart::receiveUartMessage(uint8_t * payloadReceived) {

	// Messages <= 255 starts with "2", 2nd byte is length
	// Messages > 255 starts with "3" 2nd and 3rd byte is length combined with 1st >>8 and then &0xFF

	uint16_t counter = 0;
	uint16_t endMessage = 256;
	bool messageRead = false;
	uint8_t messageReceived[256];
	uint16_t lenPayload = 0;

	uint32_t timeout = millis() + 100; // Defining the timestamp for timeout (100ms before timeout)

	while ( millis() < timeout && messageRead == false) {

		while (serialPort->available()) {

			messageReceived[counter++] = serialPort->read();

			if (counter == 2) {

				switch (messageReceived[0])
				{
					case 2:
						endMessage = messageReceived[1] + 5; //Payload size + 2 for sice + 3 for SRC and End.
						lenPayload = messageReceived[1];
					break;

					case 3:
						// ToDo: Add Message Handling > 255 (starting with 3)
						if( debugPort != NULL ){
							debugPort->println("Message is larger than 256 bytes - not supported");
						}
					break;

					default:
						if( debugPort != NULL ){
							debugPort->println("Unvalid start bit");
						}
					break;
				}
			}

			if (counter >= sizeof(messageReceived)) {
				break;
			}

			if (counter == endMessage && messageReceived[endMessage - 1] == 3) {
				messageReceived[endMessage] = 0;
				if (debugPort != NULL) {
					debugPort->println("End of message reached!");
				}
				messageRead = true;
				break; // Exit if end of message is reached, even if there is still more data in the buffer.
			}
		}
	}
	if(messageRead == false && debugPort != NULL ) {
		debugPort->println("Timeout");
	}

	bool unpacked = false;

	if (messageRead) {
		unpacked = unpackPayload(messageReceived, endMessage, payloadReceived);
	}

	if (unpacked) {
		// Message was read
		return lenPayload;
	}
	else {
		// No Message Read
		return 0;
	}
}


bool VescUart::unpackPayload(uint8_t * message, int lenMes, uint8_t * payload) {

	uint16_t crcMessage = 0;
	uint16_t crcPayload = 0;

	// Rebuild crc:
	crcMessage = message[lenMes - 3] << 8;
	crcMessage &= 0xFF00;
	crcMessage += message[lenMes - 2];

	if(debugPort!=NULL){
		debugPort->print("SRC received: "); debugPort->println(crcMessage);
	}

	// Extract payload:
	memcpy(payload, &message[2], message[1]);

	crcPayload = crc16(payload, message[1]);

	if( debugPort != NULL ){
		debugPort->print("SRC calc: "); debugPort->println(crcPayload);
	}

	if (crcPayload == crcMessage) {
		if( debugPort != NULL ) {
			debugPort->print("Received: ");
			serialPrint(message, lenMes); debugPort->println();

			debugPort->print("Payload :      ");
			serialPrint(payload, message[1] - 1); debugPort->println();
		}

		return true;
	}
	else {
		return false;
	}
}


int VescUart::packSendPayload(uint8_t * payload, int lenPay) {

	uint16_t crcPayload = crc16(payload, lenPay);
	int count = 0;
	uint8_t messageSend[256];

	if (lenPay <= 256)
	{
		messageSend[count++] = 2;
		messageSend[count++] = lenPay;
	}
	else
	{
		messageSend[count++] = 3;
		messageSend[count++] = (uint8_t)(lenPay >> 8);
		messageSend[count++] = (uint8_t)(lenPay & 0xFF);
	}

	memcpy(&messageSend[count], payload, lenPay);

	count += lenPay;
	messageSend[count++] = (uint8_t)(crcPayload >> 8);
	messageSend[count++] = (uint8_t)(crcPayload & 0xFF);
	messageSend[count++] = 3;
	messageSend[count] = '\0';

	if(debugPort!=NULL){
		debugPort->print("UART package send: "); serialPrint(messageSend, count);
	}

	// Sending package
	serialPort->write(messageSend, count);

	// Returns number of send bytes
	return count;
}


bool VescUart::processReadPacket(bool deviceType, uint8_t * message) {

	COMM_PACKET_ID packetId;
	COMM_PACKET_ID_DIEBIEMS packetIdDieBieMS;

	int32_t ind = 0;

	if (!deviceType) { //device if VESC type
		packetId = (COMM_PACKET_ID)message[0];
		message++; // Removes the packetId from the actual message (payload)

		switch (packetId){
			case COMM_FW_VERSION: // Structure defined here: https://github.com/vedderb/bldc/blob/43c3bbaf91f5052a35b75c2ff17b5fe99fad94d1/commands.c#L164

				fw_version.major = message[ind++];
				fw_version.minor = message[ind++];
				return true;

			case COMM_GET_VALUES_SETUP_SELECTIVE: // Structure defined here: https://github.com/vedderb/bldc/blob/43c3bbaf91f5052a35b75c2ff17b5fe99fad94d1/commands.c#L164

				ind = 4; // Skip the mask
				data.tempMotor 		= buffer_get_float16(message, 10.0, &ind);
				data.avgMotorCurrent 	= buffer_get_float32(message, 100.0, &ind);
				data.avgInputCurrent 	= buffer_get_float32(message, 100.0, &ind);
				//ind += 8; // Skip the next 8 bytes
				//data.dutyCycleNow 		= buffer_get_float16(message, 1000.0, &ind);
				data.rpm 				= buffer_get_int32(message, &ind);
				data.inpVoltage = buffer_get_float16(message, 10.0, &ind);
				data.watt_hours = buffer_get_float32(message, 10000.0, &ind);
				data.watt_hours_charged = buffer_get_float32(message, 10000.0, &ind);
				//ind += 8; // Skip the next 8 bytes
				//data.tachometer 		= buffer_get_int32(message, &ind);
				//data.tachometerAbs 		= buffer_get_int32(message, &ind);
				data.fault = message[ind];
				return true;

			case COMM_GET_DECODED_PPM:

				data.throttlePPM 	= (float)(buffer_get_int32(message, &ind) / 10000.0);
				//data.rawValuePPM 	= buffer_get_float32(message, 100.0, &ind);
				return true;
			break;

			default:
				return false;
			break;
		}
	}
	else { //device is DieBieMS
		packetIdDieBieMS = (COMM_PACKET_ID_DIEBIEMS)message[0];
		message++; // Removes the packetId from the actual message (payload)

		switch (packetIdDieBieMS){

			case DBMS_COMM_GET_VALUES: // Structure defined here: https://github.com/DieBieEngineering/DieBieMS-Firmware/blob/master/Modules/Src/modCommands.c

				ind = 8; // Skip the first 2 float32 (pack voltage and current)
				// DieBieMSdata.packVoltage = buffer_get_float32(message, 1000.0, &ind);
				// DieBieMSdata.packCurrent = buffer_get_float32(message, 1000.0, &ind);
				DieBieMSdata.soc = message[ind];
				// DieBieMSdata.cellVoltageHigh = buffer_get_float32(message, 1000.0, &ind);
				// DieBieMSdata.cellVoltageAverage = buffer_get_float32(message, 1000.0, &ind);
				// DieBieMSdata.cellVoltageLow = buffer_get_float32(message, 1000.0, &ind);
				// DieBieMSdata.cellVoltageMisMatch = buffer_get_float32(message, 1000.0, &ind);
				// DieBieMSdata.loCurrentLoadVoltage = buffer_get_float16(message, 100.0, &ind);
				// DieBieMSdata.loCurrentLoadCurrent = buffer_get_float16(message, 100.0, &ind);
				// DieBieMSdata.hiCurrentLoadVoltage = buffer_get_float16(message, 100.0, &ind);
				// DieBieMSdata.hiCurrentLoadCurrent = buffer_get_float16(message, 100.0, &ind);
				// DieBieMSdata.auxVoltage = buffer_get_float16(message, 100.0, &ind);
				// DieBieMSdata.auxCurrent = buffer_get_float16(message, 100.0, &ind);
				// DieBieMSdata.tempBatteryHigh = buffer_get_float16(message, 10.0, &ind);
				// DieBieMSdata.tempBatteryAverage = buffer_get_float16(message, 10.0, &ind);
				// DieBieMSdata.tempBMSHigh = buffer_get_float16(message, 10.0, &ind);
				// DieBieMSdata.tempBMSAverage = buffer_get_float16(message, 10.0, &ind);
				// DieBieMSdata.operationalState = message[ind];
				// DieBieMSdata.chargeBalanceActive = message[ind++];
				// DieBieMSdata.faultState = message[ind++];

				return true;
			break;

			case DBMS_COMM_GET_BMS_CELLS: // Structure defined here: https://github.com/DieBieEngineering/DieBieMS-Firmware/blob/master/Modules/Src/modCommands.c

				DieBieMScells.noOfCells = message[ind++];

				for (uint8_t i=0; i<12;i++){
					DieBieMScells.cellsVoltage[i] = buffer_get_float16(message, 1000.0, &ind);
				}

				return true;
			break;

			default:
				return false;
			break;
		}
	}
}


bool VescUart::getVescValues(void) {
	uint8_t command[5];
	command[0] = { COMM_GET_VALUES_SETUP_SELECTIVE };
	//values selected : 0x000118AE
	command[1] = { 0x00 }; //mask MSB
	command[2] = { 0x01 }; //mask
	command[3] = { 0x18 }; //mask
	command[4] = { 0xAE }; //mask LSB
	uint8_t payload[256];

	packSendPayload(command, 5);
	// delay(1); //needed, otherwise data is not read

	int lenPayload = receiveUartMessage(payload);

	if (lenPayload > 0 && lenPayload < 55) {
		bool read = processReadPacket(false, payload); //returns true if sucessful
		return read;
	}
	else
	{
		return false;
	}
}

bool VescUart::getLocalVescPPM(void) {

	uint8_t command[1] = { COMM_GET_DECODED_PPM };

	uint8_t payload[256];

	packSendPayload(command, 1);
	// delay(1); //needed, otherwise data is not read

	int lenPayload = receiveUartMessage(payload);

	if (lenPayload > 0) { //&& lenPayload < 55
		bool read = processReadPacket(false, payload); //returns true if sucessful
		return read;
	}
	else
	{
		return false;
	}
}

bool VescUart::getMasterVescPPM(uint8_t id) {

	uint8_t command[3];
	command[0] = { COMM_FORWARD_CAN };
	command[1] = id;
	command[2] = { COMM_GET_DECODED_PPM };

	uint8_t payload[256];

	packSendPayload(command, 3);
	// delay(1); //needed, otherwise data is not read

	int lenPayload = receiveUartMessage(payload);

	if (lenPayload > 0) { //&& lenPayload < 55
		bool read = processReadPacket(false, payload); //returns true if sucessful
		return read;
	}
	else
	{
		return false;
	}
}

bool VescUart::getFWversion(void){

	uint8_t command[1] = { COMM_FW_VERSION };
	uint8_t payload[256];

	packSendPayload(command, 1);
	// delay(1); //needed, otherwise data is not read

	int lenPayload = receiveUartMessage(payload);

	if (lenPayload > 0) { //&& lenPayload < 55
		bool read = processReadPacket(false, payload); //returns true if sucessful
		return read;
	}
	else
	{
		return false;
	}
}

bool VescUart::getDieBieMSValues(uint8_t id) {
	uint8_t command[3];
	command[0] = { COMM_FORWARD_CAN }; //VESC command
	command[1] = id;
	command[2] = { DBMS_COMM_GET_VALUES }; //DieBieMS command

	uint8_t payload[256];

	packSendPayload(command, 3);
	// delay(1); //needed, otherwise data is not read

	int lenPayload = receiveUartMessage(payload);

	if (lenPayload > 0) { //&& lenPayload < 55
		bool read = processReadPacket(true, payload); //returns true if sucessful
		return read;
	}
	else
	{
		return false;
	}
}

bool VescUart::getDieBieMSCellsVoltage(uint8_t id) {
	uint8_t command[3];
	command[0] = { COMM_FORWARD_CAN }; //VESC command
	command[1] = id;
	command[2] = { DBMS_COMM_GET_BMS_CELLS }; //DieBieMS command

	uint8_t payload[256];

	packSendPayload(command, 3);
	// delay(1); //needed, otherwise data is not read

	int lenPayload = receiveUartMessage(payload);

	if (lenPayload > 0) { //&& lenPayload < 55
		bool read = processReadPacket(true, payload); //returns true if sucessful
		return read;
	}
	else
	{
		return false;
	}
}

void VescUart::setNunchuckValues() {
	int32_t ind = 0;
	uint8_t payload[11];

	payload[ind++] = COMM_SET_CHUCK_DATA;
	payload[ind++] = nunchuck.valueX;
	payload[ind++] = nunchuck.valueY;
	buffer_append_bool(payload, nunchuck.lowerButton, &ind);
	buffer_append_bool(payload, nunchuck.upperButton, &ind);

	// Acceleration Data. Not used, Int16 (2 byte)
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;

	if(debugPort != NULL){
		debugPort->println("Data reached at setNunchuckValues:");
		debugPort->print("valueX = "); debugPort->print(nunchuck.valueX); debugPort->print(" valueY = "); debugPort->println(nunchuck.valueY);
		debugPort->print("LowerButton = "); debugPort->print(nunchuck.lowerButton); debugPort->print(" UpperButton = "); debugPort->println(nunchuck.upperButton);
	}

	packSendPayload(payload, 11);
}

void VescUart::setCurrent(float current) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_CURRENT;
	buffer_append_int32(payload, (int32_t)(current * 1000), &index);

	packSendPayload(payload, 5);
}

void VescUart::setBrakeCurrent(float brakeCurrent) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_CURRENT_BRAKE;
	buffer_append_int32(payload, (int32_t)(brakeCurrent * 1000), &index);

	packSendPayload(payload, 5);
}

void VescUart::setRPM(float rpm) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_RPM ;
	buffer_append_int32(payload, (int32_t)(rpm), &index);

	packSendPayload(payload, 5);
}

void VescUart::setDuty(float duty) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_DUTY;
	buffer_append_int32(payload, (int32_t)(duty * 100000), &index);

	packSendPayload(payload, 5);
}

void VescUart::serialPrint(uint8_t * data, int len) {
	if(debugPort != NULL){
		for (int i = 0; i <= len; i++)
		{
			debugPort->print(data[i]);
			debugPort->print(" ");
		}

		debugPort->println("");
	}
}

void VescUart::printVescValues() {
	if(debugPort != NULL){
		debugPort->print("avgMotorCurrent: "); 	debugPort->println(data.avgMotorCurrent);
		debugPort->print("avgInputCurrent: "); 	debugPort->println(data.avgInputCurrent);
		debugPort->print("dutyCycleNow: "); 	debugPort->println(data.dutyCycleNow);
		debugPort->print("rpm: "); 				debugPort->println(data.rpm);
		debugPort->print("inputVoltage: "); 	debugPort->println(data.inpVoltage);
		debugPort->print("ampHours: "); 		debugPort->println(data.ampHours);
		debugPort->print("ampHoursCharges: "); 	debugPort->println(data.ampHoursCharged);
		debugPort->print("tachometer: "); 		debugPort->println(data.tachometer);
		debugPort->print("tachometerAbs: "); 	debugPort->println(data.tachometerAbs);
	}
}
