#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         5  // Ajusta a tu configuración
#define SS_PIN          53 // Ajusta a tu configuración

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  
  Serial.println(F("Iniciando..."));

  // Verificar conexión con el lector RFID
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if (v == 0x00 || v == 0xFF) {
    Serial.println(F("Error: No se pudo leer la versión del firmware, verifica la conexión del lector."));
    while (true); // Detiene el programa aquí si no se detecta el lector
  }
  Serial.print(F("Versión del firmware: 0x"));
  Serial.println(v, HEX);

  // Mostrar opciones al usuario
  Serial.println(F("Seleccione una opción:"));
  Serial.println(F("1. Escribir en la tarjeta"));
  Serial.println(F("2. Leer la tarjeta"));

  while (!Serial.available()) {
    // Espera a que el usuario envíe una opción
    delay(100);
  }

  char opcion = Serial.read(); // Lee la opción seleccionada

  // Limpiar el buffer Serial para evitar lecturas erróneas en llamadas futuras
  while (Serial.available()) Serial.read();

  // Verificar la presencia de la tarjeta después de elegir la opción
  Serial.println(F("Aproxime la tarjeta al lector..."));
  unsigned long startTime = millis();
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    if (millis() - startTime > 5000) { // Espera 5 segundos
      Serial.println(F("No se detecta ninguna tarjeta. Reintente."));
      return; // Salir del setup si no se encuentra ninguna tarjeta en 5 segundos
    }
    delay(100);
  }

  if (opcion == '1') {
    // Opción para escribir en la tarjeta
    Serial.println(F("Escribiendo en la tarjeta..."));
    String texto = "Placas ABC-123, Serie XYZ, Marca Toyota, Modelo 2020, Color Rojo, Número de Captura 456";
    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
    escribeTextoEnTarjeta(texto, key);
  } else if (opcion == '2') {
    // Opción para leer la tarjeta
    Serial.println(F("Leyendo la tarjeta..."));
    leerContenidoDeLaTarjeta();
  } else {
    Serial.println(F("Opción no válida."));
  }
}

void loop() {
  // Mantén el loop vacío
}

void escribeTextoEnTarjeta(String texto, MFRC522::MIFARE_Key key) {
  int indexTexto = 0; // Índice para el texto
  byte buffer[18]; // Buffer para almacenar el segmento de texto a escribir

  // Comenzar desde el sector 1, evitando el sector 0 completamente
  for (int sector = 1; sector < 16; sector++) {
    // Solo usa los primeros tres bloques de cada sector, evitando el bloque de trailer
    for (int bloqueRelativo = 0; bloqueRelativo < 3; bloqueRelativo++) {
      int bloque = sector * 4 + bloqueRelativo; // Calcula el bloque absoluto

      if (indexTexto < texto.length()) {
        // Prepara el segmento de texto para escribir
        String segmento = texto.substring(indexTexto, min(indexTexto + 16, texto.length()));
        segmento.getBytes(buffer, 18); // Asegúrate de que el buffer es limpiado adecuadamente si el texto es más corto
        memset(buffer + segmento.length(), 0, 16 - segmento.length()); // Rellena el resto del buffer con 0s

        // Autenticación con la llave A para el bloque actual
        MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bloque, &key, &(mfrc522.uid));
        if (status != MFRC522::STATUS_OK) {
          Serial.print(F("Autenticación fallida en bloque ")); Serial.println(bloque);
          return;
        }

        // Escribir el segmento de texto en el bloque actual
        status = mfrc522.MIFARE_Write(bloque, buffer, 16);
        if (status != MFRC522::STATUS_OK) {
          Serial.print(F("Escritura fallida en bloque ")); Serial.println(bloque);
          return;
        }
        
        indexTexto += 16; // Incrementa el índice de texto para el próximo segmento
      }
    }
  }

  Serial.println(F("Escritura completada."));
  mfrc522.PICC_HaltA(); // Halt PICC
  mfrc522.PCD_StopCrypto1(); // Detiene la encriptación en PCD
}
void leerContenidoDeLaTarjeta() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF; // Llave por defecto

  Serial.println(F("Leyendo contenido de la tarjeta..."));

  // Iterar sobre los sectores y bloques válidos
  for (byte sector = 1; sector < mfrc522.PICC_GetType(mfrc522.uid.sak); sector++) {
    for (byte bloqueRelativo = 0; bloqueRelativo < 3; bloqueRelativo++) { 
      byte blockAddr = sector * 4 + bloqueRelativo;

      // Autenticación usando la llave A
      MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(mfrc522.uid));
      if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Autenticación fallida para el bloque: ")); Serial.println(blockAddr);
        continue;
      }

      byte buffer[18];
      byte blockSize = sizeof(buffer);
      status = mfrc522.MIFARE_Read(blockAddr, buffer, &blockSize);
      if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Error de lectura en bloque: ")); Serial.println(blockAddr);
        continue;
      }

      // Convertir de Hex a ASCII para mostrar el contenido
      Serial.print(F("Bloque ")); Serial.print(blockAddr); Serial.print(F(": "));
      for (byte i = 0; i < 16; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
          Serial.write(buffer[i]);
        } else if (buffer[i] == 0) { // Omitir o reemplazar valores nulos si es necesario
          // Serial.write(' '); // Descomentar si deseas agregar espacios en lugar de omitir
        } else {
          Serial.write('.'); // Reemplaza caracteres no imprimibles con puntos
        }
      }
      Serial.println();
    }
  }
  Serial.println(F("Lectura completada."));

  mfrc522.PICC_HaltA(); // Halt PICC
  mfrc522.PCD_StopCrypto1(); // Detiene la encriptación en PCD
}

