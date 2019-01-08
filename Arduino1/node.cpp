#include "node.h"

volatile bool Node::consensusCheck = false;
int Node::iter_consensus = 1;
int Node::iter = 1;

/*Node::Node(){
    d[0] = 0; // Duty-cycle set by the local controller
    d[1] = 0;
  };*/


Node::Node(const float _m, const float _b, int _addr, float _c, float _rho) {

  addr = _addr;
  m = _m;
  b = _b;
  c = _c;
  Lcon = L;
  rho = _rho;
  o = 0;
  iter = 1;
  L_desk = 0;
  L_ref = 0;
  /*d_avg[0] = 0; d_avg[1] = 0;
  d_best[0] = 0; d_avg[1] = 0;
  d_out[0] = 0; d_out[1] = 0;*/
  //d[0] = 0; d[1] = 0;
}


typedef struct NodeInfo {
  float m;
  float b;
  float k1;
  float k2;
  float L;
  float d1;
  float d2;
} NodeInfo;

float Node::readIlluminance() {

  const int R1 = 10000;
  const int Vcc = 5;

  float v = readVoltage();
  /*Serial.println(v);
  Serial.println(m);
  Serial.println(b);*/
  float R2 = R1 * (Vcc - v) / v;
  float illuminance = pow(10, (log10(R2) - b) / m);
  return illuminance;
}

float Node::readVoltage() {

  int analogPin = A0;
  return 5 * analogRead(analogPin) / 1023.0;
}

bool Node::setPWM(int PWM) {

  int ledPin = 11;

  if (PWM < 0 || PWM > 255)
    return false;

  analogWrite(ledPin, PWM);
  return true;
}


float Node::extIlluminance() {

  msgBroadcast('a',floatToString(d_best[addr-1])); // Broadcast my duty-cycle to other nodes
  d_ext[addr-1] = d_best[addr-1];
  while(!calc_ext_ill); // Wait until every node sends me its duty-cycle
  calc_ext_ill = false;
  
  float a = readIlluminance();
  for(int j = 0; j < ndev; j++) a -= k[j]*d_ext[j];

  if (a < 0) return 0; // this condition should not be necessary!!
  
  return a;
}

NodeInfo* Node::getNodeInfo() {

  /*NodeInfo* n;
  n->m = m;
  n->b = b;
  n->L = L_ref;

  n->k1 = k[0];
  n->k2 = k[1];
  n->d1 = d[0];
  n->d2 = d[1];

  return n;*/
}

void Node::setLux(float _L) {
  L = _L;
  //Serial.println(_L);
}

void Node::NodeSetup(){

  /*float dummy_array[MAX_LUM];
  String dummy_str_array[MAX_LUM-1];
  
  k.setStorage(dummy_array);
  d_best.setStorage(dummy_array);
  y.setStorage(dummy_array);
  d_avg.setStorage(dummy_array);
  d_test.setStorage(dummy_array);
  d_out.setStorage(dummy_array);
  z.setStorage(dummy_array);
  consensus_data.setStorage(dummy_str_array);*/
  
  d1_m = 0;
  // For consensus (initialise variables)
  for(int j = 0; j < ndev; j++){
    d1_m += pow(k[j], 2);
    //k[j] = 0;
    d_best[j] = 0;
    y[j] = 0;
    d_avg[j] = 0;
    d_test[j] = 0;
    d_out[j] = 0;
    z[j] = 0;
  }

  d1_n = d1_m - pow(k[addr-1], 2);

}

bool Node::calib() {

  if(iter == ndev) NodeSetup(); // Initialise Consensus arrays

  setPWM(0);
  float oj, lj;
  calib_flag = false;

  if (iter > ndev) return true; // All nodes have finished their calibration

  if (iter == addr) {  // It's my turn, I'm the master
    
    o = readIlluminance();
    delay(500);
    
    setPWM(255);
    delay(500);
    float x_max = readIlluminance();
    k[addr-1] = (x_max - o) / 100.0;
    Serial.println(k[addr-1]); // My own gain

    setPWM(0);
    delay(500);

    for(int j = 1; j <= ndev; j++){

      if(j == addr) continue;

      oj = readIlluminance();
      msgSync(j); // Request node j to light up its led at max power
      delay(500);
      lj = readIlluminance();
      msgSync(j);

      k[j-1] = (lj - oj) / 100.0;
      Serial.println(k[j-1]); // Coupling gain with node j
    }

    msgBroadcast('n',""); // Finish, pass to the next node
    delay(500);
    ++iter;
    calib();
  }
  else { // Not my turn, I'm a slave, wait for master's requests

    msgSync(iter); 
    setPWM(255); // Answer to the master's request

    msgSync(iter); // Master has got the info to compute the coupling gain, turn my led off
    setPWM(0);
    
    while(!calib_flag); // Wait that master finishes its calibration 
    delay(500);
    ++iter;
    calib();
  }
}

const char* Node::getConsensusData() {

  //return consensus_data.c_str();
}

int Node::msgConsensus(char id, int src_addr, String data_str) {

  /*switch (id){
    case 1:
      Serial.println("Consensus Flag -> T");
      consensus_flag = true;
      consensus_data = data_str;
      break;

    case 'L':
      d_best[src_addr-1] = atof(data_str.c_str());
      consensus_init = false;
      break;

    default:
      return -1;
      break;

    }*/
}


bool Node::checkFeasibility() {

  float tol = 0.001;

  float ill = 0;
  for(int j = 0; j < ndev; j++) ill += k[j]*d_test[j];

  max_act = false;

  if (d_test[addr-1] < 0 - tol || ill < L - o - tol)  return false;

  if (d_test[addr-1] > 100 + tol) {
    max_act = true;
    return false;
  }

  return true;
}

float Node::getCost() {

  int diff;

  float cost = c*d_test[addr-1];
  for(int j = 0; j < ndev; j++){
    Serial.println(d_test[j]);
    Serial.println(d_avg[j]);
    diff = d_test[j] - d_avg[j];
    cost += ( y[j]*diff + rho/2*diff*diff );
  }

  return cost;
}

void Node::checkSolution(int sol) {

  if (checkFeasibility()) {
    //Serial.println(sol);
    float cost = getCost();
    //Serial.println(cost);
    if (cost < cost_best) {
      //Serial.println(cost_best);
      Serial.println(sol);
      cost_best = cost;
      for(int j = 0; j < ndev; j++) d_best[j] = d_test[j];
    }
  }
}

void Node::getCopy() {

  float d_out_matrix[MAX_LUM-1][MAX_LUM]; // Each line saves one d vector
  char* d_str[MAX_LUM];

  int x;

  Serial.print("Received: ");
  for(int j = 0; j < ndev-1; j++){
    Serial.println(consensus_data[j].c_str());

    String aux_str = consensus_data[j];
    char* token = strtok((char*)aux_str.c_str(), "/");

    x = 0;
    if (token != NULL) d_str[x] = token; 
    ++x;
    while(token != NULL){
      token = strtok(NULL, "/");
      d_str[x] = token;
      ++x;
    }

    for(int k = 0; k < ndev; k++){
      d_out_matrix[j][k] = atof(d_str[k]);
      d_str[k] = NULL;
    }

    consensus_data[j] = ""; 

  }

  float sum; // Sum all d vectors received
  for(int n = 0; n < ndev; n++){
    sum = 0;
    for(int m = 0; m < ndev-1; m++){
      //Serial.println(d_out_matrix[m][n]);
      sum += d_out_matrix[m][n];
    }

    d_out[n] = sum;
  }
}

void Node::sendCopy() {

  String str = floatToString(d_best[0]);
  for(int j = 1; j < ndev; j++) str += ("/" + floatToString(d_best[j]));

  msgBroadcast('c', str);
 
  Serial.print("Sent: ");
  Serial.println(str.c_str());
}

void Node::initConsensus() {

  //k[addr-1] = 2; k[0] = 0.5;
  consensusCheck = false;
  //Lcon = L; // update lux reference
  
  for(int j = 0; j < ndev; j++){
    y[j] = 0; 
    d_out[j] = 0;
    d_avg[j] = 0;
    //d_test[j] = 0;
    z[j] = 0;
    consensus_data[j] = "";
  }

  o = 0;
  /*o = extIlluminance();
  Serial.println(o);
  delay(1000);*/
}

void Node::consensusAlgorithm() {

  all_copies = false;
  Serial.println(iter_consensus);

  //L_desk = k[0] * d_best[0] + k[1] * d_out[1] + o;

  /*Serial.println(L_desk);
  Serial.println(readIlluminance());
  Serial.println(o);*/

  /*if(abs(o - extIlluminance()) > 10 && consensusCheck){
    msgSend(2,OTHER_ADDR,"");
    delay(5);
    consensusCheck = false;
    initConsensus();
    iter_consensus = 1;
  }*/

  /*float a = abs(extIlluminance() - o);
    Serial.println(a);*/

  if (iter_consensus > 20) {
    o = extIlluminance();
    consensusCheck = true;
    return;
  }

  //unsigned long init = micros();
  float rho_inv = 1.0 / rho;
  int j = 0;
  cost_best = COST_BEST;

  for(j = 0; j < ndev; j++){

    d_best[j] = -1;

    if(j == addr-1){
      z[j] = rho*d_avg[j] - c - y[j];
      continue;
    }

    z[j] = rho*d_avg[j] - y[j];    
  }

  //Serial.println(z[0]);
  //Serial.println(z[1]);

  // Unconstrained minimum
  for(j = 0; j < ndev; j++) d_test[j] = rho_inv*z[j];
  checkSolution(1);

  // Solution in the DLB (Dimming lower bound)
  d_test[addr-1] = 0;
  checkSolution(2);

  // Solution in the DUB (Dimming Upper Bound)
  d_test[addr-1] = 100;
  checkSolution(3);

  // Solution in the ILB (Illuminance Lower Bound)
  float sum = 0;
  for(j = 0; j < ndev; j++) sum += k[j]*z[j];

  for(j = 0; j < ndev; j++) d_test[j] = rho_inv*z[j] - k[j]*(o - L + rho_inv*sum) / d1_m;
  checkSolution(4);

  // Solution in the ILB & DLB
  for(j = 0; j < ndev; j++){
    if(j == addr-1){
      d_test[j] = 0;
      continue;
    }

    d_test[j] = rho_inv*z[j] - k[j]/d1_n * (o - L - rho_inv*(k[addr-1]*z[addr-1] - sum));
  }
  checkSolution(5);

  // Solution in the ILB & DUB
  for(j = 0; j < ndev; j++){
    if(j == addr-1){
      d_test[j] = 100;
      continue;
    }

    d_test[j] = rho_inv*z[j] - (k[j]*(o - L) + 100*k[j]*k[addr-1] - rho_inv*k[j]*(k[addr-1]*z[addr-1] - sum)) / d1_n;
  }
  checkSolution(6);

  //Serial.println(d_best[0]);
  //Serial.println(d_best[1]);

  if (max_act) { // Request Illuminance value above LED actuation, power everthing at max
    for(j = 0; j < ndev; j++) d_best[j] = 100;
  }

  for(j = 1; j <= ndev; j++){
    if(j == addr) continue;
    msgSync(j);
  } 
  
  sendCopy();
  while(!all_copies); // Wait until every node sends its local copy of d vector
  all_copies = false;
  getCopy();

  //Serial.println(d_out[0]);
  //Serial.println(d_out[1]);

  for(j = 0; j < ndev; j++){   
    d_avg[j] = (d_best[j] + d_out[j]) / ndev; // Average solutions from all nodes
    y[j] += rho*(d_best[j] - d_avg[j]); // Dual update -> Update the Lagrangian Multipliers
  }

  L_ref = k[addr-1]*d_best[addr-1];
  ++iter_consensus;
  
  /*unsigned long finish = micros() - init;
    Serial.println(finish);*/

  /*Serial.println(d_avg[0]);
    Serial.println(d_avg[1]);*/

  /*if(iter_consensus == 20){
    Serial.println(L_desk);
    Serial.println(d_best[0]);
    Serial.println(d_best[1]);
    }*/

}

float Node::Windup(float u) {

  int wdp = 0;

  if (u > 100)
    wdp = 100 - u;
  else if (u < 0)
    wdp = -u;
  else
    wdp = 0;

  return wdp;
}

void Node::PID() {

  float y = readIlluminance();
  //Serial.print(y);
  //Serial.print("\t");
  //Serial.println(des_brightness);
  //Serial.print("\t");
  float e = L_ref - y;           // error in LUX
  //Serial.print(e);
  //Serial.print("\t");
  float p = k1 * e;                       // porpotional term
  float i = i_ant + k2 * (e + e_ant) + kwdp * Windup(usat);    // integal term
  //float i = i_ant + (e    + e_ant);     // integal term
  if (abs(e) < 0.5)
    p = 0;

  float u = p + i + des_brightness / k[addr-1];     // add feed-forward term

  u = constrain(u, 0, 100);
  usat = u;
  setPWM(map(u, 0, 100, 0, 255));
  //Serial.println(u);
  i_ant = i;
  e_ant = e;
  y_ant = y;
}

void Node::init_PID(float ku, float T) {

  k1 = ku * 0.45;
  ki = 1.2 / T;
  k2 = k1 * ki * T / 2;

  i_ant = 0, e_ant = 0, y_ant = 0;//, usat = 0;
}

void Node::set_Brightness() {
  if (consensusCheck) {
    des_brightness = L_ref;
    //Serial.print("VALOR VINDO DO CONSENSUS = ");
  }
  else {
    des_brightness = L;
    //Serial.print("VALOR VINDO DO USER = ");
  }
  //Serial.println(des_brightness);
}

/*void Node::button() {
  if (digitalRead(buttonPin) == HIGH)
    return;

  for (int k = 0; k < 50; k++) {

    delay(1);
    if (digitalRead(buttonPin) == HIGH)
      return;
  }

  while (digitalRead(buttonPin) == LOW);
  Occu = !Occu;
  Serial.println(Occu);
  }*/

void Node::Read_serial(char v_read) {
  /*if (v_read == 'o') {
    Occu = !Occu;
    //iter_consensus = 1;
  }

  if(v_read == 'd'){
    /*delete[] k;
    delete[] d_best;
    delete[] y;
    delete[] d_avg;
    delete[] d_best;
    delete[] d_out;

    Serial.println("Bye");

    while(1);
  }*/
}

void Node::set_occupancy() {
  //button();
  if (Occu == 1) {
    setLux(UPPB); //Set desired illuminance to 150 lux
    //Serial.println("Lux set to Upper Bound");
  }
  else {
    setLux(LOWB); //Set desired illuminance to 50 lux
    //Serial.println("Lux set to Lower Bound");
  }
}

void Node::SendInfo(int counter) {
  if (counter % 10 == 0) {
    msgBroadcast('l', floatToString(L)); //Send Illuminance
    Serial.println(L);
    msgBroadcast('p', floatToString(map(u, 0, 100, 0, 255))); //Send PWM
    Serial.println(map(u, 0, 100, 0, 255));
    msgBroadcast('e', floatToString(o)); //Send External Illuminance
    Serial.println(o);
    msgBroadcast('o', floatToString(Occu)); //Send Occupancy
    Serial.println(Occu);
    msgBroadcast('b', floatToString(LOWB)); //Send Lower Bound
    Serial.println(LOWB);
    msgBroadcast('r', floatToString(L_ref)); //Send L
    Serial.println(L_ref);
  }
}

void Node::setupint_1() {

  const int freq_des = 100; //   set freq = 100Hz

  cli();  //stop interrupts
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1 = 0; //initialize counter value to 0

  OCR1A = 2499;  // (must be <65536) (16*10^6) / (freq_des*64) - 1
  TCCR1B |= (1 << WGM12); // turn on CTC mode
  TCCR1B |= (1 << CS11) | (1 << CS10); // Set prescaler
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt

  sei(); //allow interrupts*/
}
