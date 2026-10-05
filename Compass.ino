#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADXL345_U.h>

#include <MechaQMC5883.h>

#include <math.h>


// ========================================
// OLED
// ========================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// ========================================
// ADXL345
// ========================================

Adafruit_ADXL345_Unified accelerometer =
  Adafruit_ADXL345_Unified(12345);


// ========================================
// QMC5883L
// ========================================

MechaQMC5883 qmc;


// ========================================
// GET COMPASS DIRECTION
// ========================================

const char* getDirection(float heading) {

  if (heading >= 337.5 || heading < 22.5)
    return "N";

  if (heading < 67.5)
    return "NE";

  if (heading < 112.5)
    return "E";

  if (heading < 157.5)
    return "SE";

  if (heading < 202.5)
    return "S";

  if (heading < 247.5)
    return "SW";

  if (heading < 292.5)
    return "W";

  return "NW";
}


// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println();
  Serial.println(F("=============================="));
  Serial.println(F("       STEM LAB"));
  Serial.println(F("=============================="));


  // ----------------------------------------
  // I2C
  // ----------------------------------------

  Wire.begin();
  Wire.setClock(100000);

  Serial.println(F("I2C READY"));


  // ----------------------------------------
  // OLED
  // ----------------------------------------

  Serial.println(F("Starting OLED..."));

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    Serial.println(F("OLED FAILED"));

    while (true);
  }

  Serial.println(F("OLED SUCCESS"));


  // ----------------------------------------
  // ADXL345
  // ----------------------------------------

  Serial.println(F("Starting ADXL345..."));

  if (!accelerometer.begin()) {

    Serial.println(F("ADXL345 FAILED"));

    while (true);
  }

  Serial.println(F("ADXL345 SUCCESS"));


  // ----------------------------------------
  // QMC5883L
  // ----------------------------------------

  Serial.println(F("Starting QMC5883L..."));

  qmc.init();

  Serial.println(F("QMC SUCCESS"));

  Wire.setClock(100000);
}


// ========================================
// LOOP
// ========================================

void loop() {

  // ======================================
  // READ ACCELEROMETER
  // ======================================

  sensors_event_t event;

  accelerometer.getEvent(&event);

  float x = event.acceleration.x;
  float y = event.acceleration.y;
  float z = event.acceleration.z;


  // ======================================
  // READ MAGNETOMETER
  // ======================================

  int magX;
  int magY;
  int magZ;

  qmc.read(
    &magX,
    &magY,
    &magZ
  );


  // ======================================
  // CALCULATE HEADING
  // ======================================

  float heading =
    atan2(
      (float)magY,
      (float)magX
    ) * 180.0 / PI;


  // Convert negative angle to 0-360
  if (heading < 0) {
    heading += 360.0;
  }


  // ======================================
  // GET DIRECTION
  // ======================================

  const char* direction =
    getDirection(heading);


  // ======================================
  // SERIAL MONITOR
  // ======================================

  Serial.print(F("ACC: "));

  Serial.print(x, 2);
  Serial.print(F(", "));

  Serial.print(y, 2);
  Serial.print(F(", "));

  Serial.print(z, 2);

  Serial.print(F(" | MAG: "));

  Serial.print(magX);
  Serial.print(F(", "));

  Serial.print(magY);
  Serial.print(F(", "));

  Serial.print(magZ);

  Serial.print(F(" | HEADING: "));

  Serial.print(heading, 1);

  Serial.print(F(" | DIR: "));

  Serial.println(direction);


  // ======================================
  // OLED
  // ======================================

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);

  display.println(F("STEM LAB"));

  display.println();

  display.print(F("X: "));
  display.println(x, 2);

  display.print(F("Y: "));
  display.println(y, 2);

  display.print(F("Z: "));
  display.println(z, 2);

  display.println();

  display.print(F("HEAD: "));
  display.print(heading, 0);
  display.println(F(" deg"));

  display.print(F("DIR: "));
  display.println(direction);

  display.display();


  // Update approximately 5 times/sec
  delay(200);
}