void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  // This just waits for a serial com
  // if it gets a "1" it writes back "1"
  // if it gets anything else it writes back "0"
  // if it doesn't get a command it just waits doing nothing
  while (Serial.available()>0)
  {
    readChar = Serial.read();

    if (readChar == "1")
    {
      Serial.write("1");
    }
    else 
    {
      Serial.write("0")
    }
  }
}
