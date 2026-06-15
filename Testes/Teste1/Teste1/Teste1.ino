#include <ESP32Servo.h>

Servo esc;

#define ESC_PIN 4

const int VELOCIDADE_MINIMA = 1000;
const int VELOCIDADE_MAXIMA = 2000;

unsigned long tempoAnterior = 0;
const long intervalo = 3000;
bool motorNaVelocidadeMaxima = false;

void setup() {
  Serial.begin(115200);

  esc.attach(ESC_PIN, 1000, 2000);

  esc.writeMicroseconds(VELOCIDADE_MINIMA);
  delay(5000); 
}

void loop() {
  unsigned long tempoAtual = millis();

  if (tempoAtual - tempoAnterior >= intervalo) {
    tempoAnterior = tempoAtual;

    if (!motorNaVelocidadeMaxima) {
      esc.writeMicroseconds(VELOCIDADE_MAXIMA);
      motorNaVelocidadeMaxima = true;
    } else {
      esc.writeMicroseconds(VELOCIDADE_MINIMA);
      motorNaVelocidadeMaxima = false;
    }
  }
}