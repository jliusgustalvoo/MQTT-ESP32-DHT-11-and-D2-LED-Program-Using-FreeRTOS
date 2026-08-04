#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// --- Pengaturan Dasar ---
const char* ssid = "Ayam Kate";
const char* password = "supangat10";
const char* mqtt_server = "broker.hivemq.com";
const int LED_PIN = 2; 

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

// =========================================================
// TASK 1: PETUGAS JAGA KOMUNIKASI (Dijalankan di Core 1)
// Tugas: Mengurus WiFi, Reconnect MQTT, dan Menerima Perintah
// =========================================================
void TaskKomunikasi(void *pvParameters) {
  // Setup Task (berjalan sekali)
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Loop Task (berjalan selamanya)
  for (;;) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop(); // Pasang telinga

    // Beri jeda 10 milidetik agar CPU tidak overheat (Wajib di FreeRTOS)
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

// =========================================================
// TASK 2: PETUGAS BACA SENSOR (Dijalankan di Core 0)
// Tugas: Membaca DHT11 dan mengirim data setiap 5 detik
// =========================================================
void TaskSensor(void *pvParameters) {
  // Setup Task (berjalan sekali)
  dht.begin();

  // Loop Task (berjalan selamanya)
  for (;;) {
    if (client.connected()) {
      float suhuAsli = dht.readTemperature();
      
      if (!isnan(suhuAsli)) {
        String paketJSON = "{\"suhu\": " + String(suhuAsli, 1) + "}"; 
        Serial.println("[CORE 0] Mengirim: " + paketJSON);
        client.publish("julius/sensor/suhu", paketJSON.c_str());
      }
    }
    
    // INILAH PENGGANTI MILLIS!
    // Tidurkan tugas ini selama 5000 milidetik.
    // Selama tugas ini tidur, Core 0 bisa istirahat atau mengerjakan hal lain.
    vTaskDelay(5000 / portTICK_PERIOD_MS); 
  }
}

// =========================================================
// FUNGSI CALLBACK & RECONNECT (Sama seperti sebelumnya)
// =========================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String pesan = "";
  for (int i = 0; i < length; i++) pesan += (char)payload[i];
  
  if (pesan == "ON") digitalWrite(LED_PIN, HIGH);
  else if (pesan == "OFF") digitalWrite(LED_PIN, LOW);
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS); // Gunakan vTaskDelay sebagai pengganti delay() biasa
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32_Pekerja_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe("julius/kontrol/lampu");
    } else {
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
}

// =========================================================
// PUSAT KENDALI UTAMA (SETUP)
// =========================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("Memulai Sistem Operasi FreeRTOS...");

  // Mendaftarkan Koki ke Dapur masing-masing!
  
  // 1. Buat Task Komunikasi, lemparkan ke Core 1 (Core Utama)
  xTaskCreatePinnedToCore(
    TaskKomunikasi,   // Nama fungsi
    "Task_MQTT",      // Nama alias untuk debugging
    10000,            // Ukuran memori yang disediakan (Stack)
    NULL,             // Parameter tambahan (kosongkan)
    1,                // Prioritas (1 = Standar)
    NULL,             // Handle (kosongkan)
    1                 // Dijalankan di Core 1
  );

  // 2. Buat Task Sensor, lemparkan ke Core 0 (Core Rahasia)
  xTaskCreatePinnedToCore(
    TaskSensor,       // Nama fungsi
    "Task_Sensor",    // Nama alias
    10000,            // Ukuran memori
    NULL,             // Parameter
    1,                // Prioritas
    NULL,             // Handle
    0                 // Dijalankan di Core 0!
  );
}

// Loop dibiarkan kosong, karena FreeRTOS yang mengambil alih!
void loop() {
  vTaskDelete(NULL); // Hapus loop bawaan agar memori lebih lega
}