#!/usr/bin/env bash
set -euo pipefail
LC_ALL=C

# --- 前提コマンド確認 ---
for cmd in ip nmap awk; do
  command -v "$cmd" >/dev/null 2>&1 || {
    echo "ERROR: '$cmd' コマンドが見つかりません。" >&2
    exit 1
  }
done

# --- デフォルト経路に使われるインターフェースを特定 ---
# 8.8.8.8宛のルートから dev 名と src を拾う（VPN等があっても実運用に近い系統を選ぶ）
default_dev="$(ip route get 8.8.8.8 2>/dev/null | awk '{for(i=1;i<=NF;i++) if($i=="dev"){print $(i+1); exit}}')"
if [[ -z "${default_dev:-}" ]]; then
  echo "ERROR: デフォルト経路のインターフェースを特定できませんでした。" >&2
  exit 1
fi

# --- そのインターフェースのIPv4 CIDRを取得（例: 192.168.179.27/24）---
ip_cidr="$(ip -o -4 addr show dev "$default_dev" primary scope global | awk '{print $4}' | head -n1)"
if [[ -z "${ip_cidr:-}" ]]; then
  echo "ERROR: インターフェース '$default_dev' のIPv4アドレスが見つかりませんでした。" >&2
  exit 1
fi

# 注意: nmap は CIDR 指定を受け付けるので、ネットワークアドレス計算は不要
# 例) 192.168.179.27/24 -> その/24全体をスキャン
echo "Scanning subnet: $ip_cidr (dev: $default_dev)" >&2

# --- nmap 実行して IRKit*.lan を抽出 ---
# 出力形式にできるだけ頑健に対応:
#   Nmap scan report for IRKitXXXX.lan (192.168.179.x)
#   Host is up ...
#   MAC Address: XX:XX:... (Vendor)
#
# 期待する出力:
#   IRKit*.lan : [MAC] : [IP]
sudo nmap -sn "$ip_cidr" 2>/dev/null | awk '
  BEGIN{
    # フラグや保持変数初期化
    is_irkit=0; name=""; ip=""; mac="";
  }
  # ホスト開始行を検知
  /^Nmap scan report for / {
    is_irkit=0; name=""; ip=""; mac="";
    # パターン1: Nmap scan report for NAME (IP)
    if (match($0, /^Nmap scan report for (.+) \(([0-9.]+)\)$/, m)) {
      name=m[1]; ip=m[2];
    } else if (match($0, /^Nmap scan report for ([0-9.]+)$/, m2)) {
      # パターン2: Nmap scan report for 192.168.179.10
      name=""; ip=m2[1];
    }
    # IRKit*.lan のみ対象
    if (name ~ /^IRKit.*\.lan$/) {
      is_irkit=1;
    }
    next;
  }

  # MACアドレス行
  /^MAC Address:/ {
    if (is_irkit) {
      # "MAC Address: XX:XX:... (Vendor)" の3番目がMAC
      mac=$3;
      printf("%s : %s : %s\n", name, mac, ip);
      # 1ホスト分出力したのでリセット（次のホストに備える）
      is_irkit=0; name=""; ip=""; mac="";
    }
    next;
  }
' | {
  # 何もヒットしない場合のケア
  read -r first || {
    echo "IRKit*.lan は見つかりませんでした。" >&2
    exit 0
  }
  # 1行目は取得済みなので表示して、残りをそのまま流す
  echo "$first"
  cat
}

