#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SPIFFS.h>

#define WIFI_SSID "HOTPOT"
#define WIFI_PASS "pass1234"

WebServer server(80);

// Cached file content
String str_file_index="";
String str_file_style="";
String str_file_favicon="";

// Message storage
struct Message{
  char sender[11]{0};
  char message[151]{0};
};
int index_head = 0;
int index_tail = 0;
const int max_messages = 11; // 1 empty slot as divider
Message messages[max_messages];

// Functions: Server 
void connectToNetwork();
void setupServer();

// Functions: SPIFFS 
String readFile(const char* filepath);
void cacheFiles();

// Functions: Messages 
void addMessage(String sender, String message);
void addTestMessages();
void getNextSlot(int &index);
void prepareNextMessageSlot();

// Functions: HTTP request handler
void returnMessages();
void handleNewMessage();
void serveRoot();
void serveStyle();
void serveFavIcon();
void serveNotFound();

void setup()
{
  Serial.begin(115200);
  SPIFFS.begin();

  cacheFiles();
  // addTestMessages();

  connectToNetwork();
  setupServer();
}

void loop(){
  server.handleClient();
}

// ============================
// Server 
// ============================

void connectToNetwork()
{
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for(int i=10;i>0;--i)
  {
    delay(1000);
    Serial.print(".");
    if(WL_CONNECTED==WiFi.status()) break;
  }

  if(WL_CONNECTED!=WiFi.status())
  {
    Serial.println("Failed to connect.");
    for(;;);
  }
  
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP().toString());
}

void setupServer()
{
  // Handlers
  server.onNotFound(serveNotFound);

  server.on("/", serveRoot);
  server.on("/style1.css", serveStyle);
  server.on("/favicon.ico", serveFavIcon);

  server.on("/messages", returnMessages);
  server.on("/new", handleNewMessage);

  server.begin();
  Serial.println("Server started");
}

// ============================
// SPIFFS 
// ============================

String readFile(const char* filepath)
{
  String tmp;
  File file = SPIFFS.open(filepath, "r");
  if(file)
  {
    Serial.printf("%s (%d)\n", filepath, file.size());
    tmp.reserve(file.size());
    while(file.available())
    {
      tmp += file.readString();
    }
    file.close();
  }
  Serial.printf("Loaded: (%d)\n", tmp.length());
  return tmp;
}

void cacheFiles()
{  
  str_file_index = readFile("/index.html");
  str_file_style = readFile("/style1.css");
  str_file_favicon = readFile("/favicon.ico");
}

// ============================
// Messages 
// ============================

void addMessage(String sender, String message)
{
  sprintf(messages[index_tail].sender,sender.c_str());
  sprintf(messages[index_tail].message,message.c_str());
  prepareNextMessageSlot();
}

void addTestMessages()
{
  addMessage("Allie","Hello");
  addMessage("Bob","Hi");
  addMessage("Colleen","Hey");
  addMessage("Dave","Hullooo");
}

void getNextSlot(int &index)
{
  index = (index + 1) % max_messages;
}

void prepareNextMessageSlot()
{
  getNextSlot(index_tail);

  // overwrite older messages
  if(index_head == index_tail) getNextSlot(index_head);
}

// ============================
// HTTP Request Handlers
// ============================

void returnMessages()
{
  char buffer[ max_messages * (sizeof(Message) + 30)]="{\"sender\":[";
  char* buffer_pointer = buffer + strlen(buffer);

  // fill in sender array
  for (int index = index_head; 
    index != index_tail; 
    getNextSlot(index))
  {
    sprintf(buffer_pointer,"\"%s\",",messages[index].sender);
    buffer_pointer += strlen(buffer_pointer);
  }
   // remove last comma
  if(index_head!=index_tail) --buffer_pointer;
  
  sprintf(buffer_pointer, "],\"message\":[");
  buffer_pointer += strlen(buffer_pointer);

  // fill in message array
  for (int index = index_head; 
    index != index_tail; 
    getNextSlot(index))
  {
    sprintf(buffer_pointer,"\"%s\",",messages[index].message);
    buffer_pointer += strlen(buffer_pointer);
  }  
   // remove last comma
  if(index_head!=index_tail) --buffer_pointer;
  
  sprintf(buffer_pointer, "]}");
  buffer_pointer += strlen(buffer_pointer);

  server.send(200,"text/plain", buffer);
}

void handleNewMessage()
{
  Serial.printf("Sender: %s\nMsg: %s\n",
    server.arg("sender").c_str(),
    server.arg("msg").c_str());

  addMessage(
    server.arg("sender").substring(1,server.arg("sender").length() - 1),
    server.arg("msg").substring(1,server.arg("msg").length() - 1)
    );
  
  returnMessages();
}

void serveRoot() { server.send(200,"text/html", str_file_index); }
void serveStyle() { server.send(200,"text/css",str_file_style); }
void serveFavIcon() { server.send(200,"image/x-icon", str_file_favicon); }
void serveNotFound(){ server.send(299,"text/html","<h2>HTTP 299 Disappointed:</h2> <br/>The server has accepted your request but thinks you can do better"); }