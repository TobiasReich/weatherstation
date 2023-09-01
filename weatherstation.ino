#include <Adafruit_SGP30.h>
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "ImageData.h"
#include <stdlib.h>
#include <DHT.h>
#include <Wire.h> 
#include <Adafruit_BMP085.h> // Barometer - I2C:0x77
#include <pins_arduino.h>
#include "Adafruit_SGP30.h" // Air Quality I2C: 58


// 400x300, 4.2inch E-Ink raw display
// 9 x 7 cm display size
// https://www.waveshare.com/4.2inch-e-Paper.htm

#define DHTTYPE    DHT11  // DHT 11
#define DHTPIN 32         //7th from bottom right
DHT dht(DHTPIN, DHTTYPE); // Digital Humidity/Temperature sensor
Adafruit_BMP085 bmp;      // I2C controlled Barometer

UBYTE *BlackImage;
UWORD Imagesize = ((EPD_4IN2_WIDTH % 8 == 0) ? (EPD_4IN2_WIDTH / 8 ) : (EPD_4IN2_WIDTH / 8 + 1)) * EPD_4IN2_HEIGHT;

// Millis for the measurement update
// Note this also refreshes the screen (visibly).
// Since this can cause distractions and the weather 
// doesn't change that rapidly, it is perfectly fine to pick
// high values like
// 600000 -> every 10 minutes
// 1800000 -> every 30 minutes
// 3600000 -> every hour 
// or even more
uint32_t REFRESH_TIME = 1800000; 


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
#define SOIL_DRYNESS_MEDIUM 1150
#define SOIL_HUMIDITY_PIN 34 // Pin 34 5th from bottom right


// AIR QUALITY constants
#define VOC_LOW 25
#define VOC_MED 75
#define CO2_LOW 475
#define CO2_MED 600

#define DEBUG true
#define STORED_VALUES 20
#define DIAGRAM_HEIGHT 25
#define DIAGRAM_WIDTH 200

// Functions declaration
void drawSun(int x, int y);
void drawWaterdrop(int x, int y);
void drawProgressBar(int x, int y, int percent);
void showSplashscreen();
float getTemperature();
float getHumidty();
void processTemperatureHumidtyPressure(float temperature, float humidity);
void processRainSensor();
void processSoilHumidity();
void processAirQuality();


// --------------------------------------------


// Air quality
Adafruit_SGP30 sgp;

// The air quality calculation seems a bit fickle so it might be more reliable 
// to store the previous value and use the mean value
int previousTvoc = 0;
int previousCo2  = 400;

// For a "histogram" of previous values
int previousCo2Values[STORED_VALUES] = {0};
int previousTvocValues[STORED_VALUES] = {0};


uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

// --- Air Quality

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
  if (percent > 100) {
    percent = 100;
  } else if (percent < 0){
    percent = 0;
  }
  Paint_DrawRectangle(x, y, x+50, y+15, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawRectangle(x, y, x+(percent/2), y+15, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}


/** Draws a diagram of values. For simplicity I gave it a fixed size of 10 elements.
    the min parameter subtracts this value from all values so the min value becomes "0"
    in the graph
    1. find the max value of the array
    2. calculate the scale factor so we cann draw the diagram propperly
    3. draw bars (for simplicity) for each of the 10 values */
void drawDiagramm(int (&values)[STORED_VALUES], int x, int y, int min = 0){
  // Draw an empty graph in any case
  Paint_DrawRectangle(x, y, x+DIAGRAM_WIDTH, y+DIAGRAM_HEIGHT, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  // Calculate the values
  int bucketWidth = DIAGRAM_WIDTH / STORED_VALUES;
  int max = 0;
  float scale = 0.0;

  for(int i=0; i<STORED_VALUES; i++){
    if ((values[i]-min) > max){
      max = values[i] - min;
    }
  }

  // no max value found, no need to render a graph
  if (max == 0){
    Serial.println("\nNo values to print!");
    return;
  }

  // Calculate the scale all values have to be multiplied before they are drawn
  // E.g. a max of 100 with a diagram height of 50 leads to a scale of 0.5f
  // E.g. the 100 value will be scaled down to 50pixels
  scale = static_cast<float>(DIAGRAM_HEIGHT) / static_cast<float>(max);

  // Draw the graph now
  for(int i=0; i < STORED_VALUES; i++){
    int value = static_cast<float>(values[i]-min) * scale;
    // if the bucket is still filled with 0 values, we don't have to draw that
    if (value > 0){
      Paint_DrawRectangle(x+(i*bucketWidth), y+(DIAGRAM_HEIGHT - value),
                          x+((i+1)*bucketWidth)-1, y+DIAGRAM_HEIGHT, 
                          BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
  }
}


void showSplashscreen() {
  Paint_NewImage(BlackImage, EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT, 0, WHITE);
  Serial.println("SelectImage:BlackImage\r\n");
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  Paint_DrawString_EN(100, 140, "Wetterstation", &Font24, WHITE, BLACK);
  Paint_DrawLine(90, 165, 335, 165, BLACK, DOT_PIXEL_2X2, LINE_STYLE_DOTTED); // above
  EPD_4IN2_Display(BlackImage);
}


float getTemperature(){
  return dht.readTemperature();
}


float getHumidty(){
  return dht.readHumidity();
}

void processTemperatureHumidtyPressure(float temperature, float humidity){
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  //float humidity = dht.readHumidity();
  //float temperature = dht.readTemperature();
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
    Paint_DrawString_EN(20, 70, "Temperatur:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 70, tempbuffer, &Font20, WHITE, BLACK);

    // Felt 
    Paint_DrawString_EN(20, 100, "Gefuehlt:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 100, hicbuffer, &Font20, WHITE, BLACK);

    // Humidity
    char humidbuffer [7];
    sprintf (humidbuffer, "  %.0f %%", humidity);
    Paint_DrawString_EN(20, 130, "Luftfeucht.:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 130, humidbuffer, &Font20, WHITE, BLACK);
    drawProgressBar(320, 130, (int)humidity);

    //Pressure
    char pressurebuffer [9];
    sprintf (pressurebuffer, "%4d hpa", pressure);
    Paint_DrawString_EN(20, 160, "Luftdruck:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(200, 160, pressurebuffer, &Font20, WHITE, BLACK);
  }
}


void processSoilHumidity(){
  int soilDryness = analogRead(SOIL_HUMIDITY_PIN);
  Serial.println(soilDryness);

  Paint_DrawString_EN(20, 190, "Boden:", &Font20, WHITE, BLACK);

  // print out the value you read:
  if (soilDryness > SOIL_DRYNESS_HIGH){
    // DRYEST
    Serial.println("DRY");
    Paint_DrawString_EN(200, 190, "TROCKEN", &Font20, WHITE, BLACK);
    drawSun(325, 195);  
  } else if (soilDryness > SOIL_DRYNESS_MEDIUM) {
    // MID HUMID
    Serial.println("MID");
    Paint_DrawString_EN(200, 190, "Gut", &Font20, WHITE, BLACK);
    drawWaterdrop(325, 195);
  } else {
    // WET
    Serial.println("Wet");
    Paint_DrawString_EN(200, 190, "Nass", &Font20, WHITE, BLACK);
    drawWaterdrop(325, 195);
    drawWaterdrop(340, 195);
    drawWaterdrop(355, 195);
  }

  if (DEBUG){
    char debugText[10 * sizeof(char)];
    std::sprintf(debugText, "%d", soilDryness);
    Paint_DrawString_EN(300, 290, "Boden:" , &Font8, WHITE, BLACK);
    Paint_DrawString_EN(350, 290, debugText , &Font8, WHITE, BLACK);
  }
}


void processAirQuality(float temperature, float humidity){
  if (! sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    Paint_DrawString_EN(20, 220, "Luft: Fehler 1", &Font20, WHITE, BLACK);
    return;
  }

  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    Paint_DrawString_EN(20, 220, "Luft: Fehler 2", &Font20, WHITE, BLACK);
    return;
  }

  // If you have a temperature / humidity sensor, you can set the absolute
  // humidity to enable the humditiy compensation for the air quality signals
  sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));


  int tvoc = sgp.TVOC;
  int co2  = sgp.eCO2;
  Serial.print("TVOC "); Serial.print(tvoc); Serial.println(" ppb\t");
  Serial.print("eCO2 "); Serial.print(co2); Serial.println(" ppm");

  // in order to flatten air quality values (which are a bit fickle)
  // we calculate tvoc and co2 as the mean of this and the last measurement
  tvoc = (previousTvoc + tvoc) / 2; 
  co2  = (previousCo2 + co2) /2;

  boolean isHigh = false;

  Paint_DrawString_EN(20, 220, "TVOC:", &Font20, WHITE, BLACK);
  if(tvoc < VOC_LOW){
    Paint_DrawString_EN(100, 220, "OK", &Font20, WHITE, BLACK);
  } else if (tvoc < VOC_MED) {
    Paint_DrawString_EN(100, 220, "GUT", &Font20, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(100, 220, "HOCH", &Font20, WHITE, BLACK);
    isHigh = true;
  }

  Paint_DrawString_EN(20, 250, "CO2", &Font20, WHITE, BLACK);
  if(co2 < CO2_LOW){
    Paint_DrawString_EN(100, 250, "OK", &Font20, WHITE, BLACK);
  } else if (co2 < CO2_MED) {
    Paint_DrawString_EN(100, 250, "GUT", &Font20, WHITE, BLACK);
  } else {
    Paint_DrawString_EN(100, 250, "HOCH", &Font20, WHITE, BLACK);
    isHigh = true;
  }

  // if any of the air quality values is "too high" a message "open the windows" appears
  if(isHigh){
    Paint_DrawString_EN(200, 20, "FENSTER AUF", &Font24, WHITE, BLACK);
  }

  if (DEBUG){
    // Writing some debug values    
    char airQualityText[10 * sizeof(char)];

    Paint_DrawString_EN(20, 290, "Debug: ", &Font8, WHITE, BLACK);
    std::sprintf(airQualityText, "%d", tvoc);
    Paint_DrawString_EN(80, 290, "TVOC: " , &Font8, WHITE, BLACK);
    Paint_DrawString_EN(140, 290, airQualityText , &Font8, WHITE, BLACK);

    Paint_DrawString_EN(180, 290, "CO2: " , &Font8, WHITE, BLACK);
    std::sprintf(airQualityText, "%d", co2);
    Paint_DrawString_EN(220, 290, airQualityText , &Font8, WHITE, BLACK);
  }

  // Diagram magic here
  // 1. shift all values of the "diagram array" to the left,
  // 2. add the current value to the end

  for (int i=0; i<STORED_VALUES; i++){
    previousTvocValues[i] = previousTvocValues[i+1];
    previousCo2Values[i] = previousCo2Values[i+1];
  }
  
  previousTvocValues[STORED_VALUES-1] = tvoc;
  previousCo2Values[STORED_VALUES-1] = co2;

  drawDiagramm(previousTvocValues, 200, 220);
  drawDiagramm(previousCo2Values, 200, 250, 400);
}


// --------------------------------------------------------
// --------------------------------------------------------
// --------------------------------------------------------


void setup(){
  DEV_Module_Init();

  Serial.begin(115200);
  while (!Serial) {
  ;  // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Wetter Station");

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


  Serial.println("Starting SGP30");

  if (! sgp.begin()){
    Serial.println("SGP Air quality sensor not found :(");
    while (1);
  }

  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  showSplashscreen();

  DEV_Delay_ms(500);
}


void loop() {
  // Draw the results screen
  Paint_Clear(WHITE);

  // Write parameters
  Paint_DrawString_EN(20, 20, "Wetter:", &Font24, WHITE, BLACK);
  Paint_DrawLine(20, 43, 135, 43, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // above

  // --- TEMPERATURE, HUMIDITY, PRESSURE ---
  float temp = getTemperature();
  float humidty = getHumidty();
  processTemperatureHumidtyPressure(temp, humidty);

  // ------ Soil Humidity ------
  processSoilHumidity();

  // ------ Air Quality ------
  processAirQuality(temp, humidty);

  // ------ Draw the image ------
  EPD_4IN2_Display(BlackImage);

  // Delay between measurements.
  DEV_Delay_ms(REFRESH_TIME);
}



