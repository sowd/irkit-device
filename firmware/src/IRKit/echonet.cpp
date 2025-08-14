#include "echonet.h"
#include "log.h"

#define EL_PORT 3610
#define MCAST_A 224
#define MCAST_B 0
#define MCAST_C 23
#define MCAST_D 0

// EOJ 定数（配列を持たない）
#define SEOJ_NODE_CG 0x05
#define SEOJ_NODE_CC 0xFF
#define SEOJ_NODE_IC 0x01
#define DEOJ_ALL_CG  0x0E
#define DEOJ_ALL_CC  0xF0
#define DEOJ_ALL_IC  0x01

#define EOJ_AC_CG  0x01
#define EOJ_AC_CC  0x30
#define EOJ_AC_IC  0x01
#define EOJ_LT_CG  0x02
#define EOJ_LT_CC  0x90
#define EOJ_LT_IC  0x01

// ESV
#define ESV_INF      0x73
#define ESV_INF_REQ  0x63

// 電源 EPC 値
#define EPC_POWER    0x80
#define ON_0x30      0x30
#define OFF_0x31     0x31

// インスタンスリスト通知 EPC
#define EPC_INSTLIST 0xD5

static EthernetUDP udp;
//static IPAddress mcast(MCAST_A, MCAST_B, MCAST_C, MCAST_D);
static uint16_t TID = 1;

// 送信ヘッダを逐次書き込み（OPC=1固定）
static inline void begin1EPC(uint8_t se_cg,uint8_t se_cc,uint8_t se_ic,
                             uint8_t de_cg,uint8_t de_cc,uint8_t de_ic,
                             uint8_t esv, uint8_t epc, uint8_t pdc)
{
  udp.write((uint8_t)0x10); udp.write((uint8_t)0x81);       // EHD1,2
  udp.write((uint8_t)(TID>>8)); udp.write((uint8_t)TID);    // TID
  udp.write(se_cg); udp.write(se_cc); udp.write(se_ic);     // SEOJ
  udp.write(de_cg); udp.write(de_cc); udp.write(de_ic);     // DEOJ
  udp.write(esv);                                           // ESV
  udp.write((uint8_t)0x01);                                 // OPC=1
  udp.write(epc);                                           // EPC
  udp.write(pdc);                                           // PDC
}

// (1) ネット参加アナウンス：インスタンスリスト通知（INF, EPC=D5）をマルチキャスト
void el_init() {
  MAINLOG_PRINTLN("ECHONET Lite: join announce.");
  IPAddress mcast(MCAST_A, MCAST_B, MCAST_C, MCAST_D);

  udp.begin(EL_PORT);
  udp.beginPacket(mcast, EL_PORT);
  begin1EPC(SEOJ_NODE_CG,SEOJ_NODE_CC,SEOJ_NODE_IC,
            DEOJ_ALL_CG, DEOJ_ALL_CC, DEOJ_ALL_IC,
            ESV_INF, EPC_INSTLIST, 1 + 3*2); // 個数1 + EOJ×2
  udp.write((uint8_t)2);                  // 個数
  // EOJ #1: Aircon
  udp.write((uint8_t)EOJ_AC_CG); udp.write((uint8_t)EOJ_AC_CC); udp.write((uint8_t)EOJ_AC_IC);
  // EOJ #2: Lighting
  udp.write((uint8_t)EOJ_LT_CG); udp.write((uint8_t)EOJ_LT_CC); udp.write((uint8_t)EOJ_LT_IC);
  udp.endPacket();
  TID++;
}

// (2) INF_REQ(EPC=D5) へのユニキャスト応答（EDTは逐次書き込み）
void respond_instance_list_req(const uint8_t *buf, int len, IPAddress rip, uint16_t rport) {
  if (len < 14) return;
  if (buf[0]!=0x10 || buf[1]!=0x81) return;
  if (buf[10]!=ESV_INF_REQ) return;
  if (buf[11]!=0x01) return;        // OPC=1のみ対応
  if (buf[12]!=EPC_INSTLIST) return;

  //MAINLOG_PRINTLN("ECHONET Lite: INF_REQ(EPC=D5) received. Replying.");

  // 相手SEOJ（buf[4..6]）をDEOJにセットして返す
  udp.beginPacket(rip, rport);
  begin1EPC(SEOJ_NODE_CG,SEOJ_NODE_CC,SEOJ_NODE_IC,
            buf[4], buf[5], buf[6],
            ESV_INF, EPC_INSTLIST, 1 + 3*2);
  udp.write((uint8_t)2);
  udp.write((uint8_t)EOJ_AC_CG); udp.write((uint8_t)EOJ_AC_CC); udp.write((uint8_t)EOJ_AC_IC);
  udp.write((uint8_t)EOJ_LT_CG); udp.write((uint8_t)EOJ_LT_CC); udp.write((uint8_t)EOJ_LT_IC);
  udp.endPacket();
  TID++;
}

void el_loop(){
  if( TID==1 ) return ; // Not initialized
  int p = udp.parsePacket();
  if (p <= 0) return ;

  //uint8_t rxbuf[48];  // 14バイト超あれば十分（安全に48バイト）
  uint8_t rxbuf[15];
  int n = udp.read(rxbuf, (p < (int)sizeof(rxbuf)) ? p : (int)sizeof(rxbuf));
  respond_instance_list_req(rxbuf, n, udp.remoteIP(), udp.remotePort());

}

// (3) 電源が変わった時の状態通知（対象EOJの EPC=0x80 を INF でマルチキャスト）
void el_notify_power_change(bool aircon //true=エアコン, false=照明
  , bool on) {
  //MAINLOG_PRINTLN("ECHONET Lite: Announce power status change.");
  IPAddress mcast(MCAST_A, MCAST_B, MCAST_C, MCAST_D);

  udp.beginPacket(mcast, EL_PORT);
  if (aircon) {
    begin1EPC(EOJ_AC_CG,EOJ_AC_CC,EOJ_AC_IC, DEOJ_ALL_CG,DEOJ_ALL_CC,DEOJ_ALL_IC,
              ESV_INF, EPC_POWER, 1);
  } else {
    begin1EPC(EOJ_LT_CG,EOJ_LT_CC,EOJ_LT_IC, DEOJ_ALL_CG,DEOJ_ALL_CC,DEOJ_ALL_IC,
              ESV_INF, EPC_POWER, 1);
  }
  udp.write(on ? (uint8_t)ON_0x30 : (uint8_t)OFF_0x31);
  udp.endPacket();
  TID++;
}

/*

// ------- 受信処理（省メモリ：バッファ最小） -------

void setup() {
  // できるだけSRAMを節約：Serialは使わない
  byte mac[6] = {0xDE,0xAD,0xBE,0xEF,0xFE,0xED};
  IPAddress ip(192,168,1,123);
  Ethernet.begin(mac, ip);
  udp.begin(EL_PORT);
  announce_join();
}

void loop() {
  int p = udp.parsePacket();
  if (p > 0) {
    int n = udp.read(rxbuf, (p < (int)sizeof(rxbuf)) ? p : (int)sizeof(rxbuf));
    respond_instance_list_req(rxbuf, n, udp.remoteIP(), udp.remotePort());
  }

  // 例：何かのイベントで電源通知
  // notify_power_change( bIsAircon, bIsOn);
}
*/