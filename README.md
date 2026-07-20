<details>
<summary><b>【全体構成図】物理層(C言語)からWeb層(Java)までのデータ同期経路を表示</b></summary>

```mermaid
flowchart TD
    classDef raspi fill:#8B0000,stroke:#333,stroke-width:2px,color:#fff;
    classDef win fill:#1E3A8A,stroke:#333,stroke-width:2px,color:#fff;
    classDef ubuntu fill:#D97706,stroke:#333,stroke-width:2px,color:#fff;

    subgraph Layer1 [【ハードウェア / C言語層】 Raspberry Pi 4]
        A[スマホ / MIFAREカード] -- NFC電波: 13.56MHz --> B[MFRC522 リーダー]
        B -- SPI通信: spidev0.0 <br>「100kHz / usleep 200」で安定化 --> C[C-Engine: rfid_system]
    end

    subgraph Layer2 [【ネットワーク中継層】 Windows Host]
        C -- C/O2ソケット通信 --> D[VirtualBox Bridged Network]
        D -- NAT / ポートフォワーディング <br>「Host:5000 -> Guest:5000」 --> E[Ubuntu 仮想環境]
    end

    subgraph Layer3 [【バックエンド / Webアプリケーション層】 Ubuntu 10.0.2.15]
        E --> F[Java Receiver.java <br>「Port 5000 待機」]
        F -- 8文字HEX UID抽出 --> G[(PostgreSQL <br> portfolio_db)]
        
        H[Webブラウザ <br> view画面] -- HTTPリクエスト --> I[Tomcat 10.1 / Servlet]
        I -- LEFT JOIN <br>「iot_log_data ✖ product_master」 --> G
        G -- 製品名マッピングされたログ --> H
    end
```
</details>

# Multi-Layer IoT RFID Logging & Mapping System
本プロダクトは、ICカードリーダー（Raspberry Pi 4 / C言語）からデータを受け取るサーバー（Java）、データを保存するデータベース（PostgreSQL）、そしてブラウザに表示するWeb画面（Tomcat / サーブレット）までを、すべて1人で繋ぎ合わせて自作したフルスタックのIoTシステムです。単にデータを横流しするだけのシステムではありません。「センサーがデータを読み取れない」「ノイズでデータが溢れかえる」といった、ハードウェアとWebシステムの間で発生する実務的な通信バグを自力で波形・ログから突き止め、ビット単位のレジスタ制御で解決しています。

[1. 物理・ハードウェア層]
ICカードをかざしてから、Web画面に商品名が表示されるまでのデータの流れです。

* **1. 物理・ハードウェア層（データの検知）**
  * 端末・言語： Raspberry Pi 4（C言語）
  * 処理内容： ICカードリーダー（MFRC522）を制御し、カードの固有番号（UID：8文字の英数字）を検知します。
  * 通信： ネットワーク中継（ポート5000 / ソケット通信）でサーバーへ送信します。
* **2. アプリケーション層（データの受け取り）**
  * 環境・言語： 仮想環境（Ubuntu）上のJavaプログラム
  * 処理内容： 送られてきたデータから、余計なノイズを削ぎ落として綺麗な「8文字のUID」だけを抽出します。
  * 通信： データベースへ保存（JDBC経由でINSERT）します。
* **3. データベース層（データの蓄積・紐付け）**
  * システム： PostgreSQL
  * 処理内容： 読み取った履歴（生ログ）と、あらかじめ登録してある「商品マスタ（商品名データ）」をリアルタイムに結合します。
* **4. Web画面・描画層（データの見える化）**
  * システム： Apache Tomcat（Javaサーブレット）
  * URL： http://localhost:8080/portfolio/view
  * 処理内容： データベースで紐付けたデータを読み込み、ただの英数字だったUIDを「iPhone 15 Pro」などの実際の商品名に変えて画面にリアルタイム表示します。

---

## 技術スタック & 稼働環境

<details>
<summary><b>【使用技術一覧】Raspberry Pi4 / C言語 / Java17 / Tomcat / PostgreSQL</b></summary>

### ハードウェア / 低レイヤ
- **デバイス**: Raspberry Pi 4 Model B
- **RFIDモジュール**: MFRC522 (MIFARE規格 / ISO14443A)
- **言語/制御**: C言語 (GCC -O2 最適化コンパイル) / Linux標準 `spidev` および `ioctl`
- **サービス常駐化**: `systemd` による起動時自動常駐化 (自己修復・自動再接続ループ実装)

### バックエンド / インフラ
- **OS**: VirtualBox 7.x ➔ Ubuntu Server
- **Web/サーブレットコンテナ**: Apache Tomcat 10.1.57 (Jakarta EE 仕様)
- **言語**: Java 17 (Socket / ServerSocket / JDBC / Servlet)
- **データベース**: PostgreSQL 15 (リレーショナル・マスタ共有スキーマ設計)

</details>

---

## 開発時に苦労したバグと、その解決実績

開発中、ハードウェア（C言語）とWeb（Java/DB）の処理速度のギャップにより、「ICカードを誤検知する」「ゴミデータが大量発生する」という通信の崩壊（ノイズ洪水）に直面しました。これらを既存ライブラリに頼らず、ICチップの仕様書（データシート）を読み解き、ビット単位のレジスタ制御で解決しました。
1. カードリーダーの空振りとエラーの解消（物理層のタイミング調整）問題：C言語の処理が早すぎたため、カードリーダーが電波を出してデータを準備し終える前に次の命令を送ってしまい、読み取りエラー（パリティエラー）や空振りが連発しました。解決：命令送信の直後に usleep(200);（100万分の200秒の待機時間）を意図的に挿入しました。機械側の処理を待つ「猶予」を作ったことで、カードの検知率を100%に引き上げました。
2. ノイズによるデータ大洪水のストップ（レジスタの初期化バグ）問題：カードの初期検知に必要な特殊パケット（7ビットデータ）をループ処理する際、機器のメモリ（レジスタ）に端数フラグが残ったまま次の通信に突入し、データが1ビットずつズレて大量のゴミデータ（ノイズ）が溢れ出るバグが発生しました。解決：通信のたびにレジスタを明示的に初期化（0x07 と 0x00 を切り替え制御）するコードを実装しました。これにより、浮遊ノイズをカードデータと誤認する挙動を構造から完全にシャットアウトしました。
3. 商品名が画面に連動しないバグの修正（データクレンジング）問題：Javaサーバー側で受け取ったデータにタイムスタンプ等の不要な情報が混入し、マスタ情報と照合できず表示エラーとなる現象が発生。解決：Java側でデータの文字列処理（スプリット・クレンジング）を行い、純粋なカードUIDのみを抽出するロジックを実装。これにより、マスタデータと正確にリアルタイム連動する仕組みを構築しました。

---

## 導入・実行手順

<details>
<summary><b>### 1. データベースセットアップ</b></summary>

```sql
CREATE DATABASE portfolio_db;
-- ログテーブル
CREATE TABLE iot_log_data (
    id SERIAL PRIMARY KEY,
    device_name VARCHAR(50),
    data_type VARCHAR(50),
    data_value VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
-- 商品マスタテーブル
CREATE TABLE product_master (
    rfid_uid VARCHAR(50) PRIMARY KEY,
    product_name VARCHAR(100)
);
```
</details>
<details>
<summary><b>### 2. Raspberry Pi 4（Cエンジン）のビルド・起動</b></summary>

```bash
gcc -O2 -Wall -o rfid_system main.c
sudo systemctl enable rfid.service
sudo systemctl start rfid.service
```
</details>
<details>
<summary><b>### 3. Javaレシーバ & Tomcatの稼働</b></summary>
`db.properties` を配置後、レシーバソケットをバックグラウンドで起動させ、Tomcatへサーブレットをホットデプロイします。

```bash
javac Receiver.java && java -cp .:db.jar Receiver
```
</details>

