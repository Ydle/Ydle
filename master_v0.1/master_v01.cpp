#include <wiringPi.h>
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <sched.h>
#include <sstream>

/*
Par Idleman (idleman@idleman.fr - http://blog.idleman.fr)
Licence : CC by sa
Toutes question sur le blog ou par mail, possibilité de m'envoyer des bières via le blog
*/

using namespace std;

int pin;
bool bits[32]={};
int interruptor;
int sender;
int target;
long data;

void log(string a){
  cout << a << endl;
}

void scheduler_realtime() {

struct sched_param p;
  p.__sched_priority = sched_get_priority_max(SCHED_RR);
  if( sched_setscheduler( 0, SCHED_RR, &p ) == -1 ) {
    perror("Failed to switch to realtime scheduler.");
  }
}

void scheduler_standard() {

  struct sched_param p;
  p.__sched_priority = 0;
  if( sched_setscheduler( 0, SCHED_OTHER, &p ) == -1 ) {
    perror("Failed to switch to normal scheduler.");
  }
}

//Envois d'une pulsation (passage de l'etat haut a l'etat bas)
//1 = 310µs haut puis 1340µs bas
//0 = 310µs haut puis 310µs bas
void sendBit(bool b) {
  if (b) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(310);   //275 orinally, but tweaked.
    digitalWrite(pin, LOW);
    delayMicroseconds(1340);  //1225 orinally, but tweaked.
  }
  else {
    digitalWrite(pin, HIGH);
    delayMicroseconds(310);   //275 orinally, but tweaked.
    digitalWrite(pin, LOW);
    delayMicroseconds(310);   //275 orinally, but tweaked.
  }
}

//Calcul le nombre 2^chiffre indiqué, fonction utilisé par itob pour la conversion decimal/binaire
unsigned long power2(int power){
  unsigned long integer=1;
  for (int i=0; i<power; i++){
    integer*=2;
  }
  return integer;
} 

void itob(unsigned long integer, int start, int length) {
  for (int i=0; i<length; i++){
    if ((integer / power2(length-1-i))==1){
      integer -= power2(length-1-i);
      bits[start + i]=1;
    }
    else bits[start + i]=0;
  }
}

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


//Fonction d'envois du signal //recoit en parametre un booleen dï¿½finissant l'arret ou la marche du matos (true = on, false = off) v
void transmit(int blnOn) {
 int i;
 // Sequence de verrou anoncant le dï¿½part du signal au recepeteur
 digitalWrite(pin, HIGH);
 delayMicroseconds(275); // un bit de bruit avant de commencer pour remettre les delais du recepteur a 0
 digitalWrite(pin, LOW);
 delayMicroseconds(9900); // premier verrou de 9900ï¿½s
 digitalWrite(pin, HIGH); // high again
 delayMicroseconds(275); // attente de 275ï¿½s entre les deux verrous
 digitalWrite(pin, LOW); // second verrou de 2675ï¿½s
 delayMicroseconds(2675);
 digitalWrite(pin, HIGH); // On reviens en ï¿½tat haut pour bien couper les verrous des donnï¿½es
 // Envoie du code emetteur (272946 = 1000010101000110010 en binaire)
 for(i=0; i<32;i++)
 {
   sendPair(bits[i]);
 }
 digitalWrite(pin, HIGH); // coupure donnï¿½es, verrou
 delayMicroseconds(275); // attendre 275ï¿½s
 digitalWrite(pin, LOW); // verrou 2 de 2675ï¿½s pour signaler la fermeture du signal
}

int main (int argc, char** argv)
{
  if (setuid(0))
  {
  perror("setuid");
  return 1;
  }
  scheduler_realtime();
  log("Demarrage du programme");
  pin = atoi(argv[1]);
  sender = atoi(argv[2]);
  target = atoi(argv[3]);
  data = 0;
  cout << target << endl;
  //Si on ne trouve pas la librairie wiringPI, on arrï¿½te l'execution
  if(wiringPiSetup() == -1)
  {
    log("Librairie Wiring PI introuvable, veuillez lier cette librairie...");
    return -1;
  }
  pinMode(pin, OUTPUT);
  log("Pin GPIO configure en sortie");
  itob(sender,0,7);
  itob(target,7,7);
  itob(data,14,17);
  
  cout << "signal : ";
  for(int j = 0; j < 32;j++){cout << bits[j] ; }
  cout << endl;
  log("envois du signal");
  for(;;){
    transmit(true); // envoyer ON
    delay(10); // attendre 10 ms (sinon le socket nous ignore)
  }
  log("fin du programme"); // execution terminï¿½e.
  scheduler_standard();
}
