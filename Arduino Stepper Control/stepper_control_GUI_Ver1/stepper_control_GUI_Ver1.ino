
/**************************************************************************
*  Developed by: Josh Burdine                                             *
*  Under Lab Primary Investigator: Dr. Chen-Yu Liu                        *
*  With guidance from Graduate Student: Nathan Callahan                   *
*                                                                         *
*  For use with the UCNtau project conducted at IU CEEM                   *
*  and Los Alamos National Lab (and other satellite reaserch facilities). *
*                                                                         *
*  This code can be used with proper reference to the original authors    *
*  and UCNtau project.                                                    *
*                                                                         *
*  Current Version as of Thursday June, 11th 2020 6:00PM                  *
*                                                                         *
***************************************************************************/
 
void setMicrostepRes();
void takeStep(int, int);
void scan();
void returnHome();
void updatePosition();
void executeCmd();
void recDataWithMarkers();
void parseData();
void sendCurrentPos();

/* Variables for serial communication and data handling*/
const byte numChars = 14;
char receivedChars[numChars];
char tempChars[numChars]; // parsing array
boolean newData = false;
bool stopRunFlag = false;
char cmd[1] = {'0'};
float fltVal1 = 0;
float fltVal2 = 0; 

// make sure to update the QT Code with all of these values
// in the mainwindow.h header file
#define SPEED 60.0 // Speed (v) in RPM, update QT code with this value too, like usteps
#define ANGLE 1.8 // Step angle for full step (1.8 deg for our steppers)

#define MAX_STEPS_LENGTH 4214.8215 // 59 cm
#define MAX_STEPS_WIDTH 1992.375 // 28 cm

// STEP_RES sets Microstepping Resolution
// 1 = 1/2 step; 2 = 1/4 step; 3 = 1/8 step; 
// 4 = 1/16 step; 5 = 1/32 step; 0 or >5 = Full step;
// 1/32 microstepping should be used, if this is changed
// carefully evaluate the effect on both this code
// and especially the QtCreator project code
// even though much of the arduino code auto calculates values
// as STEP_RES is changed
int STEP_RES = 5;

// Set digital pin values
int mode0 = 3; // Microstepping MODE0 pin
int mode1 = 4; // Microstepping MODE1 pin
int mode2 = 5; // Microstepping MODE2 pin
int sleepPin = 6; // SLEEP pin, set LOW for low power (delay 1ms)
int resetPin = 7; // RESET pin, set LOW for index reset
int stepPin1 = 9; // Step pin; Stepper 1
int dirPin1 = 8; // Direction pin; Stepper 1
int stepPin2 = 11; // Step pin; Stepper 2
int dirPin2 = 10; // Direction pin; Stepper 2
int homeXPin = 12; // Pin to know if X is home;
int homeYPin = 13; // Pin to know if Y is home;

// Other variable initialization
double usteps = 1.0;
double stepFreq= 0.0;
double pulseWidth = 0.0;
double currentX = 0.0; // usteps from (0,0)
double currentY = 0.0; // usteps from (0,0)
bool initHomeFlag = false;

void setup() 
{
  Serial.begin(9600);
  pinMode(sleepPin, OUTPUT);
  pinMode(resetPin, OUTPUT);
  pinMode(stepPin1, OUTPUT);
  pinMode(dirPin1, OUTPUT);
  pinMode(stepPin2, OUTPUT);
  pinMode(dirPin2, OUTPUT);
  pinMode(mode0, OUTPUT);
  pinMode(mode1, OUTPUT);
  pinMode(mode2, OUTPUT);
  pinMode(homeXPin, INPUT);
  pinMode(homeYPin, INPUT);
  
  setMicrostepRes();
  stepFreq = (SPEED * 360 * usteps) / (60 * ANGLE);
  pulseWidth = (1.0 / stepFreq) * 1000000.0; // Pulse width in microseconds

  digitalWrite(sleepPin, HIGH);
  digitalWrite(resetPin, HIGH);
  delay(10);

}

void loop() 
{
  if (initHomeFlag == false)
  {
    int i = 0;
    
    for (i = 0; i < (100 * usteps); i++)
    {
      takeStep(0, 1);
    }
    currentX = 0.0;
    
    for (i = 0; i < (100 * usteps); i++)
    {
      takeStep(0, 2);
    }
    currentY = 0.0;
    
    returnHome();
    currentX  = 0.0;
    currentY = 0.0;
    
    initHomeFlag = true;
  }

  
  digitalWrite(sleepPin, LOW);
  recDataWithMarkers();
  if (newData == true)
  {
    strcpy(tempChars, receivedChars); // temp copy for data protection
    parseData();
    executeCmd();
    newData = false;
  }
}

void recDataWithMarkers()
{
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>'; 
  char readChar = 0;

  while (Serial.available() > 0 && newData == false)
  {
    readChar = Serial.read();

    if (recvInProgress == true)
    {
      if (readChar != endMarker)
      {
        receivedChars[ndx] = readChar;
        ndx++;
      }
      else
      {
        receivedChars[ndx] = '\0'; 
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }
    else if (readChar == startMarker)
    {
      recvInProgress = true;
    }
  }
}

void parseData() 
{      // split the data into its parts

  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(tempChars,",");      // get cmd char
  strcpy(cmd, strtokIndx); // copy char to cmd

  strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
  fltVal1 = atof(strtokIndx);     // convert this part to an integer

  strtokIndx = strtok(NULL, ",");
  fltVal2 = atof(strtokIndx);     // convert this part to a float

  return;
}

void sendCurrentPos()
{
  Serial.write('<');
  Serial.print(currentX, 6);
  Serial.write('>');
  Serial.write('<');
  Serial.print(currentY, 6);
  Serial.write('>');
}

void scan()
{
  /*************************************
   * Total length (cm): 59 
   * Total width (cm): 28
   * One cm length ~= 71 15/32 steps
   * One cm width ~= 71 5/32 steps
   ************************************/
  double i = 0; // length (motor 1) counter
  double k = 0; // length 10ths counter
  double j = 0; // width (motor 2) counter
  double l = 0; // return counter
  double m = 0; // time counter

  double sampTime = fltVal2; // sampling time (s)
  double sec2ms = 0;
  double sampSpace = fltVal1; // sampling space (cm)
  double cmToStepsWid;
  double cmToStepsLen;
  double lengthUSteps;
  double widthUSteps;

  fltVal1 = 0;
  fltVal2 = 0;

  sec2ms = sampTime * 1000;
  cmToStepsWid = sampSpace * (71.0 + (5.0 / 32.0));
  cmToStepsLen = sampSpace * (71.0 + (15.0 / 32.0));
  
  lengthUSteps = cmToStepsLen * usteps;
  widthUSteps = cmToStepsWid * usteps;
  
  for (m = 0; m < 2000; m++) // delay for data collection to start
  {
    if (Serial.available())
    {
      stopRunFlag = true;
      return;
    }
    delay(1);  
  }
  
  for (m = 0; m < sec2ms; m++) // 1st sample time delay
  {
    if (Serial.available())
    {
      stopRunFlag = true;
      return;
    }
    delay(1);  
  }
  
  for (i = 0; i < floor(((MAX_STEPS_LENGTH * usteps) / lengthUSteps)); i++)
  {
    for (j = 0; j < floor(((MAX_STEPS_WIDTH * usteps) / widthUSteps)); j++) // Go out width
    { 
      for (k = 0; k < widthUSteps; k++)
      {
        if (Serial.available())
        {
          stopRunFlag = true;
          return;
        }
        takeStep(0, 2);
      }
      
      for (m = 0; m < sec2ms; m++)
      {
        if (Serial.available())
        {
          stopRunFlag = true;
          return;
        }
        delay(1);  
      }  
    }
      
    for (l = 0; l < floor(((MAX_STEPS_WIDTH * usteps) / widthUSteps)) * widthUSteps; l++) // go back in width
    {
      if (Serial.available())
      {
        stopRunFlag = true;
        return;
      }
      takeStep(1, 2);
    }

    for (j = 0; j < lengthUSteps; j++) // advance length
    {
      if (Serial.available())
      {
        stopRunFlag = true;
        return;
      }
      takeStep(0, 1);
    }
    
    for (m = 0; m < sec2ms; m++)
    {
      if (Serial.available())
      {
        stopRunFlag = true;
        return;
      }
      delay(1);  
    }      
  }

  for (j = 0; j < floor(((MAX_STEPS_WIDTH * usteps) / widthUSteps)); j++)
  {
    for (k = 0; k < widthUSteps; k++)
    {
      if (Serial.available())
      {
          return;
      }
      takeStep(0, 2);
    }
    
    for (m = 0; m < sec2ms; m++)
    {
      if (Serial.available())
      {
        stopRunFlag = true;
        return;
      }
      delay(1);  
    } 
  }

  Serial.write('9'); // stop data aqcuisition cmd
  returnHome();
  return ;
}

// 0 means forward, !0 means back
void takeStep(int dir, int motor)
{
  if (motor == 1) // Stepper 1
  {
    if (dir == 0)
    {
      digitalWrite(dirPin1, LOW); 
      currentX++;
    }
    else
    {
      digitalWrite(dirPin1, HIGH);  
      currentX--;
    }
    delay(0.005);
    
    digitalWrite(stepPin1, HIGH);
    delayMicroseconds(pulseWidth / 2.0);
    digitalWrite(stepPin1, LOW);
    delayMicroseconds(pulseWidth / 2.0);
  
  } 
  else // Stepper 2
  {
    if (dir == 0)
    {
      digitalWrite(dirPin2, LOW); 
      currentY++;
    }
    else
    {
      digitalWrite(dirPin2, HIGH);  
      currentY--;
    }
    delay(0.005);

    digitalWrite(stepPin2, HIGH);
    delayMicroseconds(pulseWidth / 2.0);  
    digitalWrite(stepPin2, LOW);
    delayMicroseconds(pulseWidth / 2.0);
  }
  
  return ;
}

void setMicrostepRes() 
{
  switch(STEP_RES) 
  {
    case 1: // 1/2 step
      digitalWrite(mode0, HIGH);
      digitalWrite(mode1, LOW);
      digitalWrite(mode2, LOW);
      usteps = 2.0;
      break;
    case 2: // 1/4 step
      digitalWrite(mode0, LOW);
      digitalWrite(mode1, HIGH);
      digitalWrite(mode2, LOW);
      usteps = 4.0;
      break;
    case 3: // 1/8 step
      digitalWrite(mode0, HIGH);
      digitalWrite(mode1, HIGH);
      digitalWrite(mode2, LOW);
      usteps = 8.0;
      break;
    case 4: // 1/16 step
      digitalWrite(mode0, LOW);
      digitalWrite(mode1, LOW);
      digitalWrite(mode2, HIGH);
      usteps = 16.0;
      break;
    case 5: // 1/32 step
      digitalWrite(mode0, HIGH);
      digitalWrite(mode1, LOW);
      digitalWrite(mode2, HIGH);
      usteps = 32.0;
      break;
    default: // Full step
      digitalWrite(mode0, LOW);
      digitalWrite(mode1, LOW);
      digitalWrite(mode2, LOW);
      usteps = 1.0;
      break;
  }
  
  return ;
}

void returnHome()
{
  while (digitalRead(homeYPin) == LOW)
  {
    takeStep(1, 2);
  }
  while (digitalRead(homeXPin) == LOW)
  {
    takeStep(1, 1);
  }

  currentX = 0.000;
  currentY = 0.000;
  
  return ;
}

void updatePosition()
{
  double xToGo = 0; // number of steps to be made by Motor 1
  double yToGo = 0; // number of steps to be made by Motor 2
  double xRec = 0; // new desired position for x
  double yRec = 0; // new desired position for yRec
  double xUSteps = 0;
  double yUSteps = 0;
  double cmToStepsWid;
  double cmToStepsLen;
  double i = 0; // steps to go counter

  xRec = fltVal1;
  yRec = fltVal2;
  
  cmToStepsWid = yRec * (71.0 + (5.0 / 32.0));
  cmToStepsLen = xRec * (71.0 + (15.0 / 32.0));

  xUSteps = cmToStepsLen * usteps;
  yUSteps = cmToStepsWid * usteps;

  xToGo = round(xUSteps - currentX);
  yToGo = round(yUSteps - currentY);

  if (xToGo > 0 & currentX < (MAX_STEPS_LENGTH * usteps)) // advance forward
  {
    for (i = 0; i < xToGo; i++)
    {
      if (Serial.available())
      {
          return;
      }
      takeStep(0, 1);
    }
  }
  else // go back
  {
    if (currentX > 0)
    {
      for (i = 0; i < abs(xToGo); i++)
      {
        if (Serial.available())
        {
          return;
        }
        takeStep(1, 1);
      }  
    }
  }

  if (yToGo > 0 & currentY < (MAX_STEPS_WIDTH * usteps)) // advance forward
  {
    for (i = 0; i < yToGo; i++)
    {
      if (Serial.available())
      {
          return;
      }
      takeStep(0, 2);
    }
  }
  else // go back
  {
    if (currentY > 0)
    {
      for (i = 0; i < abs(yToGo); i++)
      {
        if (Serial.available())
        {
          return;
        }
        takeStep(1, 2);
      }  
    }
  }

  fltVal1 = 0;
  fltVal2 = 0;
  
  return ;
}

void executeCmd()
{ 
  int i = 0;
  
  digitalWrite(sleepPin, HIGH);
  
  switch(cmd[0])
  {
    case '1': // X Back
      if(currentX > 0)
      {
        i = 0;
        for (i = 0; i < (1 * usteps); i++)
        {
          takeStep(1, 1);  
        }
      }
      break;
    case '2': // Y Back
      if(currentY > 0)
      {
        i = 0;
        for (i = 0; i < (1 * usteps); i++)
        {
          takeStep(1, 2);  
        }
      }
      break;
    case '3': // X Forward
      if(currentX < MAX_STEPS_LENGTH * usteps)
      {
        i = 0;
        for (i = 0; i < (1 * usteps); i++)
        {
          takeStep(0, 1);
        }
      }
      break;
    case '4': // Y Forward
      if(currentY < MAX_STEPS_WIDTH * usteps)
      {
        i = 0;
        for (i = 0; i < (1 * usteps); i++)
        {
          takeStep(0, 2);
        }
      }
      break;
    case '5': // Run Scan
      if (currentX == 0 & currentY == 0)
      {
        scan();
      }
      break;
    case '6': // Stop
      if (stopRunFlag == false)
      {
        Serial.write('0');
        delay(800);  
      }
      else
      {
        stopRunFlag = false;
        Serial.write('9');
        delay(800);  
      }
      sendCurrentPos();
      break;
    case '7': // Update Position
      updatePosition();
      break;
    case '8': // Return Home
      returnHome();
      break;
    default:
      cmd[0] = '0';
  }
  cmd[0] = '0';  

  return ;
}
