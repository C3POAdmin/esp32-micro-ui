# ESP32 Micro UI

Lightweight, modular UI engine for ESP32 display projects.

Designed for clean integration with display drivers such as TFT_eSPI and common touchscreen controllers like XPT2046. The core engine remains hardware-agnostic, with rendering handled by the application layer.

---

## ✨ Features

- Buttons with visual press feedback and callback support  
- Label-based callbacks for simple identification  
- Unique element IDs for efficient updates and tracking  
- Sliders with drag + full-track touch support  
- Flicker-free updates using sprite-based rendering  
- Touch state tracking with debounce logic  
- Progress bars and common drawing primitives  
- Designed for ESP32-class microcontrollers  

---

## 🧱 Architecture

- Hardware-agnostic core  
- No direct dependency on display drivers  
- Rendering implemented by the example or application layer  
- Clean separation between UI logic and hardware binding  

---

## 🚀 Example

See the example projects:

Tested on the popular Cheap Yellow Display (ESP32 + ILI9341 240x320).

