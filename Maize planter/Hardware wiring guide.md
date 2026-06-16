

To prevent the high-current draw of the wiper motors from causing a voltage drop that reboots your ESP32, you must adhere strictly to a **Star Ground** configuration. All grounds must meet at the battery negative terminal, not daisy-chained through the components.

**Table 1: Power & Data Lines**

| **Component**         | **Pin**     | **Connects To**    | **Notes**                       |
| --------------------- | ----------- | ------------------ | ------------------------------- |
| **Pixhawk (TELEM 2)** | TX          | ESP32 GPIO 4       | MAVLink Serial RX.              |
| **Pixhawk (TELEM 2)** | RX          | ESP32 GPIO 5       | MAVLink Serial TX.              |
| **Left BTS7960**      | RPWM / LPWM | ESP32 GPIO 12 & 13 | Forward / Reverse signals.      |
| **Left BTS7960**      | R_EN / L_EN | ESP32 3.3V (or 5V) | Tied HIGH to enable the driver. |
| **Right BTS7960**     | RPWM / LPWM | ESP32 GPIO 14 & 27 | Forward / Reverse signals.      |
| **Right BTS7960**     | R_EN / L_EN | ESP32 3.3V (or 5V) | Tied HIGH to enable the driver. |