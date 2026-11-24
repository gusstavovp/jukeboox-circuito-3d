// Jukeboox - pinagem da revisão de 22/11/2026.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <DFRobotDFPlayerMini.h>
#include "Painel.h"

constexpr int RFID_SS=5, RFID_SCK=18, RFID_MISO=19, RFID_MOSI=23, RFID_RST=22;
constexpr int ESP_RX=32, ESP_TX=21;
constexpr int VOLUME_MAX=20;
constexpr char REDE_WIFI[]="Jukeboox";
constexpr char SENHA_WIFI[]="Jukeboox-Configure"; // Altere antes de gravar.

struct Faixa { uint16_t numero; const char *titulo; };
struct Cadastro { const char *uid; const char *nome; const Faixa *faixas; size_t total; };
// Os números correspondem a /MP3/0001.mp3, /MP3/0002.mp3 etc.
const Faixa playlist[]={{1,"Abertura da noite"},{2,"Luzes da cidade"},{3,"Último refrão"}};
const Faixa avulsa[]={{4,"Um lado do disco"}};
const Cadastro musicas[]={
  {"SUBSTITUA_PELO_UID_1","Seleção da jukebox",playlist,sizeof(playlist)/sizeof(playlist[0])},
  {"SUBSTITUA_PELO_UID_2","Faixa avulsa",avulsa,sizeof(avulsa)/sizeof(avulsa[0])}
};

MFRC522 rfid(RFID_SS,RFID_RST);
HardwareSerial serialMP3(2);
DFRobotDFPlayerMini player;
WebServer web(80);
enum Estado { AGUARDANDO, TOCANDO, ENCERRADO, FALHA };
Estado estado=AGUARDANDO;
bool audioPronto=false, webPronta=false;
const Cadastro *selecao=nullptr;
size_t posicao=0;
int volume=10;
uint32_t revisao=0, ultimoRFID=0, inicioFaixa=0;
String candidata, ultimoUID;
uint8_t confirmacoes=0;

void falhar(const char *motivo) {
  if(estado==FALHA) return;
  estado=FALHA;
  ++revisao;
  if(audioPronto) player.stop();
  Serial.println(motivo);
}

bool lerUID(String &uid) {
  byte atqa[2], tamanho=sizeof(atqa);
  const auto status=rfid.PICC_WakeupA(atqa,&tamanho);
  if(status!=MFRC522::STATUS_OK && status!=MFRC522::STATUS_COLLISION) return false;
  if(!rfid.PICC_ReadCardSerial()) return false;
  uid="";
  for(byte i=0;i<rfid.uid.size;i++) {
    if(rfid.uid.uidByte[i]<16) uid+='0';
    uid+=String(rfid.uid.uidByte[i],HEX);
  }
  uid.toUpperCase();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}

void tocar(size_t indice) {
  // Ouça os avisos pela serial. O GPIO BUSY fica sem fio.
  posicao=indice;
  estado=TOCANDO;
  inicioFaixa=millis();
  ++revisao;
  player.playMp3Folder(selecao->faixas[indice].numero);
}

void atualizarRFID(uint32_t agora) {
  if(agora-ultimoRFID<50) return;
  ultimoRFID=agora;
  String uid;
  if(!lerUID(uid)) {
    candidata="";
    confirmacoes=0;
    return; // Ao retirar o disco, a faixa atual continua.
  }
  if(uid!=candidata) {
    candidata=uid;
    confirmacoes=1;
    Serial.print("UID: ");
    Serial.println(uid);
    return;
  }
  if(confirmacoes!=1) return;
  ++confirmacoes;
  ultimoUID=uid;
  for(const auto &entrada:musicas) {
    if(uid!=entrada.uid) continue;
    if(selecao!=&entrada) {
      selecao=&entrada;
      tocar(0);
    }
    return; // Reler o mesmo disco não reinicia a reprodução.
  }
}

void atualizarAudio() {
  while(player.available()) {
    const auto tipo=player.readType();
    const auto numero=player.read();
    if(tipo==DFPlayerCardRemoved || tipo==DFPlayerError) {
      Serial.printf("Evento DFPlayer %u, código %d\n",unsigned(tipo),int(numero));
      falhar("Confira o cartão e os arquivos do DFPlayer.");
      return;
    }
    if(tipo!=DFPlayerPlayFinished || estado!=TOCANDO || millis()-inicioFaixa<350) continue;
    if(numero!=selecao->faixas[posicao].numero) {
      Serial.printf("Fim UART de outra faixa: %d (esperava %u)\n",int(numero),unsigned(selecao->faixas[posicao].numero));
      continue;
    }
    if(posicao+1<selecao->total) tocar(posicao+1);
    else {
      estado=ENCERRADO;
      ++revisao;
    }
  }
}

void stringJson(String &json, const char *valor) {
  json+='"';
  for(;*valor;++valor) {
    const unsigned char c=*valor;
    if(c=='"' || c==92) json+=char(92);
    if(c>=32) json+=char(c);
  }
  json+='"';
}

void enviarEstado() {
  const char *nome=estado==TOCANDO?"playing":estado==ENCERRADO?"ended":estado==FALHA?"fault":"idle";
  String json="{\"status\":";
  stringJson(json,nome);
  json+=",\"uid\":";
  stringJson(json,ultimoUID.c_str());
  json+=",\"name\":";
  stringJson(json,selecao?selecao->nome:"");
  json+=",\"tracks\":[";
  if(selecao) {
    for(size_t i=0;i<selecao->total;++i) {
      if(i) json+=',';
      stringJson(json,selecao->faixas[i].titulo);
    }
  }
  json+="],\"index\":";
  json+=(estado==TOCANDO?int(posicao):-1);
  json+=",\"volume\":";
  json+=volume;
  json+=",\"max\":";
  json+=VOLUME_MAX;
  json+=",\"revision\":";
  json+=revisao;
  json+= "}";
  web.sendHeader("Cache-Control","no-store");
  web.send(200,"application/json",json);
}

void iniciarWeb() {
  webPronta=WiFi.softAP(REDE_WIFI,SENHA_WIFI);
  if(!webPronta) {
    Serial.println("Wi-Fi indisponível.");
    return;
  }
  web.on("/",HTTP_GET,[](){web.send_P(200,"text/html; charset=utf-8",PAINEL_HTML);});
  web.on("/api/status",HTTP_GET,enviarEstado);
  web.on("/api/volume",HTTP_POST,[](){
    if(estado==FALHA || !audioPronto) {web.send(503);return;}
    const String bruto=web.arg("value");
    if(bruto.isEmpty() || bruto.length()>2) {web.send(400);return;}
    for(size_t i=0;i<bruto.length();++i) {
      if(bruto[i]<'0' || bruto[i]>'9') {web.send(400);return;}
    }
    const int novo=bruto.toInt();
    if(novo>VOLUME_MAX) {web.send(400);return;}
    volume=novo;
    player.volume(volume);
    enviarEstado();
  });
  web.on("/api/next",HTTP_POST,[](){
    if(estado!=TOCANDO || posicao+1>=selecao->total || web.arg("revision")!=String(revisao)) {
      web.send(409);return;
    }
    tocar(posicao+1);
    enviarEstado();
  });
  web.on("/api/replay",HTTP_POST,[](){
    if(estado!=ENCERRADO || web.arg("revision")!=String(revisao)) {web.send(409);return;}
    tocar(0);
    enviarEstado();
  });
  web.begin();
  Serial.print("Painel: http://");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  SPI.begin(RFID_SCK,RFID_MISO,RFID_MOSI,RFID_SS);
  rfid.PCD_Init();
  const auto versao=rfid.PCD_ReadRegister(MFRC522::VersionReg);
  serialMP3.begin(9600,SERIAL_8N1,ESP_RX,ESP_TX);
  audioPronto=player.begin(serialMP3,false,true);
  player.setTimeOut(500);
  if(versao==0 || versao==255) falhar("RC522 sem comunicação.");
  else if(!audioPronto || player.readFileCounts(DFPLAYER_DEVICE_SD)<=0)
    falhar("DFPlayer sem resposta ou cartão sem faixas.");
  else player.volume(volume);
  iniciarWeb();
}

void loop() {
  if(estado!=FALHA) {
    atualizarAudio();
    if(estado!=FALHA) atualizarRFID(millis());
  }
  if(webPronta) web.handleClient();
  delay(1);
}

