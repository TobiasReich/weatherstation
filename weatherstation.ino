#include <Adafruit_SGP30.h>

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "ImageData.h"
#include <stdlib.h>
#include <DHT.h>
#include <Wire.h> 
#include <Adafruit_BMP085.h>
#include <pins_arduino.h>

// 400x300, 4.2inch E-Ink raw display
// 9 x 7 cm display size
// https://www.waveshare.com/4.2inch-e-Paper.htm

#define DHTTYPE    DHT11  // DHT 11
#define DHTPIN 32         //7th from bottom right
DHT dht(DHTPIN, DHTTYPE); // Digital Humidity/Temperature sensor
Adafruit_BMP085 bmp;      // I2C controlled Barometer

UBYTE *BlackImage;
UWORD Imagesize = ((EPD_4IN2_WIDTH % 8 == 0) ? (EPD_4IN2_WIDTH / 8 ) : (EPD_4IN2_WIDTH / 8 + 1)) * EPD_4IN2_HEIGHT;

uint32_t REFRESH_TIME = 5000;

// Rain Sensor
const int analogPin=A0;   //the AO of the module attach to A0 (PIN 36) (Left 3rd from top)
const int digitalPin=25;  //D0 attach to pin25 --> DAC_1 (GPIO25) (Left 9th from top)
int Astate=0;             //store the value of A0
boolean Dstate=0;         //store the value of D0

/*Soil Humidity
  - fully in Water          = 1100
  - mid humidity            = 1300
  - outside (at normal air) = 2100 */
#define SOIL_DRYNESS_HIGH 2000   
#define SOIL_DRYNESS_MEDIUM 1300
#define SOIL_HUMIDITY_PIN 34 // Pin 34 5th from bottom right

// Functions declaration
void drawSun(int x, int y);
void drawWaterdrop(int x, int y);
void drawProgressBar(int x, int y, int percent);
void showSplashscreen();
void processTemperatureHumidtyPressure();
void processRainSensor();
void processSoilHumidity();

// --------------------------------------------



// Air quality

void drawSun(int x, int y){
  Paint_DrawCircle(x, y, 5, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  Paint_DrawLine(x-12, y, x-7, y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // left
  Paint_DrawLine(x+12, y, x+7, y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // right
  Paint_DrawLine(x, y-12, x, y-7, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // above
  Paint_DrawLine(x, y+12, x, y+7, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // below

  Paint_DrawLine(x-10, y-10, x-6, y-6, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // topleft
  Paint_DrawLine(x+10, y-10, x+6, y-6, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // topright
  Paint_DrawLine(x-10, y+10, x-6, y+6, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // topleft
  Paint_DrawLine(x+10, y+10, x+6, y+6, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // topright
}


void drawWaterdrop(int x, int y){
  Paint_DrawCircle(x, y+5, 5, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawCircle(x, y, 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawCircle(x, y-2, 1, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}


void drawProgressBar(int x, int y, int percent){
  Paint_DrawRectangle(x, y, x+50, y+15, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawRectangle(x, y, x+(percent/2), y+15, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}


void showSplashscreen() {
  Paint_NewImage(BlackImage, EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT, 0, WHITE);
  Serial.println("SelectImage:BlackImage\r\n");
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  Paint_DrawString_EN(105, 140, "Wetterstation", &Font24, WHITE, BLACK);
  Paint_DrawLine(95, 170, 340, 170, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED); // above
  EPD_4IN2_Display(BlackImage);
}


void processTemperatureHumidtyPressure(){
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int pressure = bmp.readPressure()/100;

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  } else {
    // Compute heat index in Celsius (isFahreheit = false)
    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.print(F("%, Temperature: "));
    Serial.print(temperature);
    Serial.print(F("°C, Heat index: "));
    Serial.print(heatIndex);
    Serial.println(F("°C "));

    char tempbuffer [7];
    sprintf (tempbuffer, "%.1f C", temperature);
    
    char hicbuffer [7];
    sprintf (hicbuffer, "%.1f C", heatIndex);

    // Temperature
    Paint_DrawString_EN(20, 50, "Temperatur:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 50, tempbuffer, &Font20, WHITE, BLACK);

    // Felt 
    Paint_DrawString_EN(20, 80, "Gefuehlt:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 80, hicbuffer, &Font20, WHITE, BLACK);

    // Humidity
    char humidbuffer [7];
    sprintf (humidbuffer, "  %.0f %%", humidity);
    Paint_DrawString_EN(20, 110, "Luftfeucht.:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 110, humidbuffer, &Font20, WHITE, BLACK);
    drawProgressBar(320, 110, (int)humidity);

    //Pressure
    char pressurebuffer [9];
    sprintf (pressurebuffer, "%4d hpa", pressure);
    Paint_DrawString_EN(20, 140, "Luftdruck:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 140, pressurebuffer, &Font20, WHITE, BLACK);
  }
}


void processRainSensor(){
  Paint_DrawString_EN(20, 170, "Regen:", &Font20, WHITE, BLACK);
  Astate=analogRead(analogPin);  //read the value of A0
  Serial.print("A0: ");
  Serial.println(Astate);  //print the value in the serial monitor
  Dstate=digitalRead(digitalPin);  //read the value of D0
  Serial.print("D0: ");
  Serial.println(Dstate);

  //TODO Might use the Analogue value instead if more variety is needed
  if(Dstate==HIGH) {
    Serial.println("NO RAIN");
    Paint_DrawString_EN(200, 170, "Nein", &Font20, WHITE, BLACK);
  } else {
    Serial.println("RAIN");
    //if the value of D0 is LOW
    Paint_DrawString_EN(200, 170, "Ja", &Font20, WHITE, BLACK);
    drawWaterdrop(320, 170);
    drawWaterdrop(335, 170);
    drawWaterdrop(350, 170);
  }
}


void processSoilHumidity(){
  int soilDryness = analogRead(SOIL_HUMIDITY_PIN);
  Serial.println(soilDryness);

  Paint_DrawString_EN(20, 200, "Boden:", &Font20, WHITE, BLACK);

  // print out the value you read:
  if (soilDryness > SOIL_DRYNESS_HIGH){
    // DRYEST
    Serial.println("DRY");
    Paint_DrawString_EN(200, 200, "TROCKEN", &Font20, WHITE, BLACK);
    drawSun(320, 205);  
  } else if (soilDryness > SOIL_DRYNESS_MEDIUM) {
    // MID HUMID
    Serial.println("MID");
    Paint_DrawString_EN(200, 200, "Gut", &Font20, WHITE, BLACK);
    drawWaterdrop(320, 205);
  } else {
    // WET
    Serial.println("Wet");
    Paint_DrawString_EN(200, 200, "FEUCHT", &Font20, WHITE, BLACK);
    drawWaterdrop(320, 205);
    drawWaterdrop(335, 205);
  }
}


// --------------------------------------------------------


void setup()
{
  DEV_Module_Init();

  Serial.begin(115200);
  Serial.println("Weather Station");

  // Init the Barometer
  if (!bmp.begin()) {
    Serial.println("BMP180 Sensor not found ! ! !");
    while (1) { }
  }

  // Start Humidity / Temp sensor
  dht.begin();

  Serial.println(F("DHTxx Unified Sensor Example"));

  EPD_4IN2_Init_Fast();
  EPD_4IN2_Clear();
  DEV_Delay_ms(1500);
  
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    Serial.println("Failed to apply for black memory...\r\n");
    while (1);
  }

  showSplashscreen();

  DEV_Delay_ms(500);
}


void loop()
{
  // Delay between measurements.
  DEV_Delay_ms(REFRESH_TIME);

  // Draw the results screen
  Paint_Clear(WHITE);

  // Write parameters
  Paint_DrawString_EN(20, 20, "Wetter:", &Font24, WHITE, BLACK);

  // --- TEMPERATURE, HUMIDITY, PRESSURE ---
  processTemperatureHumidtyPressure();

  // ------ RAIN SENSOR ------
  processRainSensor();

  // ------ Soil Humidity ------
  processSoilHumidity();

  // ------ Draw the image ------
  EPD_4IN2_Display(BlackImage);
}



