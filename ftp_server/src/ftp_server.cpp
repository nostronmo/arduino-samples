#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>

const char *WIFI_SSID = "NONE";
const char *WIFI_PASSWORD = "NONE";

WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("Your ESP32 IP Address is: ");
  Serial.println(WiFi.localIP());

  Serial.print("Initializing SD card...");
  if (!SD.begin(15)) {
    Serial.println(" Card Mount Failed!");
    return;
  }
  Serial.println(" SD Card initialized.");

  server.on("/", []() {
    String html = "<!DOCTYPE html><html><head><title>ESP32 SD Card</title>";
    html += "<style>body{font-family: Arial;} .btn{padding: 5px 10px; "
            "text-decoration: none; color: white; border-radius: 4px; "
            "font-size: 14px;} .btn-dl{background-color: #4CAF50;} "
            ".btn-del{background-color: #f44336; margin-left: 10px;}</style>";
    html += "</head><body><h2>Arquivos no Cartão SD</h2><ul>";

    File root = SD.open("/");
    File file = root.openNextFile();

    if (!file) {
      html += "<li>Nenhum arquivo encontrado no cartao SD.</li>";
    }

    while (file) {
      String fileName = String(file.name());
      html += "<li style='margin-bottom: 10px;'>";
      html += "<strong>" + fileName + "</strong> (" + String(file.size()) +
              " bytes)<br>";

      html += "<a href=\"/download?file=" + fileName +
              "\" class=\"btn btn-dl\">Baixar</a>";

      html += "<a href=\"/delete?file=" + fileName +
              "\" class=\"btn btn-del\" onclick=\"return confirm('Tem certeza "
              "que deseja apagar " +
              fileName + "? Esta ação não pode ser desfeita!');\">Apagar</a>";

      html += "</li>";
      file = root.openNextFile();
    }

    html += "</ul></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/download", []() {
    if (server.hasArg("file")) {
      String fileName = server.arg("file");
      if (!fileName.startsWith("/"))
        fileName = "/" + fileName;

      File downloadFile = SD.open(fileName);
      if (downloadFile) {
        server.sendHeader("Content-Type", "application/octet-stream");
        server.sendHeader("Content-Disposition",
                          "attachment; filename=" + fileName);
        server.sendHeader("Connection", "close");
        server.streamFile(downloadFile, "application/octet-stream");
        downloadFile.close();
        return;
      }
    }
    server.send(404, "text/plain", "Erro: Arquivo nao encontrado!");
  });

  server.on("/delete", []() {
    if (server.hasArg("file")) {
      String fileName = server.arg("file");
      if (!fileName.startsWith("/"))
        fileName = "/" + fileName;

      if (SD.exists(fileName)) {
        SD.remove(fileName);
        Serial.println("File deleted: " + fileName);

        server.sendHeader("Location", "/");
        server.send(303);
        return;
      }
    }
    server.send(404, "text/plain", "Erro: Nao foi possivel apagar o arquivo.");
  });

  server.begin();
  Serial.println(
      "Servidor Web Iniciado! Abra o navegador e digite o endereço IP acima.");
}

void loop() { server.handleClient(); }
