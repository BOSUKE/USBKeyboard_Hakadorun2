# Linux用クライアント

## 要るもの

Archlinux系
```bash
yay -S hidapi
```

## コンパイル
```bash
g++ -std=c++20 Hakadorun2_Client_Linux.cpp -lhidapi-libusb
```

## デバイスを取得できないとき
https://stackoverflow.com/questions/38867444/hidapi-hid-open-path-how-to-determine-which-path-to-use
