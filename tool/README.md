# Windows用クライアント
Visual Studio 2022で作成した。
Hakadorun2_Client.slnを開いてビルド。

# Linux用クライアント

## 要るもの

Archlinux系
```bash
yay -S hidapi
```

Ubuntu
```bash
sudo apt install libhidapi-dev
```

## コンパイル
```bash
g++ -std=c++20 Hakadorun2_Client_Linux.cpp -lhidapi-libusb
```

## デバイスを取得できないとき
https://stackoverflow.com/questions/38867444/hidapi-hid-open-path-how-to-determine-which-path-to-use
