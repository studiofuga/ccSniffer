# ccSniffer
A sniffer for cc1101 chip

This project implements a basic Radio Sniffer based on the TI's cc1101 transceiver.

The firmware runs on a [NanoCUL](https://www.nanocul.de/) device, an arduino NANO 328p cpu board with a cc1101 module.

Actually it outputs any received packets in a machine readable format:

```text
*913936,DCC501A15D930540ADBD56E352456EBC
*2049626,EE9BD235DFB674B10B34AE7E9BFEE3146D2CCC729C24ACB18CBD
+CC1101 Timeout
+CC1101 Timeout
*3053645,22C455136615A04E86D4
*3128774,8265175DA85B79D5A1768A25A89C3BE48C84B20F7D0A1D6C7AE80A06BFF9ABAD31003EDC1BF0399C8B53CB31C0
*3569757,1000004141414241424338
```

# Credits

Part of the code is based on [RadioLib](https://github.com/jgromes/RadioLib)
by Jan Gromeš