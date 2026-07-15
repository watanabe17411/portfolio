import socket
import time
import sys

SERVER_IP = "127.0.0.1" # 自宅WSL2ならlocalhost、学校ならJava側のIP
SERVER_PORT = 8500      # Java側が待ち受けているポート番号

def send_to_java(device_name, data_type, data_value):
    try:
        # TCP/IPソケットの作成と接続
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            c.connect((SERVER_IP, SERVER_PORT))

            # JavaのreadLine()が認識できるように、末尾に必ず「\n」を付与
            message = f"{device_name},{data_type},{data_value}\n"

            # 文字列をバイト列に変換して送信
            s.sendall(message.encode('utf-8'))
            print(f"[送信成功] -> {message.strip()}")
        except Exception as e:
            print(f"[送信失敗] Javaサーバーに接続できません: {e}")

# --- メイン処理 ---
try:
    # 本物のデバイス(PN532)のライブラリ読み込みを試みる
    from adafruit_pn532.spi import PN532_SPI
    import board
    import busio
    from digitalio import DigitalInOut

    print("--- [実機モード] PN532を検知しました。カードをかざしてください ---")
    "ハードウェアの初期化設定(SPI通信)
    spi = busio.SPI(board.SCK, board.MOSI, board.MISO)
    cs = DigitalInOut(board.D5)
    pn532 = PN532_SPI(spi, cs)
    pn532.begin()
    pn532.SAM_configuration()

    while True:
        # カード(またはスマホ)がターゲットに入ったか監視
        uid = pn532.read_passive_target(timeout=0.5)
        if uid is not None:
            # 読み取った固有ID(英数字の並び)を文字列に変換
            uid_str = "",join([j"{x:02X}" for x in uid])
            # Javaサーバーへ送信(デバイス名、データ種別、カードID)
            send_to_java("RasPi_NFC_Reader", "RFID_UID", uid_str)
            time.sleep(2) # 連続送信を防ぐためのウェイト

except ImportError:
    # ライブラリがない（PC環境や実機がない）場合は、自動でシミュレーターが起動
    print("--- [シミュレーターモード] 実機がないため、手入力テストを開始します ---")
    print("※キーボードでダミーのカードIDを入力してEnterを押してください(終了は Ctrl+C) ")

    while True:
        try:
            dummy_uid = input("ダミーのカードIDを入力してください: ")
            if dummy_uid.strip():
                # 入力された文字を、実機の代わりにJavaサーバーへ送信
                send_to_java("Simulated_Reader", "RFID_UID", dummy_uid)
            except KeyboardInterrupt:
                print("\nシミュレータを終了します。")
                sys.exit()
