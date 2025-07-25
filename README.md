
# DWIN UART Display Interface Library for STM32

# DWIN STM32 DGUS Project

[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://alexantony13.github.io/dwin-stm32/)
![Platform](https://img.shields.io/badge/platform-STM32-blue)
![DWIN DGUS](https://img.shields.io/badge/HMI-DWIN%20DGUS-green)

![Repo Size](https://img.shields.io/github/repo-size/alexantony13/dwin-stm32)
![Code Size](https://img.shields.io/github/languages/code-size/alexantony13/dwin-stm32)
![License](https://img.shields.io/github/license/alexantony13/dwin-stm32)

![Last Commit](https://img.shields.io/github/last-commit/alexantony13/dwin-stm32)
![Issues](https://img.shields.io/github/issues/alexantony13/dwin-stm32)
![Pull Requests](https://img.shields.io/github/issues-pr/alexantony13/dwin-stm32)

This repository provides a C library and accompanying resources to facilitate communication between STM32 microcontrollers and DWIN DGUS-II UART displays.

## ✨ Features

- **Modular Design**: Easy integration into existing STM32 projects.
- **Advanced STM32 UART**: Uses DMA transfer and Idle Line Detection of STM32 uart peripheral.
- **API for display update callbacks**: Uses callbacks instead of polling display VP addresses.
- **Example Project**: Ready-to-use STM32CubeIDE project to kickstart development.
    - MCU: STM32L431VCT6
    - Display: 7" COB UART Touch Panel (DMG10600T070_09WTC)
- **UI Tools**: DGUS and GIMP project files for customizing display interfaces.

## 🛠 Requirements

- **Hardware**:
  - STM32 microcontroller development board.
  - DWIN DGUS-II UART display.
  - ST-Link Programmer for flashing the microcontroller.
  - USB-to-UART TTL converter or SD card for updating the display.

- **Software**:
  - [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
  - [DWIN DGUS Tool](https://www.dwin-global.com/product/DGUS_V7.6/) 
  - [GIMP](https://www.gimp.org/) or suitable image editor.

## 🚀 Getting Started

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/alexantony13/dwin-stm32.git
   ```

2. **Open Example Project**:
   - Open STM32CubeIDE.
   - The exapmle project is located at `exapmles/STM32CubeIDE/dwin-stm32-testing`.
   - Open the example project using `File > Open Projects from File System...` menu option.
     - Note: The library is added as a linked source folder.
   - Open `dwin-stm32-testing.ioc` file and review the UART, Clock and GPIO settings.

3. **Build and Flash**:
   - Compile and flash the firmware to your STM32 board.

4. **Load DGUS Project**:
   - Open `DWIN DGUS Tool`.
   - Open the example project: `examples\DGUS\dwin-stm32-test`.
   - Upload to the display

5. **Connect display and STM32 uart pins**:
   - Connect STM32 UART Tx to Display Rx
   - Connect STM32 UART Rx to Display Tx
   - Connect STM32 GND to display GND
   - Use level shifters if needed. (If stm32 uart pins are not 5v tolerant.)

## ✅ TODO

- [x] API for writing data to VP addresses.
- [x] API for registering display event callbacks.
- [x] API for reading data from VP addresses.
- [ ] Implement CRC handling.
- [ ] Add utility functions for display manipulation.


## 📄 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Feel free to fork the repository and open a pull request with improvements or bug fixes.

## 📞 Contact

For questions or support, please open an issue in the [GitHub Issues](https://github.com/alexantony13/dwin-stm32/issues) section.

---

*Happy developing with DWIN and STM32!*
