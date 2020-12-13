// Prototype RTS/CTS + controlled flooding

#include <esp_now.h>
#include <WiFi.h>

esp_now_peer_info_t slave;

#define CHANNEL 3
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0

// Init struktur data packet
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

// RTS tambah field waktu untuk pengiriman data

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

// time since last sending data
long lastSend = 0;

// time since last print table
long lastPrint = 0;

// time since node is being quiet by cts packet
long lastCts = 0;

// alphanum
static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {

  }

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  memcpy(&receive, data, sizeof(receive)); // copy received data to struct receive
  Serial.println("==================="); 
  Serial.print("Received ID :");
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


  
  // accept cts?
  if (checkTable(receive.id)){
    Serial.println("Received packet that have been received before, dropping packet...");
  } else {
    fillTable(sndStr, receive.id); // record packet id
    if (receive.type == 1){
      Serial.println("RTS packet received, re-broadcasting....");
      sendData(receive);
    } 
    if (receive.type == 2 ) {
      Serial.println("CTS packet received, checking mac ....");
      if (strcmp(slfStr, rcvStr) == 0){
        Serial.println("MAC is correct, sending current data ....");
        sendCurrent();
      } else {
        Serial.println("Mismatched MAC, respecting time field and rebroadcasting....");
        Serial.println(lastCts);
        lastCts = millis(); // current time + time field for this node to be quiet for otehr nodes
        Serial.println(lastCts);
        Serial.println(receive.time);
        lastCts = lastCts + (receive.time * 1000);
        Serial.println(lastCts);
        sendData(receive);
      }
    }
    // accept ack?
    if (receive.type == 3) {
      if (strcmp(slfStr, rcvStr) == 0){
        Serial.println("ACK received. RTS/CTS and data sent successfully!");
      } else {
        Serial.println("ACK received but mismatched MAC, re-broadcasting...");
        sendData(receive);
      }
    }
    // accept data?
    if (receive.type == 4){
      Serial.println("Data packet received, re-broadcasting....");
      sendData(receive);
    } 
  }
}

void sendData(packet send) {
  const uint8_t *peer_addr = slave.peer_addr;
  Serial.println("==================="); 
  Serial.print("Type: "); 
  Serial.println(send.type);
  Serial.print("ID :");
  for(int i=0; i<6; i++) {
        Serial.print(send.id[i]);
      }   
  Serial.println(""); 
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
}

// kirim rts
void sendRts(){
  float a = 0;  // 0 data for rts
  float b = 0;  // 0 data for rts
  char tmp_id[6];
  for (int i = 0; i < 6; ++i) {
       tmp_id[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  memcpy(send.id, tmp_id, sizeof(send.id)); // our mac
  send.type = 1; // rts
  float data[2] = {a, b}; 
  memcpy(send.sender, mac, sizeof(mac)); // gateway ip for rts
  memcpy(send.receiver, receiver, sizeof(receiver)); // packet receiver mac
  memcpy(send.data, data, sizeof(data)); // 0 data
  Serial.print("Size of RTS :"); Serial.println(sizeof(send));
  send.time = 5;
  sendData(send); // send packet
}

// kirim data sensor terbaru setelah CTS
void sendCurrent (){
  float a = rand() % 10;  // random data for now
  float b = rand() % 11+100;  // random data for now
  char tmp_id[6];
  for (int i = 0; i < 6; ++i) {
        tmp_id[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  memcpy(send.id, tmp_id, sizeof(send.id)); // our mac
  send.type = 4; // data
  float data[2] = {a, b};
  memcpy(send.sender, mac, sizeof(mac)); // gateway ip for rts
  memcpy(send.receiver, receiver, sizeof(receiver)); // packet receiver mac
  memcpy(send.data, data, sizeof(data)); // current data
  send.time = 5; 
  Serial.print("Size of current packet :"); Serial.println(sizeof(send));
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
  Serial.println("Setting up sensor....");
  Serial.print("Sensor MAC: "); 
  Serial.println(WiFi.macAddress());
  WiFi.macAddress(mac);

  // initialize ESP_NOW
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }

  // callback when data send and receive
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // initialize srand for id randomizer
  srand(time(NULL));
  
  // clear peer in case if slave already exist
  memset(&slave, 0, sizeof(slave));

  // add broadcast address to peer
  for (int ii = 0; ii < 6; ++ii) {
    slave.peer_addr[ii] = (uint8_t)0xff;
  }
  slave.channel = CHANNEL; // channel from global var
  slave.encrypt = 0; // no encryption
  if (slave.channel == CHANNEL) {
    Serial.print("Slave Status: ");
    const esp_now_peer_info_t *peer = &slave;
    const uint8_t *peer_addr = slave.peer_addr;
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(peer_addr);
    if (exists) {
      // Slave already paired.
      Serial.println("Already Paired");
    }
    else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(peer);
      if (addStatus == ESP_OK) {
        Serial.println("Pair success");
      }
      else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println("ESPNOW Not Init");
      }
      else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");
      }
      else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println("Peer list full");
      }
      else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("Out of memory");
      }
      else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Peer Exists");
      }
      else {
        Serial.println("Not sure what happened");
      }
    }
  }
  else {
    Serial.println("No Slave found to process");
  }
}

void loop() {
  // clear packet ID every 10 seconds
  if (millis() - lastClear > 10000) {
    clearTable();
    lastClear = millis(); // counter for next clear
  } 
  // if current time is not higher than lastCts then dont send any data
  if (millis() > lastCts){
    // if current time - last data transmission time is more than 25s
    if (millis() - lastSend > 25000){
       sendRts();
       lastSend = millis();
    } 
  }

  // print packet table
  if (millis() - lastPrint > 10000){
    printTable();
    lastPrint = millis();
  }
  delay(1000);
}