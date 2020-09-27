
#define OFF_DELAY 150   //задержка звучания в msec (max 255) после отпусканиия в Режиме 1 
#define TUMB_DEBOUNCING_DELAY 40 //задержка в msec (max 255) для устранения дребезга тумблера
#define KEY_DEBOUNCING_DELAY 40  //задержка в msec (max 255) для устранения дребезга клавиши
volatile uint8_t  mode = 0; //текущи режим 0-1й, 1 - 2й


void NoteOn(uint8_t note)
{
  //Для отладки выводим сообщение в сериал
  //В рабочей версии заменить на МИДИ комманды
  Serial.print("On  ");
  Serial.println(note);
}

void NoteOff(uint8_t note)
{
  //Для отладки выводим сообщение в сериал
  //В рабочей версии заменить на МИДИ комманды
  Serial.print("Off ");
  Serial.println(note);
}

//Делаем настройку пинов для клавиатуры
//и тумблера
#define TUMBLER_PIN 19 //A5
void InitInput()
{
  //Для тестировани используем Нану
  //Клавиши
  //D2-D7  (PD2-PD7)  K1-K6
  //D8-D12 (PB0-PB4)  K7-K11
  //A0-A4  (PC0-PC4) K12-K16
  //Тумблер A5 (PC5)
  for (int i = 2; i <= 19; i++)
  {
    if (i != 13) pinMode(i, INPUT_PULLUP);
  }
  pinMode(13, OUTPUT); //оставляем для моргания встроенным светодиодом
}

//возвращает 16бит с текущим состоянием клавишь
// 1 в каком-либо разряде означает нажатую клавишу с соотв номером
// не забудьте про инвертирование если используется замыкание пинов на землю
uint16_t GetKeys()
{
  //Читаем порты B, C, D целиком для ускорения
  uint8_t  d = PIND;
  uint16_t b = PINB;
  uint16_t c = PINC;
  return ~((d >> 2) | ((b & 0x1F) << 6) | ((c & 0x1F)) << 11);
}

//0 - режим 1, 1 - режим 2
uint8_t  GetTumbler()
{
  uint8_t t = ~digitalRead(TUMBLER_PIN) & 1;
  return t;
}

uint8_t DoDebouncing(uint8_t *onTimer, uint8_t *offTimer, uint8_t  *prev, uint8_t curr, uint8_t onDelay, uint8_t offDelay)
{
  if (*onTimer)
  {
    if (--(*onTimer)) return 1; //еще не истекла задержка после нажатия, считаем нажатой
  }
  if (*offTimer)
  {
    if (--(*offTimer)) return 0; //еще не истекла задержка после отпускания,считаем отпущеной
  }
  if (curr != *prev) // cocстояние изменилось
  {
    if (curr)
      *onTimer = onDelay;
    else
      *offTimer = offDelay;
    *prev = curr;
  }
  return curr;
}

uint8_t DoTumblerDebouncing(uint8_t state)
{
  static uint8_t onTimer = 0;
  static uint8_t offTimer = 0;
  static uint8_t prev = 0;

  return (DoDebouncing(&onTimer, &offTimer, &prev, state, TUMB_DEBOUNCING_DELAY, TUMB_DEBOUNCING_DELAY));
}

uint16_t DoKeyDebouncing(uint16_t current)
{
  static uint8_t onTimers[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static uint8_t offTimers[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static uint8_t prevs[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint16_t retState = 0;
  for (int i = 0; i < 16; i++) //перебираем все клавиши
  {
    retState >>= 1;
    uint8_t state = current & 1;
    current >>= 1;
    state = DoDebouncing(&(onTimers[i]), &(offTimers[i]), &(prevs[i]), state, KEY_DEBOUNCING_DELAY, KEY_DEBOUNCING_DELAY);
    retState |= state << 15;
  }
  return retState;
}

//Выключает все играющие в данный момент ноты
void StopAllPlayung(uint8_t *playingNote)
{
  for (int i = 0; i < 16; i++)
  {
    if (playingNote[i])
    {
      NoteOff(i + 1);
      playingNote[i] = 0;
    }
  }
}
//обработчик клавишь
void Do_keys()
{
  static uint8_t ModeSwtchCounter = 0;
  static uint16_t prevKeys = 0; //Предыдушее состояние клавишь
  static uint8_t  playingNote[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  uint8_t tumbler = DoTumblerDebouncing(GetTumbler());
  uint16_t keys = DoKeyDebouncing(GetKeys());
  if (ModeSwtchCounter)
  { //для сброса debouncing счетчиков при переключении режимов
    --ModeSwtchCounter;
    return;
  }
  if (tumbler != mode)
  {
    StopAllPlayung(playingNote);
    ModeSwtchCounter = max(TUMB_DEBOUNCING_DELAY, KEY_DEBOUNCING_DELAY);
    mode = tumbler;
    return;
  }
  if (!mode)
  { //В 1-м режиме (mode ==0) проверяем если есть отпущенные но играющие ноты
    uint16_t k = keys;
    for (int i = 1; i <= 16; i++) //перебираем все клавиши
    {
      if ( playingNote[i - 1] && !(k & 1))
      { //клавиша отпушена, но нота еще играет по задержке
        --playingNote[i - 1]; //уменьшаем счетчик задержки
        if (playingNote[i - 1] == 0) NoteOff(i); //выключаем по истечении задержки
      }
      k >>= 1;
    }
  }

  if (keys == prevKeys) return;
  prevKeys = keys;
  if (mode)
  { // режим 2
    for (int i = 1; i <= 16; i++) //перебираем все клавиши
    {
      uint8_t NotePressed = keys & 1;
      keys = keys >> 1; //следующая клавиша
      if (NotePressed && !playingNote[i - 1]) //нажата новая клавиша
      {
        StopAllPlayung(playingNote); //выключаем все что играло раньше
        NoteOn(i);
        playingNote[i - 1] = 1; //запоминаем последнюю ноту
      }
    }
  }
  else
  { // режим 1
    for (int i = 1; i <= 16; i++) //перебираем все клавиши
    {
      uint8_t NotePressed = keys & 1;
      keys = keys >> 1; //следующая клавиша
      if (NotePressed)
      {
        if (! playingNote[i - 1])
        { //нажата новая клавиша
          NoteOn(i);
        }
        playingNote[i - 1] = OFF_DELAY; //обновляем счетчик задержки после отпускания
      }
    }
  }
}

//Прерывания таймера раз в 1 msec
ISR (TIMER0_COMPA_vect)
{
  Do_keys();
}

void setup() {

  Serial.begin(115200);  //поменять на скорость для МИДИ после проверки
  InitInput();
  //Настраиваем таймер0 на прерывание по совпадению
  //Раз в 1 msec
  //Этот же таймер используется средой Ардуино для millis(),
  //Но TIMER0_COMPA_vect свободен.
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

void loop() {

  //Heart Beat
  digitalWrite(13, 1);
  delay(100);
  digitalWrite(13, 0);
  delay(50);
  digitalWrite(13, 1);
  delay(100);
  digitalWrite(13, 0);
  delay(800);

  ;

}
