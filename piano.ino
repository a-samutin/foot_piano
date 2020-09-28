
#define OFF_DELAY 150   //задержка звучания в msec (max 255) после отпусканиия в Режиме 1 
#define HOLD_DEBOUNCING_DELAY 40  //задержка в msec (max 255) для устранения дребезга педли HOLD
#define KEY_DEBOUNCING_DELAY 40  //задержка в msec (max 255) для устранения дребезга клавиши
volatile uint8_t  mode = 0; //текущи режим 0-1й, 1 - 2й
//Здесь записываем назначение пинов на клавиши. Для аналоговых использовать номера. Вместо "А0"-14, вместо "A1"-15 итд
volatile uint8_t keyPins[] = {4, 3, 5, 2, 6, 7, 15, 8, 14, 9, 17, 16};
#define S_L 11  // светодиод питание, программирование
#define S_H 13//10  // светодиод холд
#define B_H 19  // Hold пин (A5). Переключения режимов, и программирование при долгом удержании  
#define PROG_DELAY 1500 //Время удержания Hold в mSec для входа в программирование
#define SM_B  59  //Базовое смещение
volatile uint8_t sm ; //Текущее смещение
void NoteOn(uint8_t note)
{
  //Для отладки выводим сообщени в сериал
  //В рабочей версии заменить на МИДИ комманды
//  Serial.print("On  ");
//  Serial.println(note);
  Serial.write(0x90);
  Serial.write(note + sm);
  Serial.write(70);
}

void NoteOff(uint8_t note)
{
  //Для отладки выводим сообщение в сериал
  //В рабочей версии заменить на МИДИ комманды
//  Serial.print("Off ");
 // Serial.println(note);
  Serial.write(0x80);
  Serial.write(note + sm);
  Serial.write(0);
}

void  InstrumenChange(uint8_t instrument)
{
  Serial.write(0xC0);
  Serial.write(instrument);
}

void inline PowerLED(uint8_t state)
{
  digitalWrite(S_L, state);
}

void inline HoldLed(uint8_t state)
{
  digitalWrite(S_H, state);
}

//Делаем настройку пинов для клавиатуры
//и педали Hold
#define NumberOfKeys (sizeof(keyPins))
#define PROG_MODE 0xa5

void InitInput()
{
  pinMode(B_H, INPUT_PULLUP);
  for (byte i = 0; i < NumberOfKeys ; i++)
  {
    pinMode(keyPins[i], INPUT_PULLUP);
  }
  pinMode(13, OUTPUT); //оставляем для моргания встроенным светодиодом
  pinMode(S_L, OUTPUT);
  pinMode(S_H, OUTPUT);
}

//возвращает 16бит с текущим состоянием клавишь
// 1 в каком-либо разряде означает нажатую клавишу с соотв номером
// не забудьте про инвертирование если используется замыкание пинов на землю
uint16_t GetKeys()
{
  /*
    //Читаем порты B, C, D целиком для ускорения
    uint8_t  d = PIND;
    uint16_t b = PINB;
    uint16_t c = PINC;
    return ~((d >> 2) | ((b & 0x1F) << 6) | ((c & 0x1F)) << 11);
  */
  uint16_t ret = 0;
  uint16_t st;
  for (int i = NumberOfKeys - 1; i >= 0 ; i--)
  {
    ret <<= 1;
    st = digitalRead(keyPins[i]); 
    ret |= (~st) & 1;
  }
  return ret;
}

//0 - режим 1, 1 - режим 2
uint8_t  GetHoldPedal()
{
  uint8_t t = ~digitalRead(B_H) & 1;
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

uint8_t DoHoldPedalDebouncing(uint8_t state)
{
  static uint8_t onTimer = 0;
  static uint8_t offTimer = 0;
  static uint8_t prev = 0;

  return (DoDebouncing(&onTimer, &offTimer, &prev, state, HOLD_DEBOUNCING_DELAY, HOLD_DEBOUNCING_DELAY));
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

uint8_t GetHoldMode ()
{
  static uint8_t currentMode = 0;
  static uint16_t holdCnt = 0;
  uint8_t state = DoHoldPedalDebouncing(GetHoldPedal());

  if (state)
  { //педаль нажата
    ++holdCnt;
    return currentMode;
  }
  //педать отпущена
  if (holdCnt)
  {
 //   if (holdCnt < PROG_DELAY) //ToDo раскоментировать когда будет сделано прогр
    { // короткое нажатие. Переключаем режимы    
      currentMode = !currentMode;
      HoldLed(currentMode);
      holdCnt = 0;     
      return currentMode;
    }
    //долгое нажатие
    holdCnt = 0;  
    return PROG_MODE;
  }
  return currentMode;
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

void DoProgramming(uint16_t keys)
{
  switch (keys){
    case 0x001:  //bC
      sm=SM_B-36;
      break;   
    case 0x004:  //bD
      sm=SM_B-24;
      break;
    case 0x010: //bE
      sm=SM_B-12;
      break;
    case 0x020: //bF
      sm=SM_B;
      break;
    case 0x080: //bG  
      sm=SM_B+12;
      break;
    case 0x200: //bA
      sm=SM_B+24;
      break;
    case 0x800: //bB
      sm=SM_B+36;
      break;
    // блок пресетов
    case 0x002: //bCd
      sm=SM_B-12;
      InstrumenChange(19); //орган Church
      break;
    case 0x008: //bDd
      sm=SM_B-12;
      InstrumenChange(18); //draw bar Organ
      break;  
    case 0x040: //bFd
      sm=SM_B-12;
      InstrumenChange(32); // бас акус
      break; 
    case 0x100: //bGd
      sm=SM_B-12;
      InstrumenChange(42); //виолончель
      break;  
    case 0x400: //bAd
      sm=SM_B-12;
      InstrumenChange(71); //кларнет
      break;          
  }
}

//обработчик клавишь
void Do_keys()
{
  static uint8_t ModeSwtchCounter = 0;
  static uint16_t prevKeys = 0; //Предыдушее состояние клавишь
  static uint8_t  playingNote[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static uint8_t  programMode = 0;
  static uint16_t progLEDcnt = 0 ;

  uint8_t newMode = GetHoldMode();
  uint16_t keys = DoKeyDebouncing(GetKeys());
  if (ModeSwtchCounter)
  { //для сброса debouncing счетчиков при переключении режимов
    --ModeSwtchCounter;
    return;
  }

  if (programMode)
  {
    ++progLEDcnt;
    progLEDcnt &=0x1F; //период мигания 512 мСек
    // Mигаем светодиодом power
    if (progLEDcnt < 350) 
      PowerLED(LOW);
    else{
      PowerLED(HIGH);
    }   
    if (keys) {
      DoProgramming(keys);
      programMode = 0; 
      PowerLED(HIGH);     
    }
    return;
  }
  if (newMode != mode)
  {
    StopAllPlayung(playingNote);
    ModeSwtchCounter = max(HOLD_DEBOUNCING_DELAY, KEY_DEBOUNCING_DELAY);
    if (newMode == PROG_MODE)
      programMode = 1;
    else
      mode = newMode;
    return;
  }
  if (mode == 0)
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
  if (mode == 1)
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
    for (byte i = 1; i <= 16; i++) //перебираем все клавиши
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

  Serial.begin(31250);  //поменять на скорость для МИДИ после проверки
  InitInput();
  //Настраиваем таймер0 на прерывание по совпадению
  //Раз в 1 msec
  //Этот же таймер используется средой Ардуино для millis(),
  //Но TIMER0_COMPA_vect свободен.
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);

  sm = SM_B - 12;  // рок орган по умолчанию
  InstrumenChange(19);
  PowerLED(HIGH); // горит как питание


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
