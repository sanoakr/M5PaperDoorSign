#include <M5EPD.h>
#include <WiFi.h>
#include <WebServer.h>

#define fullWidth 960
#define fullHeight 540
#define smallTextSize 40
#define largeTextSize 132

#define nameWidth 400
#define nameHeight 100
#define nameX 0
#define nameY 0
//
#define statusWidth (fullWidth - nameWidth)
#define statusHeight nameHeight
#define statusX nameWidth
#define statusY nameY
//
#define boardMargin 30
#define boardWidth fullWidth
#define boardHeight (largeTextSize + boardMargin)
#define boardX 0 
#define boardY nameHeight
//
#define msgWidth fullWidth
#define msgHeight (fullHeight - (nameHeight+boardHeight+qrHeight))
#define msgX 0 
#define msgY (nameHeight + boardHeight)
//
#define qrWidth 300
#define qrHeight 150
#define qrX 0
#define qrY (fullHeight - qrHeight)
//
#define buttonWidth (fullWidth - qrWidth)
#define buttonHeight 100
#define buttonX fullWidth - buttonWidth
#define buttonY (fullHeight - buttonHeight)
#define buttonSize buttonHeight

M5EPD_Canvas canvas(&M5.EPD);
M5EPD_Canvas buttonCanvas(&M5.EPD);
M5EPD_Canvas nameCanvas(&M5.EPD);
M5EPD_Canvas boardCanvas(&M5.EPD);
M5EPD_Canvas msgCanvas(&M5.EPD);
M5EPD_Canvas statusCanvas(&M5.EPD);
M5EPD_Canvas qrCanvas(&M5.EPD);
WiFiServer server(80);
String receivedFileName;

enum ConnectionType
{
  UNDEFINED_CONNECTION,
  GET_CONNECTION,
  POST_CONNECTION
};
enum FileType
{
  UNDEFINED_FILE,
  PNG_FILE,
  JPG_FILE
};
enum GetType
{
  UNDEFINED_GET,
  FORM_GET,
  BOARD_GET
};
enum BoardType
{
  FORM_BOARD,
  ABSENCE_BOARD,
  CAMPUS_BOARD,
  ONLINE_BOARD,
  BEIN_BOARD,
  OH_BOARD,
  MEETING_BOARD
};
BoardType boardTypeList[] = {FORM_BOARD, ABSENCE_BOARD, CAMPUS_BOARD, ONLINE_BOARD, BEIN_BOARD, OH_BOARD, MEETING_BOARD};
String boardNames[] = {"/form", "/absence", "/campus", "/online", "/bein", "/oh", "/meeting"};
String boardTexts[] = {"", "不在です", "学内にいます", "オンライン中", "在室してます", "オフィスアワー", "ミーティング中"};
String boardSubTexts[] = {"", "", "", "オンライン会議・オンライン講義中です", "", "", "（在室してます）"};
String btnNames[] = {"", "不在", "学内", "OL", "在室", "OH", "MTG"};

void sendResponse(WiFiClient client, String firstLine);
void sendFormHTML(WiFiClient client);
void receiveFormText(WiFiClient client);
void receiveFormFile(WiFiClient client);
void receivePostFile(WiFiClient client, String fileName);
void displayImageOfFileName(String fileName);

// Touch Point
int p_x = 0, p_y = 0;

// Set Board Message
void showBoard(int boardType, bool subText) {
  // Draw Board Text
  boardCanvas.fillCanvas(0);
  boardCanvas.drawString(boardTexts[boardType], boardMargin, boardMargin);
  boardCanvas.pushCanvas(boardX, boardY, UPDATE_MODE_DU4);

  if (subText) {
    msgCanvas.fillCanvas(0);
    msgCanvas.drawString(boardSubTexts[boardType], boardMargin, boardMargin);
    msgCanvas.pushCanvas(msgX, msgY, UPDATE_MODE_DU4);
  }
}
// Show IP & Clear Board
void showIP(bool clear = true, int delayTime = 5000) {
  // Draw IP Address
  String ipString = WiFi.localIP().toString();
  statusCanvas.drawString(ipString, boardMargin, boardMargin);
  statusCanvas.pushCanvas(statusX, statusY, UPDATE_MODE_DU4);
  
  delay(delayTime);
  // Clear Board
  if (clear) {
    boardCanvas.fillCanvas(0);
    boardCanvas.pushCanvas(boardX, boardY, UPDATE_MODE_GL16);
    msgCanvas.fillCanvas(0);
    msgCanvas.pushCanvas(msgX, msgY, UPDATE_MODE_GL16);
  }
  statusCanvas.fillCanvas(0);
  statusCanvas.pushCanvas(statusX, statusY, UPDATE_MODE_GL16);
  showBoard(ABSENCE_BOARD, true);
}

void setup()
{
  // Initialize M5Paper
  M5.begin();
  M5.EPD.SetRotation(0);
  M5.EPD.Clear(true);

  receivedFileName = "";

  // Create fullscreen canvas
  canvas.createCanvas(fullWidth, fullHeight);
  canvas.setTextSize(3);

  // Set Japanese Font
  canvas.drawString("Loading font ...", 270, 250);
  canvas.pushCanvas(0,0,UPDATE_MODE_DU4);
  Serial.println("Loading font from SD.");
  canvas.loadFont("/font.ttf", SD);
  canvas.createRender(largeTextSize, 256);
  canvas.createRender(smallTextSize, 256);
  canvas.setTextSize(smallTextSize);
  canvas.drawString("完了", 600, 250);
  canvas.pushCanvas(0,0,UPDATE_MODE_DU4);
  Serial.println("Loading done.");
  
  // Create shutdown button canvas
  buttonCanvas.createCanvas(buttonWidth, buttonHeight);
  buttonCanvas.setTextSize(smallTextSize);
  // Create name canvas
  nameCanvas.createCanvas(statusWidth, statusHeight);
  nameCanvas.setTextSize(smallTextSize);
  // Create status text canvas
  statusCanvas.createCanvas(statusWidth, statusHeight);
  statusCanvas.setTextSize(smallTextSize);
  // Create built-in board canvas
  boardCanvas.createCanvas(boardWidth, boardHeight);
  boardCanvas.setTextSize(largeTextSize);
  // Create message canvas
  msgCanvas.createCanvas(boardWidth, boardHeight);
  msgCanvas.setTextSize(smallTextSize);
  // Create Teams QR canvas
  qrCanvas.createCanvas(qrWidth, qrHeight);
  qrCanvas.setTextSize(smallTextSize);

  // Load WiFi SSID and PASS from "wifi.txt" in SD card
  String wifiIDString = "wifiID";
  String wifiPWString = "wifiPW";
  String qrString = "qrString";
  File wifiSettingFile = SD.open("/wifi.txt");
  if (wifiSettingFile)
  {
    String line = wifiSettingFile.readStringUntil('\n');
    int location = line.indexOf("SSID:");
    if (location >= 0 && line.length() > 5) {
      wifiIDString = line.substring(5);
      wifiIDString.trim();
    }
    line = wifiSettingFile.readStringUntil('\n');
    location = line.indexOf("PASS:");
    if (location >= 0 && line.length() > 5) {
      wifiPWString = line.substring(5);
      wifiPWString.trim();
    }
    line = wifiSettingFile.readStringUntil('\n');
    location = line.indexOf("QR:");
    if (location >= 0 && line.length() > 3) {
      qrString = line.substring(3);
      qrString.trim();
    }
    wifiSettingFile.close();
  }

  WiFi.begin(wifiIDString.c_str(), wifiPWString.c_str());

  Serial.println(wifiIDString);
  // Wait until wifi connected
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Get IP Address and create URL
  IPAddress address = WiFi.localIP();
  String addressString = address.toString();
  String urlString = "http://" + addressString + "/";

  // Start web server
  server.begin();

  // Draw Name Text
  nameCanvas.drawString("さのは（おそらく）", 30, 30);
  nameCanvas.pushCanvas(nameX, nameY, UPDATE_MODE_DU4);
  
  // Draw Board Text
  showBoard(ONLINE_BOARD, true);
    
  // Draw buttons
  for (int i = 0; i < 6; i++) {
    buttonCanvas.drawString(btnNames[i+1], i*buttonSize+10, 30);
    buttonCanvas.drawRect(i*buttonSize, 0, buttonSize, buttonSize, WHITE);
  }
  buttonCanvas.pushCanvas(buttonX, buttonY, UPDATE_MODE_DU4);

  // Display SSID, IP Address and QR code for this M5Paper
  canvas.drawString(wifiIDString, 540, 20);
  canvas.drawString(urlString, 540, 50);
  //canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
  
  qrCanvas.drawString("Teams", qrWidth/2, qrHeight/2-20);
  qrCanvas.drawString("   Chat", qrWidth/2, qrHeight/2+smallTextSize-20);
  qrCanvas.qrcode(qrString, 0, 0, qrHeight, 4);
  qrCanvas.pushCanvas(qrX, qrY, UPDATE_MODE_DU4);
}

void loop()
{
  // Wait client in main loop
  WiFiClient client = server.available();

  if (client)
  {
    enum ConnectionType connectionType = UNDEFINED_CONNECTION;
    enum FileType fileType = UNDEFINED_FILE;
    enum GetType getType = UNDEFINED_GET;
    enum BoardType boardType = FORM_BOARD;
    String line = "";

    while (client.connected())
    {
      if (client.available())
      {
        line = client.readStringUntil('\n');
        Serial.println(line);
        //Serial.println(line.length());

        // Read http header
        if (line.startsWith("GET /")) {
          connectionType = GET_CONNECTION;
          for (BoardType type: boardTypeList) {
            if (line.indexOf(boardNames[type]) > 3) {
              getType = BOARD_GET;
              boardType = type;
              Serial.println("BOARD_GET");
              break;
            }
          }
          if (boardType == boardTypeList[0] or line.length() == 5) {
            getType = FORM_GET;
            Serial.println("FORM_GET");
          }
        }
        else if (line.startsWith("POST /"))
          connectionType = POST_CONNECTION;
        else if (line.startsWith("Content-Type:"))
        {
          if (line.indexOf("image/png") > 0)
            fileType = PNG_FILE;
          else if (line.indexOf("image/jpeg") > 0)
            fileType = JPG_FILE;
        }

        if (line.length() <= 2) 
        { // Empty line. header finished
          switch (connectionType)
          {
          case GET_CONNECTION:
          { // Send form.html
            Serial.println("GET_CONNECTION");
            switch (getType)
            {
            case FORM_GET:
            {
              sendResponse(client, "HTTP/1.1 200 OK");
              sendFormHTML(client);
              //statusCanvas.drawString("form.html sent.", 0, 0);
              //statusCanvas.pushCanvas(statusX, statusY, UPDATE_MODE_DU4);
              break;
            }
            case BOARD_GET:
            {
              showBoard(boardType, true);
              break;
            }
            default:
              break;
            }
            sendResponse(client, "HTTP/1.1 200 OK");
            //client.println("<!DOCTYPE html><head><meta charset=\"UTF-8\"></head><body><p>Received successfully.</p></body>");
          }
          case POST_CONNECTION:
          { // Receive form text or file
            Serial.println("POST_CONNECTION");
            switch (fileType)
            {
            case PNG_FILE:
            { // Receive PNG file
              Serial.println("PNG_FILE");
              //statusCanvas.drawString("Start receving PNG file.", 0, 0);
              //statusCanvas.pushCanvas(statusX, statusY, UPDATE_MODE_DU4);
              receivePostFile(client, "/received.png");
              break;
            }
            case JPG_FILE:
            { // Receive JPG file (not used)
              Serial.println("JPG_FILE");
              //statusCanvas.drawString("Start receving JPG file.", 0, 0);
              //statusCanvas.pushCanvas(statusX, statusY, UPDATE_MODE_DU4);
              receivePostFile(client, "/received.jpg");
              break;
            }
            default:
            { // Receive TXT file (not used)
              Serial.println("Text file");
              //statusCanvas.drawString("Start receving TXT file.", 0, 0);
              //statusCanvas.pushCanvas(statusX, statusY, UPDATE_MODE_DU4);
              receiveFormText(client);
            }
            }
            sendResponse(client, "HTTP/1.1 200 OK");
            //client.println("<!DOCTYPE html><head><meta charset=\"UTF-8\"></head><body><p>Received successfully.</p></body>");
          }
          default:
          { // Other request
            sendResponse(client, "HTTP/1.1 404 Not Found");
            //client.println("<!DOCTYPE html><head><meta charset=\"UTF-8\"></head><body><p>404 Not Found</p></body>");
          }
          }
          delay(1);
          client.stop();
        }
      }
    }

    if (receivedFileName.length() > 0)
    { // File received!
      Serial.println(receivedFileName);

      // Display received file
      displayImageOfFileName(receivedFileName);

      // Shutdown M5Paper
      delay(500);
      M5.shutdown();
      return;
    }
  }

  // Touch detection for button
  if (M5.TP.avaliable()) {
    if (!M5.TP.isFingerUp()) {
      M5.TP.update();

      bool is_update = false;
      tp_finger_t FingerItem = M5.TP.readFinger(0);
      if (p_x != FingerItem.x || p_y != FingerItem.y) {
        p_x = FingerItem.x;
        p_y = FingerItem.y;
        is_update = true;
      }

      if (is_update) {
        for (int i = 0; i < 6; i++) {
          if (p_x > buttonX + i*buttonSize && p_x < buttonX + (i+1)*buttonSize
            && p_y > buttonY && p_y < (buttonY + buttonHeight)) {

            showBoard(i+1, true);
            Serial.println(boardNames[i+1]);
          }
        }
        if (p_x > statusX && p_x < statusX + statusWidth
          && p_y > statusY && p_y < (statusY + statusHeight)) {

          showIP(true);
        }
      }
      /*
      { // Touch up inside shutdown button
        // Invert button image
        buttonCanvas.pushCanvas(buttonX, buttonY, UPDATE_MODE_DU4);
        // display last image
        displayImageOfFileName("/received.png");
        // Shutdown M5Paper
        delay(500);
        M5.shutdown();
        return;
      }
      */
    }
  }
}

// Return http response header
void sendResponse(WiFiClient client, String firstLine)
{
  // First line should be like "HTTP/1.1 200 OK"
  client.println(firstLine);
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
}

// Return content of form.html in SD card
void sendFormHTML(WiFiClient client)
{
  File htmlFile = SD.open("/form.html");
  if (htmlFile)
  { // send form.html line by line
    while (htmlFile.position() < htmlFile.size())
    {
      String htmlLine = htmlFile.readStringUntil('\n');
      client.println(htmlLine);
    }
    htmlFile.close();
  }
  else
  { // when form.html not found
    //client.println("<!DOCTYPE html><head><meta charset=\"UTF-8\"></head><body><p>No html file!</p></body>");
  }
  client.flush();
}

// Handle POST body from text form. Content will be written in /received.txt on SD card
void receiveFormText(WiFiClient client)
{
  SD.remove("/received.txt");
  File receivedFile = SD.open("/received.txt", FILE_WRITE);
  while (client.available())
  {
    String line = client.readStringUntil('\n');
    if (line.length() <= 0)
    {
      break;
    }
    receivedFile.println(line);
  }
  receivedFile.close();
}

// Handle POST body from <input type="file">. No longer used in form.html
void receiveFormFile(WiFiClient client)
{
  String fileName = "/received.jpg";
  String boundary = "";
  // size_t boundarySize = 0;
  while (client.available())
  {
    // Read multipart header
    String line = client.readStringUntil('\n');
    if (line.length() < 2)
    {
      break;
    }
    else if (line.startsWith("-"))
    {
      boundary = line;
    }
    else if (line.startsWith("Content-Type:"))
    {
      if (line.indexOf("png") >= 0)
      {
        fileName = "/received.png";
      }
    }
  }
  // boundarySize = boundary.length();
  // In this code, multipart boundary string will be written after binary...
  receivePostFile(client, fileName);
}

// Handle POST body from XMLHttpRequest. More simple because of no boundary
void receivePostFile(WiFiClient client, String fileName)
{
  SD.remove(fileName);
  File receivedFile = SD.open(fileName, FILE_WRITE);
  while (client.available())
  {
    byte buffer[256];
    size_t bufferLength = client.readBytes(buffer, 256);

    if (bufferLength > 0)
    {
      receivedFile.write(buffer, bufferLength);
    }
    else
      break;
  }
  receivedFile.close();
  receivedFileName = fileName;
}

void displayImageOfFileName(String fileName)
{
  char fileNameChar[fileName.length() + 1];
  fileName.toCharArray(fileNameChar, fileName.length() + 1);

  // Fill screen with white color first to prevent ghost
  msgCanvas.fillCanvas(BLACK);
  msgCanvas.pushCanvas(0, 0, UPDATE_MODE_DU4);

  Serial.println("Draw " + fileName + " on screen.");
  if (fileName.endsWith("png"))
  {
    // PNG file
    msgCanvas.drawPngFile(SD, fileNameChar);
  }
  else if (fileName.endsWith("jpg"))
  {
    // JPG file
    msgCanvas.drawJpgFile(SD, fileNameChar);
  }
  msgCanvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
}