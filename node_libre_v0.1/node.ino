/**
* YdleNode v0.1
* Première version du node Ydle pour un capteur DS18B20
*
* @author : Yaug / Manuel Esteban
* @date : 25/7/2013
**/
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"

#define DS18B20 0x28     // Adresse 1-Wire du DS18B20

int pinRx = 12;
int pinTx = 10;
int pinNode = 9;
int pinLed = 13;
int transmissionType = 1; // Temperature
int timer = 4000;

boolean reset = true;
// Array containing the bits to send
bool bit2[32]={};       

// On crée une instance de la classe oneWire pour communiquer avec le materiel on wire (dont le capteur ds18b20)
OneWire oneWire(pinNode);
//On passe la reference onewire Ã  la classe DallasTemperature qui vas nous permettre de relever la temperature simplement
DallasTemperature sensors(&oneWire);

struct config_t
{
  long sender;
  int receptor;
  int type;
} signal;

struct signal_t
{
  long sender;
  int receptor;
  boolean isSignal;
  boolean state;
} receivedSignal;

void setup()
{
  pinMode(pinRx, INPUT);
  pinMode(pinLed, OUTPUT);
  //On initialise le capteur de temperature
  sensors.begin();
  Serial.begin(9600);
}

void loop()
{
  if(reset){
    //Ecoute des signaux
    listenSignal();
    if(receivedSignal.isSignal){
      checkEEProm();
    }
  } else {
    Serial.println(signal.sender);
    Serial.println(signal.receptor);
    float temp;
    boolean res = true;
    //Lancement de la commande de rÃ©cuperation de la temperature
    sensors.requestTemperatures();
    //Affichage de la temparature dans les logs
    float currentTemperature = sensors.getTempCByIndex(0);
    Serial.println(currentTemperature); ;  
    //Conversion de la temperature en binaire et stockage sur 7 bits dans le tableau bit2
    itob(signal.receptor, 0, 7);
    itob(signal.sender, 7, 7);
    itob(transmissionType,14, 3);
    int temperature = currentTemperature * 100;
    if(temperature > 0){ itob(1,17,1); }
    else { itob(0,17,1); }
    Serial.println(temperature);
    itob(temperature,18,14);
    //Serial.println("message radio :");
    for(int i = 0; i < 32; i++){
      Serial.print(bit2[i]);
    }
    transmit();
    delay(timer);
  }
  Serial.println("");
}

//Envoie d'une paire de pulsation radio qui definissent 1 bit rÃ©el : 0 =01 et 1 =10
//c'est le codage de manchester qui necessite ce petit bouzin, ceci permet entre autres de dissocier les donnÃ©es des parasites
void sendPair(bool b) {
 if(b)
 {
   sendBit(true);
   sendBit(false);
 }
 else
 {
   sendBit(false);
   sendBit(true);
 }
}

//Envois d'une pulsation (passage de l'etat haut a l'etat bas)
//1 = 310Âµs haut puis 1340Âµs bas
//0 = 310Âµs haut puis 310Âµs bas
void sendBit(bool b) {
 if (b) {
   digitalWrite(pinTx, HIGH);
   delayMicroseconds(310);   //275 orinally, but tweaked.
   digitalWrite(pinTx, LOW);
   delayMicroseconds(1340);  //1225 orinally, but tweaked.
 }
 else {
   digitalWrite(pinTx, HIGH);
   delayMicroseconds(310);   //275 orinally, but tweaked.
   digitalWrite(pinTx, LOW);
   delayMicroseconds(310);   //275 orinally, but tweaked.
 }
}

/** 
 * Transmit data
 * @param boolean  positive temperature negative ou positive
 * @param int temperature temperature en centiÃ¨mes de degrÃ©s 2015 = 20,15Â°C
 **/
void transmit()
{
 int i;
 
 digitalWrite(pinLed, HIGH);
 // Sequence de verrou anoncant le dÃ©part du signal au recepeteur
 digitalWrite(pinTx, HIGH);
 delayMicroseconds(275);     // un bit de bruit avant de commencer pour remettre les delais du recepteur a 0
 digitalWrite(pinTx, LOW);
 delayMicroseconds(9900);     // premier verrou de 9900Âµs
 digitalWrite(pinTx, HIGH);   // high again
 delayMicroseconds(275);      // attente de 275Âµs entre les deux verrous
 digitalWrite(pinTx, LOW);    // second verrou de 2675Âµs
 delayMicroseconds(2500);
 digitalWrite(pinTx, HIGH);  // On reviens en Ã©tat haut pour bien couper les verrous des donnÃ©es
 
 
 // Envoie du code 
 for(i=0; i<32;i++)
 {
   sendPair(bit2[i]);
 }
 
 // Sequence de verrou finale
 digitalWrite(pinTx, HIGH);
 delayMicroseconds(275);     // un bit de bruit avant de commencer pour remettre les delais du recepteur a 0
 digitalWrite(pinTx, LOW);
 digitalWrite(pinLed, LOW);
}


void checkSignal()
{
  //Comparaison signal reçu et signal de référence
  if(receivedSignal.sender==signal.sender && receivedSignal.receptor==signal.receptor){
    Serial.println("Le signal correspond");
  }
}

void checkEEProm()
{
  //Récuperation du signal de référence en mémoire interne
  EEPROM_readAnything(0, signal);
  //Si aucun signal de référence, le premier signal reçu servira de reference en mémoire interne
  if(reset || signal.sender < 0){
    reset = false;
    EEPROM_writeAnything(0, receivedSignal);
    Serial.println("Aucun signal de référence dans la mémoire interne, enregistrement du signal reçu comme signal de réference");
    EEPROM_readAnything(0, signal);
  }
}

void listenSignal()
{
  receivedSignal.sender = 0;
  receivedSignal.receptor = 0;
  receivedSignal.isSignal = false;
  int i = 0;
  unsigned long t = 0;

  byte prevBit = 0;
  byte bit = 0;

  unsigned long sender = 0;
  unsigned long receptor = 0;
  bool group = false;
  bool on = false;
  unsigned int recipient = 0;

  // latch 1
  while((t < 9480 || t > 10350))
  {
    t = pulseIn(pinRx, LOW, 1000000);
  }

  // latch 2
  while(t < 2550 || t > 2700)
  {
    t = pulseIn(pinRx, LOW, 1000000);
  }

  // data
  while(i < 64)
  {
    t = pulseIn(pinRx, LOW, 1000000);

    if(t > 200 && t < 400)
    {
      bit = 0;
    }
    else if(t > 1000 && t < 1400)
    {
      bit = 1;
    }
    else
    {
      Serial.println("Debug t");
      Serial.println(t);
      i = 0;
      break;
    }

    if(i % 2 == 1)
    {
      if((prevBit ^ bit) == 0)
      {
        // must be either 01 or 10, cannot be 00 or 11
        i = 0;
        break;
      }

      if(i < 14)
      {
        // first 14 data bits
        sender <<= 1;
        sender |= prevBit;
      }
      else if(i < 28)
      {
        // first 14 data bits
        receptor <<= 1;
        receptor |= prevBit;
      } 
      else if(i == 55)
      {
        // 27th data bit
        on = prevBit;
      }
      else
      {
        // last 4 data bits
        recipient <<= 1;
        recipient |= prevBit;
      }
    }

    prevBit = bit;
    ++i;
  }
  
  Serial.println("sender : ");
  Serial.println(sender);
  Serial.println("receptor : ");
  Serial.println(receptor);

  // interpret message
  if(i > 0)
  {
    receivedSignal.sender = sender;
    receivedSignal.receptor = receptor;
    receivedSignal.isSignal = true;
    if(on)
    {
      receivedSignal.state = true;
    }else{
      receivedSignal.state = false;
    }
    printResult(sender, group, on, recipient);
  }
}
 
void printResult(unsigned long sender, bool group, bool on, unsigned int recipient)
{
  Serial.print("sender ");
  Serial.println(sender);
 
  if(group)
  {
    Serial.println("group command");
  }
  else
  {
    Serial.println("no group");
  }
 
  if(on)
  {
    Serial.println("on");
  } else {
    Serial.println("off");
  }
 
  Serial.print("recipient ");
  Serial.println(recipient);
 
  Serial.println();
}


//Calcule 2^"power"
unsigned long power2(int power){    
 unsigned long integer=1;          
 for (int i=0; i<power; i++){      
   integer*=2;
 }
 return integer;
}

//fonction de conversion d'un nombre dÃ©cimal "integer" en binaire sur "length" bits et stockage dans le tableau bit2
void itob(unsigned long integer, int start, int length)
{
  //needs bit2[length]
  // Convert long device code into binary (stores in global bit2 array.)
 for (int i=0; i<length; i++){
   if ((integer / power2(length-1-i))==1){
     integer-=power2(length-1-i);
     bit2[start + i]=1;
   }
   else bit2[start + i]=0;
 }
}