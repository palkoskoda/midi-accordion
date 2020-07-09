
String commandbuffer;
String valuebuffer;
String namebuffer;
bool valueflag;
bool nameflag;

String commands[17] =  
{
  "set"
};

void serialRefresh()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\n')
    {
      spracovanie();
      valuebuffer = "";
      namebuffer = "";
      commandbuffer = "";
      valueflag = 0;
    }
    else if (c == ' ')
     { valueflag = 0;
      nameflag = 1;}
    else if (c == '=')
      {valueflag = 1;
      nameflag = 0;}
    else if (valueflag == 1)
      valuebuffer += c;
    else if (nameflag == 1)
      namebuffer += c;

    else
      commandbuffer += c;
  }
}

void spracovanie()
{
  if (commandbuffer == "set")
  {
    int cislo =namebuffer.toInt();
    if (valuebuffer.length() == 0)
    {
      // vypis
      Serial.print("(len zobrazenie) " + namebuffer + "=");
      Serial.println(notePitches[cislo]); 
    }
    else
    { notePitches[cislo]=valuebuffer.toInt();
      EEPROM.put((cislo * 4), notePitches[cislo]);
      int precitane; 
      EEPROM.get(cislo*4, precitane);
      Serial.print("(nastavené) " + String(cislo) + "=" + notePitches[cislo] + ", ");
      Serial.println(precitane); }
     
 
    }
  else if (commandbuffer == "clear"){
    for (int i = 0 ; i < EEPROM.length() ; i++) { 
    EEPROM.write(i, 0);
  }}
  else 
  {
    Serial.print("Nesprávny príkaz:"); 
    Serial.println(commandbuffer);
  }
}
