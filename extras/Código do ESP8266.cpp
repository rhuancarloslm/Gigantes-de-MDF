#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Configurações da rede Wi-Fi
const char* ssid = "gusta";
const char* password = "gusta123";

ESP8266WebServer server(80); // Porta do servidor HTTP

// HTML com JavaScript para controle via teclado
String generateHTML() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Controle do Carrinho</title>
    <style>
      body {
        text-align: center;
        font-family: Arial, sans-serif;
        margin-top: 50px;
      }
      .instructions {
        margin-top: 20px;
        font-size: 18px;
        color: gray;
      }
    </style>
  </head>
  <body>
    <h1>Controle do Carrinho</h1>
    <p class="instructions">Use as teclas <b>W</b> (Frente), <b>A</b> (Esquerda), <b>S</b> (Tras), <b>D</b> (Direita).</p>
    <p class="instructions">Solte as teclas para parar o carrinho.</p>

    <script>
      // Função para enviar comandos
      function sendCommand(command) {
        fetch(`/command?cmd=${command}`).catch((error) => {
          console.error("Erro ao enviar comando:", error);
        });
      }

      // Detecta teclas pressionadas
      document.addEventListener("keydown", (event) => {
        switch (event.key.toLowerCase()) {
          case "w": // Frente
            sendCommand("W");
            break;
          case "a": // Esquerda
            sendCommand("A");
            break;
          case "s": // Trás
            sendCommand("S");
            break;
          case "d": // Direita
            sendCommand("D");
            break;
        }
      });

      // Detecta quando a tecla é solta
      document.addEventListener("keyup", (event) => {
        sendCommand("X"); // Parar os motores
      });
    </script>
  </body>
  </html>
  )rawliteral";
  return html;
}

void setup() {
  Serial.begin(115200);

  // Conexão ao Wi-Fi
  Serial.print("Conectando-se ao Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.print("\nConectado ao Wi-Fi!");
  Serial.print("Endereço IP: ");
  Serial.print(WiFi.localIP());

  // Configuração da rota para a página HTML
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", generateHTML());
  });

  // Rota para receber comandos via JavaScript
  server.on("/command", HTTP_GET, []() {
    if (server.hasArg("cmd")) {
      String command = server.arg("cmd");
      Serial.print(command);
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.print("Servidor iniciado!");
}

void loop() {
  server.handleClient();
}