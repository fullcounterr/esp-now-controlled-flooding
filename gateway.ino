// Prototype RTS/CTS + controlled flooding

#include <esp_now.h>
#include <WiFi.h>

esp_now_peer_info_t slave;

#define CHANNEL 3
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0

// Init struktur data packet
typedef struct  packet {
  char id[6]; // id paket alphanumeric
  uint8_t type; // tipe paket, 1 rts, 2 cts, 3 ack, 4 data
  uint8_t sender[6]; // sender mac 
  uint8_t receiver[6]; // receiver mac
  float data[2]; // 2 array data from DHT11/22 (humidity, temp)
  uint8_t time; // time needed for data sending in rts/cts
} packet;

// struct for controlled flooding table
typedef struct {
  char packetID[6];
  String senderMac;
} record_type;
record_type knownPacket[20];

// Init struct buat kirim
packet send;

// Init struct buat menerima
packet receive;

// Init bool cts for sending packets
bool cts = false;

// Init mac receiver
uint8_t receiver[] = {0x30, 0xAE, 0xA4, 0x97, 0xA4, 0xA4}; // mac gateway
  
// Init mac untuk simpan mac kita
uint8_t mac[6];

// time since clearing flooding table
long lastClear = 0;

// time since last print table
long lastPrint = 0;

// alphanum
static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

// Init ESP Now with fallback
void InitESPNow() {
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

void initBroadcastSlave() {
  // clear slave data
  memset(&slave, 0, sizeof(slave));
  for (int ii = 0; ii < 6; ++ii) {
    slave.peer_addr[ii] = (uint8_t)0xff;
  }
  slave.channel = CHANNEL; // pick a channel
  slave.encrypt = 0; // no encryption
  manageSlave();
}

bool manageSlave() {
  if (slave.channel == CHANNEL) {
    Serial.print("Slave Status: ");
    const esp_now_peer_info_t *peer = &slave;
    const uint8_t *peer_addr = slave.peer_addr;
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(peer_addr);
    if (exists) {
      // Slave already paired.
      Serial.println("Already Paired");
      return true;
    }
    else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(peer);
      if (addStatus == ESP_OK) {
        // Pair success
        Serial.println("Pair success");
        return true;
      }
      else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println("ESPNOW Not Init");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println("Peer list full");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("Out of memory");
        return false;
      }
      else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Peer Exists");
        return true;
      }
      else {
        Serial.println("Not sure what happened");
        return false;
      }
    }
  }
  else {
    Serial.println("No Slave found to process");
    return false;
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {

}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  memcpy(&receive, data, sizeof(receive)); // copy received data to struct receive
  Serial.println("==================="); 
  Serial.print("ID :");
  for(int i=0; i<6; i++) {
        Serial.print(receive.id[i]);
      }   
  Serial.println(""); 
  ////////   Cek MAC Penerima   ///////////
  char sndStr[18]; // mac sender di packet
  char rcvStr[18]; // mac receiver di packet
  char slfStr[18]; // mac sendiri untuk compare
  // sender mac to str
  snprintf(sndStr, sizeof(sndStr), "%02x:%02x:%02x:%02x:%02x:%02x",
  mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  // rcvr mac to str
  snprintf(rcvStr, sizeof(rcvStr), "%02x:%02x:%02x:%02x:%02x:%02x",
  receive.receiver[0], receive.receiver[1], receive.receiver[2], receive.receiver[3], receive.receiver[4], receive.receiver[5]);
  // self mac to str
  snprintf(slfStr, sizeof(slfStr), "%02x:%02x:%02x:%02x:%02x:%02x",
  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  /// end mac check

  // check if packet already received before
  if (checkTable(receive.id)){
    Serial.println("Received packet that have been received before, dropping packet...");
  } else { // if id is not in table then process it
    fillTable(sndStr, receive.id); // record packet id
    if (receive.type == 1){
      Serial.println("RTS packet received, sending CTS....");
      sendCts(receive);
    } 
    // accept cts?
    if (receive.type == 2 ) {
      Serial.println("Received CTS, ignoring....");
    }
    // accept ack?
    if (receive.type == 3) {
      Serial.println("Received ACK, ignoring....");
    }
    // accept data?
    if (receive.type == 4){
      Serial.println("Data packet received");
      Serial.print("From :");Serial.println(sndStr);
      Serial.println("Data received. Sending back ACK...");
      sendAck(receive);
    } 
  }
  Serial.println("==================="); 
  
}

void sendData(packet send) {
  const uint8_t *peer_addr = slave.peer_addr;
  Serial.println("==================="); 
  Serial.print("Type: "); 
  Serial.println(send.type);
  Serial.print("Sending: "); 
  Serial.println(send.data[0]);
  Serial.print("And: "); 
  Serial.println(send.data[1]);
  esp_err_t result = esp_now_send(peer_addr, (uint8_t *) &send, sizeof(send));
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("Success");
    char sndStr[18];
    snprintf(sndStr, sizeof(sndStr), "%02x:%02x:%02x:%02x:%02x:%02x",
  send.sender[0], send.sender[1], send.sender[2], send.sender[3], send.sender[4], send.sender[5]);
    fillTable(sndStr, send.id); // record packet id
  }
  else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW not Init.");
  }
  else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  }
  else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  }
  else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  }
  else {
    Serial.println("Not sure what happened");
  }
  Serial.println("==================="); 
}

// kirim cts
void sendCts(packet receive){
  float a = 0;  // 0 data for rts
  float b = 0;  // 0 data for rts
  char tmp_id[6];
  for (int i = 0; i < 6; ++i) {
        tmp_id[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  memcpy(send.id, tmp_id, sizeof(send.id)); // our mac
  send.type = 2; // cts
  float data[2] = {a, b}; 
  memcpy(send.sender, mac, sizeof(mac)); // our mac
  memcpy(send.receiver, receive.sender, sizeof(receive.sender)); // packet receiver mac
  memcpy(send.data, data, sizeof(data)); // 0 data
  send.time = 5;
  Serial.print("Size of CTS :"); Serial.println(sizeof(send));
  sendData(send); // send packet
}

// kirim ack
void sendAck(packet receive){
  float a = 0;  // 0 data for ack
  float b = 0;  // 0 data for ack
  char tmp_id[6];
  for (int i = 0; i < 6; ++i) {
        tmp_id[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  memcpy(send.id, tmp_id, sizeof(send.id)); // our mac
  send.type = 3; // ack
  float data[2] = {a, b}; 
  memcpy(send.sender, mac, sizeof(mac)); // our mac
  memcpy(send.receiver, receive.sender, sizeof(receive.sender)); // packet receiver mac
  memcpy(send.data, data, sizeof(data)); // 0 data
  send.time = 5;
  Serial.print("Size of ACK :"); Serial.println(sizeof(send));
  sendData(send); // send packet
}

// fill table with received packet id
void fillTable(char senderMac[18], char packetID[6]){
  for(int x=0; x<20; x++) {
    if(x < 19){
      if(knownPacket[x].senderMac == NULL){
        knownPacket[x].senderMac = senderMac;
        memcpy(knownPacket[x].packetID, packetID, sizeof(knownPacket[x].packetID)); 
        return;
      }
    } else {
      if(knownPacket[x].senderMac == NULL){
        knownPacket[x].senderMac = senderMac;
        memcpy(knownPacket[x].packetID, packetID, sizeof(knownPacket[x].packetID)); 
      } else {
        clearTable();
        memcpy(knownPacket[x].packetID, packetID, sizeof(knownPacket[x].packetID)); 
        knownPacket[x].senderMac = senderMac;
      }
    }
  }
}


void printTable(){
  Serial.print("MAC "); Serial.println("ID");
  for(int x=0; x<20; x++) {
    if(knownPacket[x].packetID[x] != NULL && knownPacket[x].packetID[x] != '0'){
      Serial.print(knownPacket[x].senderMac); Serial.print(" | "); 
      for(int i=0; i<6; i++) {
        Serial.print(knownPacket[x].packetID[i]);
      }
      Serial.println("");     
    }
  }
  
}

// check if packet id already received
bool checkTable(char id[6]){
  for(int x=0; x<20; x++) {
    uint8_t match = 0;
    for(int i=0; i<6; i++) {
      if (knownPacket[x].packetID[i] == id[i]){
        match++;
      }
    }
    if (match == 6){
      return true;
    }
  }
  return false;
}

// clear table if time is >10 second
void clearTable(){
  for(int x=0; x<20; x++) {
    if(x < 19){ // index bukan yg terakhir
      if(knownPacket[x].senderMac == ""){
        return;
      }
      knownPacket[x].senderMac = knownPacket[x+1].senderMac;
      memcpy(knownPacket[x].packetID, knownPacket[x+1].packetID, sizeof(knownPacket[x].packetID));
    } else {
      knownPacket[x].senderMac == NULL;
      knownPacket[x].packetID == NULL;
    }
  }
}


void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println("Setting up gateway....");
  Serial.print("Gateway MAC: "); 
  Serial.println(WiFi.macAddress());
  WiFi.macAddress(mac);
  InitESPNow();
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  srand(time(NULL));
  // add a broadcast peer
  initBroadcastSlave();
}

void loop() {
  if (millis() - lastClear > 10000) {
    clearTable();
    lastClear = millis(); // timestamp the message
  }
  if (millis() - lastPrint > 10000){
    printTable();
    lastPrint = millis();
  }
  delay(1000);
}