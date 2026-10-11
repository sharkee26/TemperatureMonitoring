// Датчик температуры LM35 на аналоговом входе A0.
// Раз в секунду отправляет в Serial одно число - температуру в °C.
// Формат важен: ТОЛЬКО число и перевод строки, без подписей.

const int LM35_PIN = A0;

const int AVERAGING_SAMPLES = 20;
const int SAMPLE_PAUSE_MS   = 5;
const int REPORT_PAUSE_MS   = 900;

void setup()
{
    Serial.begin(9600);
}

void loop()
{
    long total = 0;

    for (int i = 0; i < AVERAGING_SAMPLES; i++) {
        total += analogRead(LM35_PIN);
        delay(SAMPLE_PAUSE_MS);
    }

    float averageCode = (float)total / AVERAGING_SAMPLES;

    // АЦП 10 бит, опорное 5 В; LM35 даёт 10 мВ на градус
    float volts   = averageCode * (5.0 / 1023.0);
    float celsius = volts * 100.0;

    Serial.println(celsius, 1);

    delay(REPORT_PAUSE_MS);
}
