#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_MLX90614.h>
#include <Smoothed.h>
#include <SPI.h>
Smoothed<float> degerNBZ;
Smoothed<float> degerSCK;

float smoothed_degerSCK = 0;
float smoothed_degerNBZ = 0;

#define REPORTING_PERIOD_MS 1000
#define ATESOLCER_PERIOD_MS 1500
#define GSM_PERIOD_MS 500
#define SMS_ZAMAN_MS 10000
String Data_SMS;

LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial GSM(7, 8); // 10gsm tx 11 rx
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
PulseOximeter pox;

static const int PowerPin = 9;   // DP9 GSM modül açma/kapama pini
static const int BuzzerPin = 11; // ALARM PINI

static const String mesajServisNu = "+905429800033";      // değiştirin
static const String gonderilecekKisiNu = "+905054024256"; // değiştirin

const float nabizReferans = 110.0;
const float vucutIsisiReferans = 39.5;
const byte kalibrasyon = 4; // vücut ısısı eklenecek değer

float vucutIsisi = 0.0;
float nabiz = 0.0;
String mesaj = "";

bool smsFlag = false;
bool smsGondermeDurum = false;
bool gsmDurum = false;

unsigned long prevTime = millis();
unsigned long gorevAtesOlcer = 0;
unsigned long gorevNabizOlcer = 0;
unsigned long gorevSMS = 0;
unsigned long gorevGSM = 0;
unsigned long smsTetiklenmeZamani = 0;

bool durum = false;
byte HeartIcon[8] = {0b00000, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000};
byte BosHeartIcon[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000};
byte DereceIcon[8] = {0b00000, 0b00100, 0b01010, 0b00100, 0b00000, 0b00000, 0b00000, 0b00000};
void (*resetFunc)(void) = 0; // resetFunc is a pointer to the reset function.
void resetArduino()
{
  Serial.println(F("Reset"));
  resetFunc();
}
void onBeatDetected()
{
  Serial.print(F("Nabız atışı!"));
  Serial.println(nabiz);
  lcd.setCursor(13, 0); // İlk satırın başlangıç noktası
  lcd.write(byte(0));
}
void lcdGuncelle()
{
  lcd.clear();
  if (smsGondermeDurum)
  {

    lcd.setCursor(0, 0);
    lcd.print(mesaj);
    Serial.println(mesaj);
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("Nabiz:");
    lcd.setCursor(7, 0);
    lcd.print(smoothed_degerNBZ);
    lcd.setCursor(0, 1);
    lcd.print("VucutIsi:");
    lcd.setCursor(9, 1);
    lcd.print(smoothed_degerSCK);
    lcd.setCursor(14, 1);
    lcd.write(byte(2));
    lcd.setCursor(15, 1);
    lcd.print("C");
  }
}
String time()
{
  String content = "00/00/00 00:00";
  char character;
  GSM.println("AT+CLTS=1\r");
  delay(100);
  GSM.println("AT+CCLK?"); // read the time
  delay(1000);
  while (GSM.available())
  {
    character = GSM.read();
    content.concat(character);
  }

  if (content != "")
  {
    int ilkkonum = content.indexOf('"');
    // int sonkonum = content.lastIndexOf ('"');
    // Serial.println(ilkkonum);//Serial.println(sonkonum);
    // Serial.print(F("content"));
    // Serial.println(content);

    String yil = content.substring(ilkkonum + 1, ilkkonum + 3);
    String ay = content.substring(ilkkonum + 4, ilkkonum + 6);
    String gun = content.substring(ilkkonum + 7, ilkkonum + 9);
    String saat = content.substring(ilkkonum + 10, ilkkonum + 12);
    String dakika = content.substring(ilkkonum + 13, ilkkonum + 15);
    content = gun + "/" + ay + "/" + yil + " " + saat + ":" + dakika;
    // return (content);
  }
  return (content);
}
void sensorCalistir()
{

  if (!pox.begin())
  {
    Serial.println(F("Nabız sensor çalışmadı."));
    while (1)
      ;
  }
  else
  {
    Serial.println(F("Nabız sensor çalıştı."));
  }

  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);

  if (mlx.begin())
  {
    Serial.println(F("Ateş ölçer çalıştı."));
  }
  else
  {
    Serial.println(F("Ateş ölçer çalışmadı."));
    while (1)
      ;
  }
}
void gsmAc()
{
  digitalWrite(PowerPin, HIGH);
  delay(1000);
  digitalWrite(PowerPin, LOW);
  delay(1000); // Gsm aktif olmasını bekle 10sn.
  Serial.println(F("GSM açıldı"));
}
void gsmKapat()
{
  digitalWrite(PowerPin, 1);
  delay(3000); // Gsm kapanmasını bekle 3sn.
  digitalWrite(PowerPin, 0);
  Serial.println(F("GSM kapatıldı"));
  smsGondermeDurum = false;
  lcdGuncelle();
}
void alarmCal()
{
  // digitalWrite(BuzzerPin, HIGH);
  // delay(1000);  // 1sn bekle.
  // digitalWrite(BuzzerPin, LOW);
  tone(BuzzerPin, 1000, 500);
}
void MesajGonder()
{
  alarmCal();
  mesaj = "Sms Gndriliyor";
  lcdGuncelle();

  smsFlag = true;
  smsTetiklenmeZamani = millis(); // sms gönderme zamanını ayarla
  // Serial.print(F("sms tetiklenme zamanı:"));
  // Serial.println(smsTetiklenmeZamani);
  String msjtime = time();
  // Serial.print(F("Mesaj Zamanı :"));
  // Serial.println(msjtime);
  // delay(100);

  // char buffer[125] = {'\0'};
  char str_isi[5];
  char str_nabiz[6];
  char tarihs[14];
  // Serial.print(F("mesaj time uzunluk"));Serial.println(msjtime.length());
  int uzunluk = msjtime.length();
  for (int i = 0; i < uzunluk; i++)
  {
    tarihs[i] = msjtime[i];
  }
  dtostrf(nabiz, 6, 2, str_nabiz);
  dtostrf(vucutIsisi, 5, 2, str_isi);
  Data_SMS = "";
  Data_SMS = "Tehlikeli Durum Olustu\nVucut isisi: " + String(str_isi) + " *C\nNabiz: " + String(str_nabiz) + " bpm\nMesaj Zamani:" + String(tarihs);
Data_SMS=Data_SMS.substring(0,Data_SMS.length()-3);
  delay(100);

  Serial.print("SMS :");
  Serial.println(Data_SMS);

  if (Data_SMS != NULL)
  {
    GSM.println("AT+CMGF=1\r");
    delay(100);
    GSM.println("AT+CSCA=\"" + mesajServisNu + "\"");
    delay(100);
    GSM.println("AT+CMGS=\"" + gonderilecekKisiNu + "\"");
    delay(100);
    GSM.println(Data_SMS);
    delay(1000);
    GSM.println((char)26);
    delay(100);
    GSM.println();
    delay(5000);
    Serial.println(F("sms gönderildi"));
  }
  else
  {
    Serial.println(F("sms gitmedi SMS BOS"));
  }

  smsGondermeDurum = false;
  lcdGuncelle();
  // pox.resume();
  delay(1000);
  resetArduino();
}
void atesOlcerGoster(unsigned long currentTime)
{
  // Serial.print(F("ateş:"));Serial.println(mlx.readObjectTempC());
  if (currentTime - gorevAtesOlcer > ATESOLCER_PERIOD_MS && smsGondermeDurum == false)
  {
    // Serial.print(F("ateş:"));
    // Serial.println(mlx.readObjectTempC());
    vucutIsisi = mlx.readObjectTempC() + kalibrasyon;
    degerSCK.add(vucutIsisi);
    smoothed_degerSCK = degerSCK.get();
    if (smoothed_degerSCK > vucutIsisiReferans)
    {
      if (smsFlag == false)
      {
        smsGondermeDurum = true;
        lcdGuncelle();
        MesajGonder();
      }
    }
    lcdGuncelle();
    gorevAtesOlcer = currentTime;
  }
}
void nabizOlcerGoster(unsigned long currentTime)
{
  if (currentTime - gorevNabizOlcer > REPORTING_PERIOD_MS && smsGondermeDurum == false)
  {
    nabiz = pox.getHeartRate();
    degerNBZ.add(nabiz);
    smoothed_degerNBZ = degerNBZ.get();
    durum = !durum;
    if (smoothed_degerNBZ > 10)
    {
      lcd.setCursor(13, 0); // İlk satırın başlangıç noktası
      if (durum)
        lcd.write(byte(0));
      else
        lcd.write(byte(1));
    }
    if (smoothed_degerNBZ > nabizReferans && smsGondermeDurum == false)
    {
      // pox.shutdown();
      if (smsFlag == false)
      {
        smsGondermeDurum = true;
        lcdGuncelle();
        MesajGonder();
        for (int i = 0; i < 10; i++)
          degerNBZ.add(0);
        smoothed_degerNBZ = degerNBZ.get();
        Serial.print(F("sms sonrası ortalama değer: "));
        Serial.println(smoothed_degerNBZ);
      }
    }
    lcdGuncelle();
    gorevNabizOlcer = currentTime;
  }
}
void serialKontroller(unsigned long currentTime)
{
  if (currentTime - gorevGSM > GSM_PERIOD_MS)
  {
    // put your main code here,to run repeatedly:
    while (GSM.available())
    {
      Serial.write(GSM.read());
    }
    while (Serial.available())
    {
      byte b = Serial.read();
      if (b == '*')
        GSM.write(0x1a);
      else if (b == '{')
        gsmAc();
      else if (b == '}')
        gsmKapat();
      // else if (b == '@')
      //   MesajGonder();
      else
        GSM.write(b);
    }
    gorevGSM = currentTime;
  }
}
void smsGondermeZamanAsimi(unsigned long currentTime)
{
  if (currentTime - smsTetiklenmeZamani > SMS_ZAMAN_MS && smsFlag == true)
  {
    smsFlag = false;
    Serial.print(F("sms flag:"));
    Serial.println(smsFlag);
  }
}
void setup()
{

  degerNBZ.begin(SMOOTHED_AVERAGE, 10);
  degerSCK.begin(SMOOTHED_AVERAGE, 5);

  GSM.begin(19200);
  Serial.begin(9600);
  pinMode(PowerPin, OUTPUT);
  pinMode(BuzzerPin, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  lcd.begin();
  sensorCalistir();
  lcd.createChar(0, HeartIcon);
  lcd.createChar(1, BosHeartIcon);
  lcd.createChar(2, DereceIcon);
}
void loop()
{

  int sensorVal = digitalRead(2);
  if (sensorVal == LOW)
  {
    Serial.println(F("Sms Gndrme Butona basıldı"));
    delay(500);
    if (smsFlag == false && smsGondermeDurum == false)
    {
      smsGondermeDurum = true;
      lcdGuncelle();
      MesajGonder();
      // pox.update();
    }
    else
    {
      Serial.println(F("Sms gönderme durumu aktif"));
    }
  }

  unsigned long currentTime = millis();
  atesOlcerGoster(currentTime);
  nabizOlcerGoster(currentTime);
  smsGondermeZamanAsimi(currentTime);
  serialKontroller(currentTime);
  pox.update();
}