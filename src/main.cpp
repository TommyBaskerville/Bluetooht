#include <Arduino.h>        // Incluye las funciones básicas de Arduino
#include <DabbleESP32.h>
#include <ESP32Servo.h>

// Definición de pines
#define steeringPin 32      // Pin del servomotor
#define IN1 21               // Pin IN1 del motor
#define IN2 19               // Pin IN2 del motor

constexpr int STEERING_CENTER = 82;
constexpr int STEERING_TRIM = 0;
constexpr int STEERING_MAX_DELTA = 20;
constexpr int STEERING_ABS_MIN = 0;
constexpr int STEERING_ABS_MAX = 180;
constexpr int JOYSTICK_MIN = -7;
constexpr int JOYSTICK_MAX = 7;
constexpr int JOYSTICK_DEADZONE = 1;
constexpr uint32_t SERVO_STEP_INTERVAL_MS = 15;
constexpr int SERVO_STEP_DEG = 1;

int steeringCurrent = STEERING_CENTER;
int steeringTarget = STEERING_CENTER;
uint32_t lastServoStepMs = 0;

// Objeto para el servomotor
Servo steeringServo;

int clampSteeringAngle(int angle) {
  const int steeringCenterWithTrim = STEERING_CENTER + STEERING_TRIM;
  const int steeringLimitMin = max(STEERING_ABS_MIN, steeringCenterWithTrim - STEERING_MAX_DELTA);
  const int steeringLimitMax = min(STEERING_ABS_MAX, steeringCenterWithTrim + STEERING_MAX_DELTA);
  return constrain(angle, steeringLimitMin, steeringLimitMax);
}

void updateSteeringSmooth() {
  uint32_t now = millis();
  if (now - lastServoStepMs < SERVO_STEP_INTERVAL_MS) {
    return;
  }

  lastServoStepMs = now;

  if (steeringCurrent < steeringTarget) {
    steeringCurrent = min(steeringCurrent + SERVO_STEP_DEG, steeringTarget);
  } else if (steeringCurrent > steeringTarget) {
    steeringCurrent = max(steeringCurrent - SERVO_STEP_DEG, steeringTarget);
  }

  steeringServo.write(steeringCurrent);
}

// Configuración inicial
void setUpPinModes() {
  // Conectar el servomotor al pin especificadoA
  steeringServo.attach(steeringPin);

  // Configurar los pines del motor como salidas
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  // Detener el motor y colocar el servomotor en posición neutra (82°)
  steeringCurrent = clampSteeringAngle(STEERING_CENTER + STEERING_TRIM);
  steeringTarget = steeringCurrent;
  steeringServo.write(steeringCurrent);
}

// Función para controlar el motor
void rotateMotor(int direction) {
  if (direction > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW); // Avanzar
  } else if (direction < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH); // Retroceder
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW); // Detener
  }
}

// Configuración del sistema
void setup() {
  // Inicializar el puerto serial para depuración
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  // Configurar los pines y el hardware
  setUpPinModes();

  // Iniciar la comunicación Bluetooth con el nombre "MyBluetoothCar"
  Dabble.begin("MyBluetoothCar");

  Serial.println("Sistema listo. Conecte con la aplicación Dabble.");
}

// Bucle principal
void loop() {
  // Procesar las entradas del gamepad
  Dabble.processInput();

  // Variables para el control del motor y el servomotor
  int motorDirection = 0;
  int steering = clampSteeringAngle(STEERING_CENTER + STEERING_TRIM);

  // Control de movimiento mediante botones
  if (GamePad.isCrossPressed()) {
    motorDirection = 1; // Avanzar
  } else if (GamePad.isSquarePressed()) {
    motorDirection = -1; // Retroceder
  } else {
    motorDirection = 0; // Detener el motor
  }

  // Control de dirección mediante el eje X
  int xAxisValue = static_cast<int>(GamePad.getXaxisData());
  if (xAxisValue >= -JOYSTICK_DEADZONE && xAxisValue <= JOYSTICK_DEADZONE) {
    steering = clampSteeringAngle(STEERING_CENTER + STEERING_TRIM);
  } else {
    const int steeringCenterWithTrim = STEERING_CENTER + STEERING_TRIM;
    const int steeringLimitMin = clampSteeringAngle(steeringCenterWithTrim - STEERING_MAX_DELTA);
    const int steeringLimitMax = clampSteeringAngle(steeringCenterWithTrim + STEERING_MAX_DELTA);
    steering = map(xAxisValue, JOYSTICK_MIN, JOYSTICK_MAX, steeringLimitMin, steeringLimitMax);
    steering = clampSteeringAngle(steering);
  }

  // Actualizar el motor y el servomotor
  rotateMotor(motorDirection);
  steeringTarget = steering;
  updateSteeringSmooth();

  // Depuración: Imprimir datos en el monitor serial
  static uint32_t lastDebugMs = 0;
  if (millis() - lastDebugMs >= 200) {
    lastDebugMs = millis();
    Serial.print("Motor Direction: ");
    Serial.print(motorDirection);
    Serial.print("\tSteering Angle: ");
    Serial.println(steering);
  }
}