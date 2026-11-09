#include <Arduino.h>        // Incluye las funciones básicas de Arduino
#include <DabbleESP32.h>
#include <ESP32Servo.h>

// Definición de pines
#define steeringPin 13      // Pin del servomotor
#define IN1 19               // Pin IN1 del motor
#define IN2 18              // Pin IN2 del motor

// El servo se controla directamente en microsegundos (más preciso que grados).
// Servo S0017M: centro = 1520 us.
constexpr int STEERING_CENTER_US = 1520;
constexpr int STEERING_TRIM_US = 0;         // Ajuste fino del centro (+/- unos pocos us)
// Recorrido máximo desde el centro, por lado (máx. 480 por los límites de abajo).
// Si un lado fuerza, bájalo; si le falta giro, súbelo.
constexpr int STEERING_LEFT_DELTA_US = 250;
constexpr int STEERING_RIGHT_DELTA_US = 450;
constexpr int SERVO_MIN_US = 1000;
constexpr int SERVO_MAX_US = 2000;
constexpr bool STEERING_INVERT = true;    // true si el giro sale al revés

constexpr int JOYSTICK_MIN = -7;
constexpr int JOYSTICK_MAX = 7;
constexpr int JOYSTICK_DEADZONE = 1;
constexpr uint32_t SERVO_STEP_INTERVAL_MS = 15;
constexpr int SERVO_STEP_US = 15;

int steeringCurrent = STEERING_CENTER_US + STEERING_TRIM_US;
int steeringTarget = STEERING_CENTER_US + STEERING_TRIM_US;
uint32_t lastServoStepMs = 0;

// Objeto para el servomotor
Servo steeringServo;

// Convierte el eje X del joystick (negativo = izquierda) en microsegundos.
int steeringFromJoystick(int x) {
  const int center = STEERING_CENTER_US + STEERING_TRIM_US;
  const int magnitude = min(abs(x), JOYSTICK_MAX);
  const bool isLeft = x < 0;
  const int delta = isLeft ? STEERING_LEFT_DELTA_US : STEERING_RIGHT_DELTA_US;
  const int offset = magnitude * delta / JOYSTICK_MAX;
  // Con STEERING_INVERT, la izquierda corresponde a más microsegundos.
  const int leftSign = STEERING_INVERT ? 1 : -1;
  const int sign = isLeft ? leftSign : -leftSign;
  return constrain(center + sign * offset, SERVO_MIN_US, SERVO_MAX_US);
}

void updateSteeringSmooth() {
  uint32_t now = millis();
  if (now - lastServoStepMs < SERVO_STEP_INTERVAL_MS) {
    return;
  }

  lastServoStepMs = now;

  if (steeringCurrent < steeringTarget) {
    steeringCurrent = min(steeringCurrent + SERVO_STEP_US, steeringTarget);
  } else if (steeringCurrent > steeringTarget) {
    steeringCurrent = max(steeringCurrent - SERVO_STEP_US, steeringTarget);
  }

  steeringServo.writeMicroseconds(steeringCurrent);
}

// Configuración inicial
void setUpPinModes() {
  // Reservar temporizadores PWM para el servo y fijar 50 Hz
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  steeringServo.setPeriodHertz(50);
  steeringServo.attach(steeringPin, SERVO_MIN_US, SERVO_MAX_US);

  // Configurar los pines del motor como salidas
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  // Dirección recta al encender
  steeringCurrent = STEERING_CENTER_US + STEERING_TRIM_US;
  steeringTarget = steeringCurrent;
  steeringServo.writeMicroseconds(steeringCurrent);
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

  int motorDirection = 0;
  const int center = STEERING_CENTER_US + STEERING_TRIM_US;
  int steering = center;

  // Control de movimiento mediante botones
  if (GamePad.isCrossPressed()) {
    motorDirection = 1; // Avanzar
  } else if (GamePad.isSquarePressed()) {
    motorDirection = -1; // Retroceder
  }

  // Control de dirección mediante el eje X del joystick
  int xAxisValue = static_cast<int>(GamePad.getXaxisData());
  if (xAxisValue >= -JOYSTICK_DEADZONE && xAxisValue <= JOYSTICK_DEADZONE) {
    steering = center;  // Joystick suelto: vuelve al centro
  } else {
    steering = steeringFromJoystick(xAxisValue);
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
    Serial.print("\tX: ");
    Serial.print(xAxisValue);
    Serial.print("\tSteering (us): ");
    Serial.println(steering);
  }
}
