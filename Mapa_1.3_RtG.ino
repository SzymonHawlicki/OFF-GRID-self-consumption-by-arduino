// Definicje stałych - Piny
#define PIN_INPUT       A1          // Pin gdzie podłączony jest dzielnik napięcia    EN Pin where the voltage divider is connected
#define PIN_OUTPUT_LED  LED_BUILTIN // Pin od wbudowanej diody LED                     EN Pin from the built-in LED
#define PIN_OUTPUT_D4   PD4         // Pin Roboczy                                      EN Working pin

// Definicje stałych - ADC
#define ADC_MAX    1023 // Wartość maksymalna ADC                                           EN ADC maximum value
#define ADC_AT_31V  915 // Wartość surowa ADC przy 31V DC przed dzielnikiem napięcia        EN ADC raw value at 31V DC before voltage divider

// Definicje stałych - miliwolty
#define MILLIVOLTS_ERROR     -1 // Magiczna wartość oznaczająca błąd ADC               EN A magic value indicating ADC error
#define MILLIVOLTS_ON     26900 // Napięcie włączenia w miliwoltach                     EN Turn-on voltage in millivolts
#define MILLIVOLTS_OFF    25000 // Napięcie wyłączenia w miliwoltach                     EN Turn-off voltage in millivolts

// Definicje stałych - czasy oczekiwania
#define TIME_DELAY_ON 3000 // Czas oczekiwania przed włączeniem w milisekundach             EN Time to wait before turning on in milliseconds

// Definicje stałych - UART
#define UART_SPEED 9600 // Prędkość UART w bps                                                  EN UART speed in bps                          

// Deklaracja stanów maszyny stanów
enum TState {
  STATE_START       =  0, // Inicjalizacja
  STATE_OFF_IDLE    = 10, // Oczekiwanie aż napięcie przekroczy napięcie włączenia
  STATE_OFF_WAITING = 20, // Oczekiwanie aż minie czas oczekiwania
  STATE_ON          = 30  // Załączenie wyjścia i oczekiwanie aż napięcie spadnie do napięcia wyłączenia
};

// Deklaracja zmiennych globalnych
enum TState state = STATE_START; // Stan maszyny stanów
unsigned long timer;             // Timer

// Inicjalizacja Arduino
void setup() {
  // Ustawienie pinu wyjściowego jako wyjście                                   EN Set output pin as output
  pinMode(PIN_OUTPUT_LED, OUTPUT);
  pinMode(PIN_OUTPUT_D4, OUTPUT);

  // Uruchomienie UART
  Serial.begin(UART_SPEED);
}

// Pętla główna programu
void loop() {
  // Pomiar napięcia
  int millivolts = measureMillivolts();

  Serial.print("Napięcie: ");
  Serial.println(millivolts);


  // Sprawdź czy zmierzone napięcie jest poprawne
  if ( millivolts == MILLIVOLTS_ERROR ) {
    // Wyświetl błąd, odczekaj pół sekundy i wyjdź
    Serial.println("Błąd pomiaru ADC - napięcie poza zakresem");
    delay(500);
    return;
  }

  // Wykonanie akcji w zależności od tego w jakim stanie jest program
  switch ( state ) {

    // Stan startowy: przejdź do stanu OFF_IDLE
    case STATE_START:
      state = STATE_OFF_IDLE;
      break;

    // Stan OFF_IDLE
    case STATE_OFF_IDLE:
      // Wyłącz wyjście
      setOutput(0);
      // Sprawdź napięcie ON
      if ( millivolts >= MILLIVOLTS_ON ) {
        Serial.println("Napięcie włączenia osiągnięte");
        // Resetuj timer
        timer_reset(&timer);
        // Przejdź do stanu OFF_WAITING
        state = STATE_OFF_WAITING;
      }
      break;

    // Stan OFF_WAITING
    case STATE_OFF_WAITING:
      // Wyłącz wyjście
      setOutput(0);
      // Sprawdź napięcie ON
      if ( millivolts < MILLIVOLTS_ON ) {
        Serial.println("Napięcie włączenia utracone");
        // Napięcie spadło, powrót do stanu OFF_IDLE
        state = STATE_OFF_IDLE;
      } else {
        // Napięcie nadal powyżej U_on, sprawdź czy minął czas T_on
        if ( timer_check(&timer, TIME_DELAY_ON) ) {
          Serial.println("Napięcie włączenia zatwierdzone -> ON");
          // Czas oczekiwania upłynął, przejdź do stanu ON
          state = STATE_ON;
        }
      }
      break;

    // Stan ON
    case STATE_ON:
      // Włącz wyjście
      setOutput(1);
      // Sprawdź napięcie OFF
      if ( millivolts <= MILLIVOLTS_OFF ) {
        Serial.println("Napięcie wyłączenia osiągnięte -> OFF");
        // Napięcie spadło poniżej U_off, przejdź do stanu OFF_IDLE
        state = STATE_OFF_IDLE;
      }
  }

  // Przerwa pomiędzy wywołaniami programu
  delay(200);
}

// Funkcja zwraca wartość w miliwoltach lub MILLIVOLTS_ERROR gdy napięcie jest poza zakresem    EN The function returns a value in millivolts or MILLIVOLTS_ERROR when the voltage is out of range
int measureMillivolts() {
  // Wczytaj wartość analogową
  int raw = analogRead(PIN_INPUT);
  //Kablibracja
  //Serial.print("Napięcie: ");
  //Serial.println(millivolts);

  // Sprawdź czy nie przekroczono wartośći maksymalnej ADC                                        EN Check that the maximum value of ADC is not exceeded
  if ( raw < ADC_MAX ) {
    // Wartość poprawna, zmapuj wartość surową na miliwolty
    return map(
             raw,           // Wartość wejściowa 0..(ADC_MAX-1)                                    EN Input value 0..(ADC_MAX-1)
             0, ADC_AT_31V, // Zakres wartości wejściowej 0..<wartość przy 31V>                     EN Input value range 0..<value at 31V>
             0, 31000       // Zakres wartości wyjściowej 0..31000mV                                 EN Output value range 0..31000mV
           );
  } else {
    // Przekroczono zakres ADC, zwróć magiczny symbol oznaczający błąd pomiaru                        EN // ADC range exceeded, return magic symbol for measurement error
    return MILLIVOLTS_ERROR;
  }
}

// Funkcja resetuje timer
void timer_reset(unsigned long * timer_p) {
  if ( timer_p ) {
    *timer_p = millis();
  }
}

// Funkcja sprawdza czy upłynęło 'ms' milisekund od resetu timera              EN The function checks if 'ms' milliseconds have elapsed since the timer was reset
bool timer_check(unsigned long * timer_p, unsigned long ms) {
  // Wartość przy której licznik czasu się przekręca
  unsigned long max_time = (unsigned long)(-1);

  // Aktualny czas zegara w milisekundach
  unsigned long now = millis();

  // Sprawdź czy pointer nie jest null
  if ( ! timer_p ) {
    return false;
  }

  // Sprawdź czy licznik czasu się przekręcił
  if ( *timer_p <= now ) {
    // Licznik czasu się nie przekręcił
    return ms <= (now - *timer_p);
  } else {
    // Licznik czasu się przekręcił
    return ms <= (max_time - *timer_p + now);
  }
}


void setOutput(bool on) {
  digitalWrite(PIN_OUTPUT_LED, on);
  digitalWrite(PIN_OUTPUT_D4, on);
}
// Made By Anonimus end Sz.H.
