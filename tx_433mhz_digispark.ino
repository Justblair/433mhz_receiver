/*
The MIT License (MIT)

Copyright (c) 2015 François GUILLIER <dev @ guillier . org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// Decoder for La Crosse Weather Sensors TX2/TX3/TX4 with or without hygrometer
// Partially based on work by Jean-Paul ROUBELAT - F6FBB http://www.f6fbb.org/domo/sensors/tx3_th.php
// Partially based on work by Pilight http://wiki.pilight.org/doku.php/alecto_ws1700_v7_0
// Partially based on work by SUI77 https://github.com/sui77/rc-switch
// Partially based on work by Pilight https://wiki.pilight.org/doku.php/elro_he
// X10 Protocol found in this document 

// Data is sent to Serial via the Debugging Library (on pin 2)
// Version for Digispark http://digistump.com/category/1

// Should be ~ 1300uS (long - high for Lacrosse)
#define LP_MIN 1100
#define LP_MAX 1500
// Should be ~ 500uS (short - high for Lacrosse)
#define SP_MIN 300
#define SP_MAX 700
// Should be ~ 1900uS (short - low for Alecto)
#define ASP_MIN 1600
#define ASP_MAX 2200
// Should be ~ 3800uS (long - low for Alecto)
#define ALP_MIN 3200
#define ALP_MAX 4400
// Should be ~ 9200uS (end - low for Alecto & RC-Switch Protocol 1)
#define AEP_MIN 8600
#define AEP_MAX 9800
// Should be ~ 320uS (short - high for RC-Switch Protocol 1)
#define RSP_MIN 270
#define RSP_MAX 470
// Should be ~ 960uS (long - high for RC-Switch Protocol 1)
#define RLP_MIN 910
#define RLP_MAX 1110
// Should be ~ 9920uS (end - low for Alecto & RC-Switch Protocol 1)
#define REP_MIN 9720
#define REP_MAX 10520
// Should be ~ 560us (short - HIGH for X10RF
#define XSP_MIN 360
#define XSP_MAX 760
// Should be ~ 1680 (high for X10RF)
#define XLP_MIN 1500
#define XLP_MAX 1900
// Should be ~ 9000us if not 4500us (end - HIGH for X10RF)
#define XEP_MIN 8800
#define XEP_MAX 9200
#define PIN 0 // From 433Mhz receiver


unsigned long dur; // pulse duration
byte nibble[11]; // Lacrosse
byte bits[34]; // Alecto
byte type; // data = temperature or humidity (Lacrosse)

void setup()
{
	pinMode(PIN, INPUT);
	Serial.begin(38400);
}

void alecto_ws1700()
{
	int p;
	for (p = 0; p < 100; p++)
	{
		dur = pulseIn(PIN, LOW);
		if ((dur > AEP_MIN) && (dur < AEP_MAX))
			break;
	}
	if (p == 100)
		return;

	// check the first four bits for 0101 header 
	byte v = 0;
	for (int i = 0; i < 4; i++)
	{
		v <<= 2;
		dur = pulseIn(PIN, LOW);
		if ((dur > ASP_MIN) && (dur < ASP_MAX))
		{
			v |= 2;
		}
		else if ((dur > ALP_MIN) && (dur < ALP_MAX))
		{
			v |= 3;
		}
	}

	if (v != 187) // 10111011b = 187d this is the equivelant of 0101
		return;

	// check the next 32 
	for (int i = 0; i < 32; i++)
	{
		dur = pulseIn(PIN, LOW);
		if ((dur > ASP_MIN) && (dur < ASP_MAX))
		{
			bits[i] = 0;
		}
		else if ((dur > ALP_MIN) && (dur < ALP_MAX))
		{
			bits[i] = 1;
		}
	}

	// Check that the end pulse comes through properly, if not cancel out
	dur = pulseIn(PIN, LOW);
	if ((dur < AEP_MIN) || (dur > AEP_MAX))
		return;

	int h = 0;
	int t = 0;
	for (int i = 0; i < 32; i++)
	{
		if (i > 11)
		{
			if (i > 23)
			{
				h <<= 1;
				h |= bits[i];
			}
			else
			{
				t <<= 1;
				t |= bits[i];
			}
		}
	}

	if (h == 0)
		return;

	if (t > 3840)
		t -= 4096;

	int channel = (bits[10] << 1) + bits[11];
	if (channel == 3)  // 00=CH1, 01=CH2, 10=CH3, 11=Error
		return;
	channel += 1001;

	Serial.print("TEMP,");
	Serial.print(channel);
	Serial.print(",");
	Serial.print(t / 10);
	Serial.print(".");
	Serial.println(abs(t % 10));
	Serial.print("HYGR,");
	Serial.print(channel);
	Serial.print(",");
	Serial.println(h);
	Serial.print("BATT,");
	Serial.print(channel);
	Serial.print(",");
	Serial.println(bits[8] ? "OK" : "KO");
	delay(500); // Signal is repeated by sensor. If values are OK then additional transmissions can be ignored.
}

void rc_switch()
{
	int p;
	for (p = 0; p < 100; p++)
	{
		// Synchronise with the very long pulse
		dur = pulseIn(PIN, LOW);
		if ((dur > REP_MIN) && (dur < REP_MAX))
			break;
	}
	if (p == 100)
		return;

	// Read the dual-bits
	for (int i = 0; i < 24; i++)
	{
		dur = pulseIn(PIN, LOW);
		if ((dur > RSP_MIN) && (dur < RSP_MAX))
		{
			bits[i] = 0;
		}
		else if ((dur > RLP_MIN) && (dur < RLP_MAX))
		{
			bits[i] = 1;
		}
	}

	String addr = "";
	String unit = "";
	byte state = 0;
	for (int i = 0; i < 24; i += 2)
	{
		if (bits[i] != 1)
			return;
		if (i < 10)
		{
			if (bits[i + 1])
				addr += '1';
			else
				addr += '0';
		}
		else if (i < 20)
		{
			if (bits[i + 1])
				unit += '1';
			else
				unit += '0';
		}
		else if (i == 20)
			state = bits[i + 1];
		else if (state == bits[i + 1]) // Last bit should be "NOT state"
			return;
	}
	Serial.print("RC,");
	Serial.print(addr);
	Serial.print(',');
	Serial.print(unit);
	Serial.print(',');
	Serial.println(state ? '1' : '0');
	delay(500); // Signal is repeated by sensor. If values are OK then additional transmissions can be ignored.
}

void x10_rf() {
	const uint8_t HOUSE_CODE[16] =
	{
		B0110,B1110,B0010,B1010,B0001,B1001,B0101,B1101,
		B0111,B1111,B0011,B1011,B0000,B1000,B0100,B1100,
	};


	int p;
	byte address;
	byte _address;
	byte command;
	byte _command;
	int addressNo = 0;

	for (p = 0; p < 100; p++)
	{
		// Synchronise with the very long pulse
		dur = pulseIn(PIN, HIGH);
		if ((dur > XEP_MIN) && (dur < XEP_MAX))
			break;
	}

	if (p == 100)
		return;

	address = Xbyte();
	_address = Xbyte();
	command = Xbyte();
	_command = Xbyte();

	// Check that the end pulse comes through properly, if not cancel out
	dur = pulseIn(PIN, HIGH);
	if ((dur < XEP_MIN) || (dur > XEP_MAX))
		return;

	// Error check the address and commands 

	if (address != ~_address)
		return;
	if (command != ~_command)
		return;

	// Lets print out the first 
	for (int i = 0; i < 16; i++) {
		if (HOUSE_CODE[i] == address & B1111)
			Serial.print(char(65 + i));
	}

	if (address& B00001111 == B00000100)
		addressNo = 8;



	switch(command & B11011111) {  // 1st in series
	case B00000000:
		addressNo += 1;
		break;
	case B00010000:
		addressNo += 2;
		break;
	case B00001000:
		addressNo += 3;
		break;
	case B00011000:
		addressNo += 4;
		break;
	case B01000000:
		addressNo += 5;
		break;
	case B01010000:
		addressNo += 6;
		break;
	case B01001000:
		addressNo += 7;
		break;
	case B01011000:
		addressNo += 8;
		break;
	}

	Serial.print(addressNo, DEC);
	// Lets find out if its on or off
	if (command & B00100000 == B00100000) {
		Serial.print("OFF");
	}
	else if (command & B10001000 == B10001000) {
		Serial.print(" DIM ");
		if (command & B00010000 == B00010000) {
			Serial.print("+5");
		}
		else Serial.print("-5"); // its a dim command
	}
	else Serial.print("ON");
}

byte Xbyte() {
	byte fetch = 0;
	for (int i = 0; i < 8; i++)
	{
		fetch << 1;
		dur = pulseIn(PIN, HIGH);
		if ((dur > XSP_MIN) && (dur < XSP_MAX))
		{
			bits[i] = 0;
		}
		else if ((dur > XLP_MIN) && (dur < XLP_MAX))
		{
			bits[i] = 1;
		}
	}
	return fetch;
}

int read_tx()
{
	for (int i = 0; i < 11; i++)
		nibble[i] = 0;
	nibble[1] = 10;

	// In theory preamble is 00001010 but here we don't care if first few
	// bits are garbled as long as it starts with 1010 with the correct
	// pulse timing.
	byte alecto_dur = 0;
	byte v2 = 0;
	byte x10_dur = 0;

	do
	{
		alecto_dur <<= 1;
		v2 <<= 1;
		x10_dur <<= 1;

		dur = pulseIn(PIN, HIGH);

		if ((dur > XSP_MIN) && (dur < XSP_MAX))
			x10_dur |= 1;
		if ((dur > SP_MIN) && (dur < SP_MAX))
			alecto_dur |= 1;  // Short Pulse => 1
		else if ((dur <= LP_MIN) || (dur >= LP_MAX))
			alecto_dur = 0;  // Not Long Pulse => Reset
		if (((dur > RSP_MIN) && (dur < RSP_MAX)) || ((dur > RLP_MIN) && (dur < RLP_MAX)))
			v2 |= 1;  // RC-Switch Pulse => 1
		else
			v2 = 0;  // Reset
	} while ((alecto_dur != 10) && (alecto_dur != 255) && (v2 != 255));  // 00001010b = 10d

	// At least 8 short pulses => Alecto detected
	if (alecto_dur == 255)
	{
		alecto_ws1700();
		return 1;
	}

	// At least 8 RC pulses => RC-Switch detected
	if (v2 == 255)
	{
		rc_switch();
		return 1;
	}

	if (x10_dur == 255) {
		x10_rf();
		return 1;
	}

	// Otherwise it is a Lacrosse TX...
	// so we read all the remaining bits
	for (int i = 8; i < 44; i++)
	{
		nibble[i >> 2] <<= 1;
		dur = pulseIn(PIN, HIGH);
		if ((dur > SP_MIN) && (dur < SP_MAX))
			nibble[i >> 2] |= 1;  // Short Pulse => 1  
		else if ((dur <= LP_MIN) || (dur >= LP_MAX))
			return -1;  // Not Long Pulse => Reset
	}

	// Should be either 0 (Temperature) or 14 (Hygrometry)
	if ((nibble[2] == 0) || (nibble[2] == 14))
		type = nibble[2];
	else
		return -1;

	// Next 7 bits are the device number generated randomly when batteries are
	// changed on the sensor.
	// Lowest bit of the nibble #4 is supposed to be the parity but this
	// doesn't seem to match on the older sensors (TX2)
	int device = (nibble[3] << 3) | (nibble[4] >> 1);

	// Temperature is composed of nibbles #5 & #6 + #7 for the decimal part
	// There is also a offset of 50 deg Celcius.
	int val = nibble[5] * 10 + nibble[6];
	int vald = 0;
	if (type == 0)
	{
		val -= 50;
		vald = nibble[7];
	}

	// Last nibble is CRC and should match 4 lowest bits of the arithmetic
	// addition of all the previous nibbles
	int crc1 = 0;
	for (int i = 0; i < 10; i++)
		crc1 += nibble[i];

	crc1 &= 15;

	if (crc1 != nibble[10])
		return -2;  // CRC KO

	if ((nibble[5] != nibble[8]) || (nibble[6] != nibble[9]))
		return -3;  // Digit check KO

	  // Format : e.g. for device #39
	  // TEMP,39,20.5
	  // HYGR,39,48
	if (type == 0)
	{
		Serial.print("TEMP,");
		Serial.print(device);
		Serial.print(",");
		Serial.print(val);
		Serial.print(".");
		Serial.println(vald);
	}
	else
	{
		Serial.print("HYGR,");
		Serial.print(device);
		Serial.print(",");
		Serial.println(val);
	}

	delay(50); // Signal is repeated by sensor. If CRC is OK then additional transmissions can be ignored.
}

void loop() { read_tx(); }
