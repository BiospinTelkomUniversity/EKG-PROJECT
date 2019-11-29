#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>


#define FIREBASE_HOST "pengmas-ekg.firebaseio.com"
#define FIREBASE_AUTH "4mkZwqFND27ZoFf7DUlvCAppYXm1eOR1OCq7pwth"

#define NAMA_AP "lab104portable"
#define PASSWD "enaksekali"


//MQTT Setup
const char* mqttServer = "m11.cloudmqtt.com";
const int mqttPort = 12948;
const char* mqttUser = "YourMqttUser";
const char* mqttPassword = "YourMqttUserPassword";

//WiFiClient espClient;
//PubSubClient client(espClient);

int sensorValue = 0;
int filteredSignal = 0;

//Reserve for adjusting flow program
bool ElectrodePlug = false;
bool lastPlug = false;

int EMA_S_low = 0;          //initialization of EMA S
int EMA_S_high = 0;

//setup oled
#define OLED_Address 0x3C

//custom clock speed, increase speed refresh rate
Adafruit_SSD1306 oled(128, 64, &Wire, -1, 800000, 800000);
int x = 0;
int yData = 0;

//untuk plot kontinu
int lastX = 0;
int lastY = 0;
int lastTime = 0;

//delay setting
int periode = 10; //delay per 10 milidetik sampling rate=100Hz
unsigned long time_now = 0;
unsigned long time_now2 = 0;

//BUTTERWORTH FILTER CONSTANT
// Fc1 =0.5Hz Fc2=40Hz
// jumlah ordo = 5


#define NZEROS_BPF 8
#define NPOLES_BPF 8
#define GAIN   3.807081066e+00

static float xv[NZEROS_BPF + 1], yv[NPOLES_BPF + 1];

int BandPassFilter(int analogValue) {
  xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4]; xv[4] = xv[5]; xv[5] = xv[6]; xv[6] = xv[7]; xv[7] = xv[8];
  xv[8] = analogValue / GAIN;
  yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4]; yv[4] = yv[5]; yv[5] = yv[6]; yv[6] = yv[7]; yv[7] = yv[8];
  yv[8] =   (xv[0] + xv[8]) - 4 * (xv[2] + xv[6]) + 6 * xv[4]
            + ( -0.0694168803 * yv[0]) + ( -0.1573294523 * yv[1])
            + (  0.1988344065 * yv[2]) + (  0.8672882976 * yv[3])
            + ( -0.5531263498 * yv[4]) + ( -0.7286735628 * yv[5])
            + ( -0.9163679567 * yv[6]) + (  2.3587872690 * yv[7]);
  return yv[8];
}
/*HIGHPASS FILTER*/

#define NZEROS_HPF 2
#define NPOLES_HPF 2
#define GAIN_HPF   1.052651765e+00

static float xvHPF[NZEROS_HPF + 1], yvHPF[NPOLES_HPF + 1];

int HighPassFilter(int analogValue) {
  xvHPF[0] = xvHPF[1]; xvHPF[1] = xvHPF[2];
  xvHPF[2] =analogValue/ GAIN_HPF;
  yvHPF[0] = yvHPF[1]; yvHPF[1] = yvHPF[2];
  yvHPF[2] =   (xvHPF[0] + xvHPF[2]) - 2 * xvHPF[1]
            + ( -0.9565436765 * yvHPF[0]) + (  1.9555782403 * yvHPF[1]);
  return yvHPF[2];
}

bool isAttach() {
  if ((digitalRead(D5) == 1 && digitalRead(D6) == 1)) {
    return false;
  } else {
    return true;
  }
}

void setup() {


  oled.begin(SSD1306_SWITCHCAPVCC, OLED_Address);
  oled.setCursor(0, 0);
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.display();
  delay(20);


  /*WIFI INITIALIZATION*/
  //  WiFi.begin(NAMA_AP, PASSWD);
  //  while (WiFi.status() != WL_CONNECTED) {
  //    delay(500);
  //  }

  /*MQTT INITIALIZATION*/
  //  while (!client.connected()) {
  //    Serial.println("Connecting to MQTT...");
  //
  //    if (client.connect("ESP8266Client", mqttUser, mqttPassword )) {
  //
  //      Serial.println("connected");
  //
  //    } else {
  //
  //      Serial.print("failed with state ");
  //      Serial.print(client.state());
  //      delay(2000);
  //
  //    }
  //  }

  // initialize the serial communication:
  Serial.begin(115200);
  pinMode(D5, INPUT); // Setup for leads off detection LO +
  pinMode(D6, INPUT); // Setup for leads off detection LO -

  EMA_S_low = analogRead(A0);      //set EMA S for t=1
  EMA_S_high = analogRead(A0);

}

void loop() {

  if (isAttach() == 0) {
    oled.setRotation(2);
    oled.clearDisplay();
    oled.setTextColor(WHITE);
    oled.setCursor(30, 20);
    oled.println("ELEKTRODA");
    oled.setCursor(15, 35);
    oled.println("TIDAK TERPASANG");
    oled.display();
    lastPlug = 0;

  } else if (isAttach() == 1) {


    oled.setRotation(0);

    // kasus tengah jalan keputus
    if (isAttach() == 1 && lastPlug == 0) {
      oled.clearDisplay();
      x = 0;
      lastX = 0;

    }
    //kondisi overflow, perlu di reset lagi
    else if (x > 128) {

      oled.clearDisplay();
      x = 0;
      lastX = 0;
    }
    if (millis() > time_now2 + periode) {
      time_now2 = millis();
      sensorValue = analogRead(A0);    //read the sensor value using ADC
      filteredSignal = BandPassFilter(HighPassFilter(sensorValue));
      Serial.println(filteredSignal);
    }

    if (millis() > time_now + periode) {

      yData = 32 - (filteredSignal / 18);
      oled.writeLine(lastX, lastY, x, yData, WHITE);

      lastY = yData;
      lastX = x;

      x++;
      time_now = millis();
      oled.display();

    }
    lastPlug = 1;
  }





}
