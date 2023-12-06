# [WLED](https://github.com/Aircoookie/WLED) usermod for Wemos D1 mini OLED 0.66" shield
Easy control over your [WLED](https://github.com/Aircoookie/WLED) instance with 64x48 screen and two buttons! 

## Features
  * 7 info screens

   ![info-screens](/img/info.gif "Info screens")

  * 9 actions available through menu

   ![menu](/img/menu.gif "Menu")

  * Two animated screensavers + power save mode

   ![night-sky](/img/nightsky.gif "Night sky") ![clock](/img/clock.gif "Clock")
   
  * Auto brightness: low on idle and high when active

## Usage
See [guide](/guide.md)
  
## Build
Follow the steps below to add this usermod to your WLED build
  1. Move *usermod_v2_wemos_oled* to _WLED_ROOT/usermods_
  2. Define `USERMOD_ID_WEMOS_OLED` in _WLED_ROOT/wled00/const.h_
  ```c++
// ...

#define USERMOD_ID_SHT                   39     //Usermod "usermod_sht.h
#define USERMOD_ID_KLIPPER               40     //Usermod Klipper percentage
#define USERMOD_ID_WIREGUARD             41     //Usermod "wireguard.h"
#define USERMOD_ID_INTERNAL_TEMPERATURE  42     //Usermod "usermod_internal_temperature.h"
#define USERMOD_ID_WEMOS_OLED           100     //Usermod "wemos_oled.h"
  
// ...
  ```
  3. Register usermod in _WLED_ROOT/wled00/usermod_list.cpp_
  ```c++
// ...

#ifdef USERMOD_PWM_OUTPUTS
#include "../usermods/pwm_outputs/usermod_pwm_outputs.h"
#endif

#include "../usermods/usermod_v2_wemos_oled/wemos_oled.h"

void registerUsermods()
{
    // ...

    #ifdef USERMOD_INTERNAL_TEMPERATURE
    usermods.add(new InternalTemperatureUsermod());
    #endif

    usermods.add(new WemosOledUsermod());
}
```
That's it!

> [!TIP]
> A button debounce timeout `WemosOledUsermod::BTN_TIMEOUT` is set to 0.35 s. because I use specific buttons with very long push. If you use standard tactile buttons and want them to act faster decrease this value to 0.05 - 0.1 s.

