#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#include "echonet.h"

// ==== ネットワーク設定 ====
//byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // 任意
//IPAddress ip(192, 168, 1, 123); // 固定IP
const IPAddress mcast(224, 0, 23, 0);
const uint16_t EL_PORT = 3610;

// EOJ 構造体
struct EOJ { uint8_t cg, cc, ic; };

const EOJ SEOJ_NODE = {0x05, 0xFF, 0x01};
const EOJ DEOJ_ALL  = {0x0E, 0xF0, 0x01};

const EOJ EOJ_AIRCON   = {0x01, 0x30, 0x01};
const EOJ EOJ_LIGHTING = {0x02, 0x90, 0x01};

const EOJ INST_LIST[] = { EOJ_AIRCON, EOJ_LIGHTING };
const uint8_t INST_COUNT = sizeof(INST_LIST) / sizeof(INST_LIST[0]);

enum { ESV_INF_REQ=0x63, ESV_INF=0x73 };
uint16_t TID = 1;

static EthernetUDP udp;

// === フレーム生成 ===
size_t build1EPC(uint8_t* buf, size_t cap, EOJ seoj, EOJ deoj,
                 uint8_t esv, uint8_t epc, const uint8_t* edt, uint8_t pdc) {
  if (cap < 14 + pdc) return 0;
  size_t i = 0;
  buf[i++] = 0x10; buf[i++] = 0x81;
  buf[i++] = (TID >> 8) & 0xFF; buf[i++] = TID & 0xFF;
  buf[i++] = seoj.cg; buf[i++] = seoj.cc; buf[i++] = seoj.ic;
  buf[i++] = deoj.cg; buf[i++] = deoj.cc; buf[i++] = deoj.ic;
  buf[i++] = esv;
  buf[i++] = 0x01;
  buf[i++] = epc;
  buf[i++] = pdc;
  for (uint8_t k = 0; k < pdc; k++) buf[i++] = edt[k];
  return i;
}

// (1) ネット参加アナウンス（一回のみ実行）
void el_announce_join() {
 udp.begin(EL_PORT); // W5100/5500はマルチキャストJOIN不要で受信可（LAN内全受信）


  uint8_t edt[1 + 3*8]; uint8_t k = 0;
  edt[k++] = INST_COUNT;
  for (uint8_t n=0; n<INST_COUNT; n++) {
    edt[k++] = INST_LIST[n].cg;
    edt[k++] = INST_LIST[n].cc;
    edt[k++] = INST_LIST[n].ic;
  }
  uint8_t pkt[64];
  size_t len = build1EPC(pkt, sizeof(pkt), SEOJ_NODE, DEOJ_ALL, ESV_INF, 0xD5, edt, k);
  udp.beginPacket(mcast, EL_PORT);
  udp.write(pkt, len);
  udp.endPacket();
  TID++;
}
/*

// (2) 電源変更通知
void notify_power_change(const EOJ& obj, bool on) {
  uint8_t val = on ? 0x30 : 0x31;
  uint8_t pkt[48];
  size_t len = build1EPC(pkt, sizeof(pkt), obj, DEOJ_ALL, ESV_INF, 0x80, &val, 1);
  udp.beginPacket(mcast, EL_PORT);
  udp.write(pkt, len);
  udp.endPacket();
  TID++;
}

// (3) INF_REQ(D5) に応答
void respond_instance_list_req(const uint8_t* buf, int len, IPAddress rip, uint16_t rport) {
  if (len < 14) return;
  if (buf[0]!=0x10 || buf[1]!=0x81) return;
  if (buf[10] != ESV_INF_REQ) return;
  if (buf[11] != 0x01) return;
  if (buf[12] != 0xD5) return;

  uint8_t edt[1 + 3*8]; uint8_t k = 0;
  edt[k++] = INST_COUNT;
  for (uint8_t n=0; n<INST_COUNT; n++) {
    edt[k++] = INST_LIST[n].cg;
    edt[k++] = INST_LIST[n].cc;
    edt[k++] = INST_LIST[n].ic;
  }
  EOJ req_src = { buf[4], buf[5], buf[6] };
  uint8_t pkt[64];
  size_t out = build1EPC(pkt, sizeof(pkt), SEOJ_NODE, req_src, ESV_INF, 0xD5, edt, k);
  udp.beginPacket(rip, rport);
  udp.write(pkt, out);
  udp.endPacket();
  TID++;
}
*/

/*
void setup() {
  Ethernet.begin(mac, ip);
  udp.begin(EL_PORT); // W5100/5500はマルチキャストJOIN不要で受信可（LAN内全受信）
  announce_join();
}

void loop() {
  int psize = udp.parsePacket();
  if (psize > 0) {
    uint8_t buf[256];
    int len = udp.read(buf, sizeof(buf));
    IPAddress rip = udp.remoteIP();
    uint16_t rport = udp.remotePort();
    respond_instance_list_req(buf, len, rip, rport);
  }
}
*/