//Fixed 202211111446(1550)



//#define SerialDevice SerialUSB //32u4,samd21
#define SerialDevice Serial //esp8266

// bitWrite 不支持 uint64_t，以下定义来自 https://forum.arduino.cc/t/bitset-only-sets-bits-from-0-to-31-previously-to-15/193385/5
#define bitSet64(value, bit) ((value) |= (bit<32?1UL:1ULL) <<(bit))
#define bitClear64(value, bit) ((value) &= ~(bit<32?1UL:1ULL) <<(bit))
#define bitWrite64(value, bit, bitvalue) (bitvalue ? bitSet64(value, bit) : bitClear64(value, bit))
#define Threshold 40 //触摸触发阈值

#include "Adafruit_MPR121.h"//mpr121定义
Adafruit_MPR121 mprA, mprB, mprC;
#define CLAMP(val) (val < 0 ? 0 : (val > 255 ? 255 : val))
uint8_t packet[6];
uint8_t len = 0;//当前接收的包长度
#define sA1(x) bitWrite(TPD, 0, x)//设置 sensor
#define sB1(x) bitWrite(TPD, 8, x)
#define sC1(x) bitWrite(TPD, 16, x)
#define sD1(x) bitWrite(TPD, 18, x)
#define sE1(x) bitWrite(TPD, 26, x)
enum {
  commandRSET  = 0x45,//E
  commandHALT  = 0x4C,//L
  commandSTAT  = 0x41,//A
  commandRatio = 0x72,//r
  commandSens  = 0x6B,//k
};

bool Conditioning = 1;
void setup() {
  SerialDevice.begin(9600);
  SerialDevice.setTimeout(0);
  uint8_t TOUCH = 1;//按下敏感度
  uint8_t RELEASE = 1;//松开敏感度
  mprA.begin(0x5A, &Wire);
  mprB.begin(0x5C, &Wire);
  mprC.begin(0x5B, &Wire);
  Wire.setClock(800000);

}

void loop() {
  Recv();
  Conditioning ? void() : TouchSend();//只有不处于设定模式时才发送触摸数据
}
void MprSetup(Adafruit_MPR121 cap) {//mpr121自定义初始化 25
  cap.writeRegister(MPR121_SOFTRESET, 0x63);//MprReset
  delay(1);
  cap.writeRegister(MPR121_ECR, 0x0);//MprStop
  cap.writeRegister(MPR121_MHDR, 1);// 上升最大变化值
  cap.writeRegister(MPR121_NHDR, 16);//上升幅度
  cap.writeRegister(MPR121_NCLR, 4);//上升修正样本个数
  cap.writeRegister(MPR121_FDLR, 0);//修正前等待样本个数
  cap.writeRegister(MPR121_MHDF, 4);//下降最大变化值
  cap.writeRegister(MPR121_NHDF, 1);//下降幅度
  cap.writeRegister(MPR121_NCLF, 16);//下降修正样本个数
  cap.writeRegister(MPR121_FDLF, 4);//修正前等待样本个数
  cap.writeRegister(MPR121_NHDT, 0);
  cap.writeRegister(MPR121_NCLT, 0);
  cap.writeRegister(MPR121_FDLT, 0);
  cap.setThresholds(Threshold, Threshold); //设置触发阈值和充放电流时间
  cap.writeRegister(MPR121_DEBOUNCE, (4 << 4) | 2); //去抖动 释放等待4，按下等待2
  cap.writeRegister(MPR121_CONFIG1, 16);//0x10
  cap.writeRegister(MPR121_CONFIG2, 1 << 5);
  cap.writeRegister(MPR121_AUTOCONFIG0, 0x01);
  cap.writeRegister(MPR121_AUTOCONFIG1, (1 << 7));
  cap.writeRegister(MPR121_UPLIMIT, 202);//上限，((Vdd - 0.7)/Vdd) * 256
  cap.writeRegister(MPR121_TARGETLIMIT, 182);//目标，UPLIMIT * 0.9
  cap.writeRegister(MPR121_LOWLIMIT, 131);//下限，UPLIMIT * 0.65
  cap.writeRegister(MPR121_ECR, B10000000 + 12);//MprRun
}
void cmd_RSET() {//Reset
  MprSetup(mprA);
  MprSetup(mprB);
  MprSetup(mprC);



}
void cmd_HALT() {//Start Conditioning Mode
  mprA.writeRegister(MPR121_ECR, 0x0);//MprStop
  mprB.writeRegister(MPR121_ECR, 0x0);
  mprC.writeRegister(MPR121_ECR, 0x0);
  Conditioning = true;
}

void cmd_Ratio() {//Set Touch Panel Ratio
  SerialDevice.write((byte)'(');
  SerialDevice.write((byte)packet[1]);//L,R
  SerialDevice.write((byte)packet[2]);//sensor
  SerialDevice.write((byte)'r');
  SerialDevice.write((byte)packet[4]);//Ratio
  SerialDevice.write((byte)')');
}

void cmd_Sens() {//Set Touch Panel Sensitivity
  SerialDevice.write((byte)'(');
  SerialDevice.write((byte)packet[1]);//L,R
  SerialDevice.write((byte)packet[2]);//sensor
  SerialDevice.write((byte)'k');
  SerialDevice.write((byte)packet[4]);//Sensitivity
  SerialDevice.write((byte)')');
}

void cmd_STAT() { //End Conditioning Mode
  Conditioning = false;
  mprA.writeRegister(MPR121_ECR, B10000000 + 12);//MprRun
  mprB.writeRegister(MPR121_ECR, B10000000 + 12);
  mprC.writeRegister(MPR121_ECR, B10000000 + 12);
}

void Recv() {
  while (SerialDevice.available()) {
    uint8_t r = SerialDevice.read();
    if (r == '{') {
      len = 0;
    }
    if (r == '}') {
      break;
    }
    packet[len++] = r;
  }
  if (len == 5) {
    switch (packet[3]) {
      case commandRSET:
        cmd_RSET();
        break;
      case commandHALT:
        cmd_HALT();
        break;
      case commandRatio:
        cmd_Ratio();
        break;
      case commandSens:
        cmd_Sens();
        break;
      case commandSTAT:
        cmd_STAT();
        break;
    }
    len = 0;
    memset(packet, 0, 6);
  }
}

void TouchSend() {

  uint64_t TouchData = 0; //触摸数据包

  for (int i = 9; i >= 0; i--) { // E8 ~ D7
    TouchData = (TouchData | (int)(mprC.baselineData(i) - mprC.filteredData(i) + 20) > Threshold) << 1;
  }
  for (int i = 11; i >= 0; i--) { // D6 ~ B5
    TouchData = (TouchData | (int)(mprB.baselineData(i) - mprB.filteredData(i) + 20) > Threshold) << 1;
  }
  for (int i = 11; i >= 0; i--) { // B4 ~ A1
    TouchData = (TouchData | (int)(mprA.baselineData(i) - mprA.filteredData(i) + 20) > Threshold) << 1;
  }
  TouchData >>= 1;

  SerialDevice.write((byte)'(');
  for (uint8_t r = 0; r < 7; r++) {
    SerialDevice.write((byte)TouchData & B11111);
    TouchData >>= 5;
  }
  SerialDevice.write((byte)')');
}
