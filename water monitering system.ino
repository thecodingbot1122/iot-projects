#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// 20x4 LCD ka address aur size set karein
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pins Configuration (constexpr compiler issue fix karne ke liye)
constexpr int PH_PIN = 32;
constexpr int TDS_PIN = 33;
constexpr int TURBIDITY_PIN = 34;
constexpr int TEMP_PIN = 4;   // DS18B20 Data Pin
constexpr int FLOW_PIN = 23;  // Button / Flow Sensor Pin

// DS18B20 Setup
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// Global Variables
volatile int pulseCount = 0;
float totalLiters = 0.0;
unsigned long oldTime = 0; 
bool buttonStateChanged = false; // Instant update check flag

// Interrupt Function (1 click = 1 Liter)
void IRAM_ATTR pulseCounter() {
  pulseCount++;
  buttonStateChanged = true; // Interrupt trigger hote hi flag true hoga
}

void setup() {
  // Pins Mode Setup
  pinMode(PH_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(TURBIDITY_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP); // GND ke sath button lagayein
  
  // Interrupt Setup (Foran trigger hone ke liye)
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  // DS18B20 Sensor Initialize aur Non-Blocking Mode Setup
  sensors.begin();
  sensors.setWaitForConversion(false); 
  sensors.requestTemperatures();       

  // LCD Screen Initialize
  lcd.init();
  lcd.backlight();
  
  // Welcome Screen
  lcd.setCursor(2, 1); 
  lcd.print("SMART WATER SYSTEM");
  lcd.setCursor(4, 2); 
  lcd.print("ONLINE READY");
  delay(2500);
  lcd.clear();
}

void loop() {
  // --- INSTANT UPDATE ON BUTTON PRESS ---
  // Agar button press hua hai toh 1 second ke delay ka wait nahi karega, foran add karega
  if (buttonStateChanged) {
    noInterrupts(); 
    int currentPulses = pulseCount;
    pulseCount = 0; 
    buttonStateChanged = false; // Reset flag
    interrupts();   

    totalLiters += (float)currentPulses; // 1 Pulse = 1 Liter add ho gaya

    // Screen par Liter value ko foran bina delay ke update karein
    lcd.setCursor(0, 2);
    lcd.print("USED: "); lcd.print(totalLiters, 1); lcd.print(" Liters     ");
  }

  // --- STANDARD 1-SECOND SENSOR REFRESH ---
  if ((millis() - oldTime) > 1000) {
    oldTime = millis();

    // Read Analog Sensors (0-4095 mapping)
    float phValue = map(analogRead(PH_PIN), 0, 4095, 0, 14);
    float tdsValue = map(analogRead(TDS_PIN), 0, 4095, 0, 1000);
    float turbidityNTU = map(analogRead(TURBIDITY_PIN), 0, 4095, 100, 0); 
    
    // Read DS18B20 Temperature without blocking
    float temperatureC = sensors.getTempCByIndex(0);
    sensors.requestTemperatures(); 

    // Error check
    if (temperatureC == DEVICE_DISCONNECTED_C || temperatureC < -50) {
      temperatureC = 0.0;
    }

    // Contamination / Safety Logic
    String statusMsg = "SAFE";
    if (phValue < 6.5 || phValue > 8.5 || tdsValue > 500 || turbidityNTU > 5) {
      statusMsg = "CONTAMINATED!";
    }

    // --- 20x4 LCD DASHBOARD DISPLAY ---
    // Line 1: pH aur TDS
    lcd.setCursor(0, 0);
    lcd.print("pH: "); lcd.print(phValue, 1);
    lcd.setCursor(10, 0);
    lcd.print("TDS: "); lcd.print((int)tdsValue); lcd.print("ppm ");

    // Line 2: Turbidity aur Temperature
    lcd.setCursor(0, 1);
    lcd.print("TURB: "); lcd.print((int)turbidityNTU); lcd.print("NTU");
    lcd.setCursor(11, 1);
    lcd.print("TMP: "); lcd.print((int)temperatureC, 1); lcd.print("C ");

    // Line 3: Total Water Used (Backup update)
    lcd.setCursor(0, 2);
    lcd.print("USED: "); lcd.print(totalLiters, 1); lcd.print(" Liters     ");

    // Line 4: System Status Message
    lcd.setCursor(0, 3);
    lcd.print("STATUS: ");
    lcd.setCursor(8, 3);
    lcd.print("____________"); 
    lcd.setCursor(8, 3);
    lcd.print(statusMsg); 
  }
}
