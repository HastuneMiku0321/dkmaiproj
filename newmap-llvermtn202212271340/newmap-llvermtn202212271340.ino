//llvermtn202212271340.ino with new!! map
#define SerialDevice Serial 
#define Threshold 60 
#include "Adafruit_MPR121.h"//mpr121定义
Adafruit_MPR121 mpr[5];

class touchblock {
  public:
    uint8_t mprid;
    uint8_t portid;
    uint16_t thres;
};


/*
  {mprid,portid,threshold}

  mprid: the id of mpr,1 = 0x5a,2=0x5b,3=0x5c,4=0x5d
  portid: mpr pin port,0-11
  threshold:threshold)

  remember all  cap.writeRegister(MPR121_ECR, B10000000 + XXX);(total 5) ,the XXX must > max{portid}+1.

*/
touchblock touchmap[34] = {
  // Group A
  {1, 0, 40}, //1
  {1, 6, 40}, //2
  {2, 0, 40}, //3
  {2, 7, 40}, //4
  {4, 0, 40}, //5
  {4, 7, 40}, //6
  {3, 0, 40}, //7
  {3, 6, 40}, //8
  //Group B
  {1, 1, 40}, //1
  {1, 4, 40}, //2
  {2, 1, 40}, //3
  {2, 5, 40}, //4
  {4, 1, 40}, //5
  {4, 5, 40}, //6
  {3, 1, 40}, //7
  {3, 4, 40}, //8
  //Group C
  {2, 4, 40}, //1
  {4, 2, 40}, //2
  //Group D
  {3, 7, 40}, //1
  {1, 2, 40}, //2
  {1, 7, 40}, //3
  {2, 2, 40}, //4
  {2, 8, 40}, //5
  {4, 3, 40}, //6
  {4, 8, 40}, //7
  {3, 3, 40}, //8
  //Group E
  {3, 5, 40}, //1
  {1, 3, 40}, //2
  {1, 5, 40}, //3
  {2, 3, 40}, //4
  {2, 6, 40}, //5
  {4, 4, 40}, //6
  {4, 6, 40}, //7
  {3, 2, 40}, //8
};




#define CLAMP(val) (val < 0 ? 0 : (val > 255 ? 255 : val))
uint8_t packet[6];
uint8_t len = 0;//当前接收的包长度

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
  mpr[1].begin(0x5A, &Wire);
  mpr[2].begin(0x5B, &Wire);
  mpr[3].begin(0x5C, &Wire);
  mpr[4].begin(0x5D, &Wire);
  Wire.setClock(800000);

}

void loop() {
  Recv();
  Conditioning ? void() : TouchSend();
}

void MprSetup(Adafruit_MPR121 cap) {
  cap.writeRegister(MPR121_SOFTRESET, 0x63);
  delay(1);
  cap.writeRegister(MPR121_ECR, 0x0);
  cap.writeRegister(MPR121_MHDR, 1);
  cap.writeRegister(MPR121_NHDR, 16);
  cap.writeRegister(MPR121_NCLR, 4);
  cap.writeRegister(MPR121_FDLR, 0);
  cap.writeRegister(MPR121_MHDF, 4);
  cap.writeRegister(MPR121_NHDF, 1);
  cap.writeRegister(MPR121_NCLF, 16);
  cap.writeRegister(MPR121_FDLF, 4);
  cap.writeRegister(MPR121_NHDT, 0);
  cap.writeRegister(MPR121_NCLT, 0);
  cap.writeRegister(MPR121_FDLT, 0);
  cap.setThresholds(Threshold, Threshold);
  cap.writeRegister(MPR121_DEBOUNCE, (4 << 4) | 2);
  cap.writeRegister(MPR121_CONFIG1, 16);
  cap.writeRegister(MPR121_CONFIG2, 1 << 5);
  cap.writeRegister(MPR121_AUTOCONFIG0, 0x01);
  cap.writeRegister(MPR121_AUTOCONFIG1, (1 << 7));
  cap.writeRegister(MPR121_UPLIMIT, 202);
  cap.writeRegister(MPR121_TARGETLIMIT, 182);
  cap.writeRegister(MPR121_LOWLIMIT, 131);
  cap.writeRegister(MPR121_ECR, B10000000 + 9 );
}
void cmd_RSET() {//Reset
  MprSetup(mpr[1]);
  MprSetup(mpr[2]);
  MprSetup(mpr[3]);
  MprSetup(mpr[4]);


}
void cmd_HALT() {//Start Conditioning Mode
  mpr[1].writeRegister(MPR121_ECR, 0x0);//MprStop
  mpr[2].writeRegister(MPR121_ECR, 0x0);
  mpr[3].writeRegister(MPR121_ECR, 0x0);
  mpr[4].writeRegister(MPR121_ECR, 0x0);
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
  mpr[1].writeRegister(MPR121_ECR, B10000000 + 8);//MprRun
  mpr[2].writeRegister(MPR121_ECR, B10000000 + 9);
  mpr[3].writeRegister(MPR121_ECR, B10000000 + 8);
  mpr[4].writeRegister(MPR121_ECR, B10000000 + 9);
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

  uint64_t TouchData = 0;
  for (int i = 33; i >= 0; i--) {
    TouchData <<= 1;
    TouchData = (TouchData |
                 (int)(mpr[touchmap[i].mprid].baselineData(touchmap[i].portid) -
                       mpr[touchmap[i].mprid].filteredData(touchmap[i].portid) +
                       20)
                 > touchmap[i].thres) ;
  }

  SerialDevice.write((byte)'(');
  for (uint8_t r = 0; r < 7; r++) {
    SerialDevice.write((byte)TouchData & B11111);
    TouchData >>= 5;
  }
  SerialDevice.write((byte)')');
}
