//////////////////////////////////////////////////////////////////////
/// @file EL.h
/// @brief ECHONET Lite protocol for Arduino
/// @author SUGIMURA Hiroshi
/// @date 2013.09.27
/// @details https://github.com/Hiroshi-Sugimura/EL_dev_arduino
//////////////////////////////////////////////////////////////////////
#ifndef __ECHONET_H__
#define __ECHONET_H__
#pragma once

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#ifdef __cplusplus
extern "C" {
#endif

// (1) ネット参加アナウンス：インスタンスリスト通知（INF, EPC=D5）をマルチキャスト
extern void el_init();

// (2) INF_REQ(EPC=D5) へのユニキャスト応答（EDTは逐次書き込み）
extern void el_loop();

// (3) 電源が変わった時の状態通知（対象EOJの EPC=0x80 を INF でマルチキャスト）
extern void el_notify_power_change(bool aircon //true=エアコン, false=照明
  , bool on);


#ifdef __cplusplus
}
#endif

#endif
