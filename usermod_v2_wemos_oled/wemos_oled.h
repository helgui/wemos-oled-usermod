#pragma once

#include "wled.h"
#include <U8g2lib.h>

/*
    Display vertical layout:
    - Info screens:
        0..8: top icon bar
        9..17: first text line
        18: blank
        19..27: second text line
        28: blank
        29..37: third text line
        38: blank
        39..47: fourth text line
    - Splash and menu screens:
        0..35 picture
        39..47 caption
*/

class WemosOledUsermod : public Usermod {
private:
    using WO = WemosOledUsermod;

    static constexpr unsigned long BTN_TIMEOUT = 350;            // button debounce timeout
    static constexpr unsigned long MENU_EXIT_TIMEOUT = 30000;    // quit menu after 30 sec of inactivity
    static constexpr unsigned long SCREENSAVER_TIMEOUT = 120000; // enable screensaver mode after 2 min of inactivity
    static constexpr unsigned long HIGHLIGHT_TIMEOUT = 10000;    // set min contrast after 10 sec of inactivity

    static constexpr const char* DAY_NAME[7] = {
        "SUNDAY", "MONDAY", "TUESDAY",
        "WEDNESDAY", "THURSDAY",
        "FRIDAY", "SATURDAY"
    };

    enum WifiMode : uint8_t {
        AP,
        CLIENT,
        NONE
    };

    enum Screen : uint8_t {
        WIFI = 0,
        LED = 1,
        FX = 2,
        TECH_INFO = 3,
        TIME_AND_DATE = 4,
        DISPLAY_INFO = 5,
        ABOUT = 6,
        MENU_POWER = 127,
        MENU_COLOR = 128,
        MENU_AP = 129,
        MENU_REBOOT = 130,
        MENU_FACTORY_RESET = 131,
        MENU_NEXT_EFFECT = 132,
        MENU_BRI_PLUS = 133,
        MENU_BRI_MINUS = 134,
        MENU_SCREENSAVER = 135,
        MENU_EXIT = 136,
        SCREENSAVER_NIGHTSKY = 251,
        SCREENSAVER_CLOCK = 252,
        SCREENSAVER_EMPTY = 253,
        NOTHING = 254,
        SPLASH = 255
    };

    U8G2_SSD1306_64X48_ER_F_HW_I2C display;
    
    unsigned long lastUpdate;      // timepoint(ms) of latest render
    unsigned long lastActionPress; // timepoint(ms) of latest action button press
    unsigned long lastMenuPress;   // timepoint(ms) of latest menu button press
    unsigned long lastWokeUp;      // timepoint(ms) of last `wakeUp` call
    
    bool enabled;            
    uint8_t lowContrast;     // idle contrast
    uint8_t highContrast;    // in-use contrast
    
    bool ready;              // is display HW ready to communicate
    bool redraw;             // force redraw flag
    bool menu;               // menu opened
    bool screenSaving;       // is display in ss mode
    bool highlighting;       // is display highlighted
    bool ssClockMoveForward; // flag for animation in clock screensaver

    WO::Screen activeScreen;   // screen to render
    WO::Screen renderedScreen; // screen that's actually rendered
    WO::Screen screenSaver;    // screensaver type: empty, clock or night sky
    WO::WifiMode wifiState;    // current wifi mode
    uint8_t animationFrame;   // used by splash screen and clock screensaver

    /* Utility functions */

    // update rate in ms for current mode/screen
    unsigned long getUpdateRate() const {
        if (screenSaving) {
            return 1000;
        }
        if (activeScreen == WO::Screen::SPLASH) return 500;
        if (activeScreen == WO::Screen::TIME_AND_DATE) return 1000;
        if (activeScreen == WO::Screen::LED ||
            activeScreen == WO::Screen::FX) return 3000;
        if (activeScreen == WO::Screen::ABOUT) return 30000;
        return 10000;
    }

    // returns whether update is neccessary
    bool isRedrawNeeded() const {
        return redraw || // force redraw
            renderedScreen != activeScreen || // not synced
            (millis() - lastUpdate >= getUpdateRate());
    }

    // the same as previous but for screensaver mode
    bool isScreensaverRedrawNeeded() const {
        return renderedScreen != screenSaver || // not displayed yet
            (millis() - lastUpdate >= getUpdateRate());
    }

    // timepoint in ms of the last button press/wake up
    unsigned long mostRecentAction() const {
        return max({
            lastActionPress,
            lastMenuPress,
            lastWokeUp
        });
    }

    /*  Display logic  */

    // activate display
    void enable() {
        display.setPowerSave(0);
        display.clearDisplay();
    }

    // deactivate display
    void disable() {
        display.setPowerSave(1);
    }

    // enable display highlighting
    void highlight() {
        if (highlighting) return;
        highlighting = true;
        display.setContrast(highContrast);
    }

    // disable display higlighting
    void setIdle() {
        if (highlighting) {
            highlighting = false;
            display.setContrast(lowContrast);
        }
    }

    // exit screensaver mode if needed and enable highlighting
    bool wakeUp() {
        highlight();
        lastWokeUp = millis();
        if (screenSaving) {
            if (renderedScreen == WO::Screen::SCREENSAVER_EMPTY) {
                enable();
            }
            screenSaving = false;
            redraw = true;
            return true;
        }
        return false;
    }

    // select screen/action in a round robin manner
    void nextScreen() {
        redraw = true;
        if (activeScreen == WO::Screen::ABOUT) {
            activeScreen = WO::Screen::WIFI;
            return;
        }
        if (activeScreen == WO::Screen::MENU_EXIT) {
            activeScreen = WO::Screen::MENU_POWER; 
            return;
        }
        activeScreen = WO::Screen(activeScreen + 1);
    }

    // open `actions` menu
    void enterMenu() {
        // no need to call `wakeUp` here
        // as we did it in button handler
        activeScreen = WO::Screen::MENU_POWER;
        menu = true;
        redraw = true;
    }

    // return to info screen
    void exitMenu() {
        // no need to call `wakeUp` here
        // as we did it in button handler
        activeScreen = WO::Screen::WIFI;
        menu = false;
        redraw = true;
    }

    // execute current selected action
    void executeAction() {
        if (!menu) return;
        if (activeScreen == WO::Screen::MENU_REBOOT) {
            exitMenu();
            disable(); // disable display before reboot
            doReboot = true;
            return;
        }
        if (activeScreen == WO::Screen::MENU_AP) {
            exitMenu();
            disable();
            WLED_FS.format();
            #ifdef WLED_ADD_EEPROM_SUPPORT
            clearEEPROM();
            #endif
            doReboot = true;
            return;
        }
        if (activeScreen == WO::Screen::MENU_POWER) {
            toggleOnOff();
            stateUpdated(CALL_MODE_BUTTON);
        }
        if (activeScreen == WO::Screen::MENU_AP) {
            WLED::instance().initAP(true);
        }
        if (activeScreen == WO::Screen::MENU_NEXT_EFFECT) {
            if (effectCurrent == strip.getModeCount() - 1) {
                effectCurrent = 0;
            } else {
                ++effectCurrent;
            }
            stateChanged = true;
            colorUpdated(CALL_MODE_BUTTON);
        }
        if (activeScreen == WO::Screen::MENU_BRI_MINUS) {
            if (bri >= 8) {
                bri -= 8;
                stateUpdated(CALL_MODE_BUTTON);
            } else if (bri > 1) {
                bri = 1;
                stateUpdated(CALL_MODE_BUTTON);
            }
        }
        if (activeScreen == WO::Screen::MENU_BRI_PLUS) {
            if (bri <= 247) { 
                bri += 8;
                stateUpdated(CALL_MODE_BUTTON);
            } else if (bri < 255) {
                bri = 255;
                stateUpdated(CALL_MODE_BUTTON);
            }
        }
        if (activeScreen == WO::Screen::MENU_COLOR) {
            setRandomColor(col);
            colorUpdated(CALL_MODE_BUTTON);
        }
        if (activeScreen == WO::Screen::MENU_SCREENSAVER) {
            exitMenu();
            setIdle(); // disable highlighting as it's guaranteedly enabled atm
            screenSaving = true;
            return;
        }
        exitMenu();
    }

    /* Drawing functions */
    
    // initialize drawing
    // should called before any drawing routine
    void startDrawing(bool showIcons = true) {
        display.clearBuffer();
        if (showIcons) drawIcons(8);
    }

    // end drawing and send image to display
    void show() {
        if (screenSaving) {
            renderedScreen = screenSaver;
        } else {
            renderedScreen = activeScreen;
        }
        display.sendBuffer();
        redraw = false;
        lastUpdate = millis();
    }

    // draw text in specified line starting from `x`
    void drawLine(u8g2_uint_t lineIdx, const char* text, u8g2_uint_t x = 0) {
        display.drawStr(x, 7 + 10 * lineIdx, text);
    }

    // draw top bar
    void drawIcons(int y) {
        display.setFont(u8g2_font_open_iconic_all_1x_t);
        display.drawGlyph(1, y, 248); // wifi
        display.drawGlyph(10, y, 259); // sun
        display.drawGlyph(19, y, 211); // play
        display.drawGlyph(28, y, 129); // tech
        display.drawGlyph(37, y, 123); // clock
        display.drawGlyph(46, y, 222); // display
        display.drawGlyph(55, y, 188); // info
        display.drawFrame(9 * activeScreen, y - 8, 10, 10);
    }

    void drawDisplayInfo() {
        display.setFont(u8g2_font_profont10_tr);
        
        drawLine(1, "MIN CTR:");
        display.setCursor(40, 17);
        display.print(lowContrast);
        
        drawLine(2, "MAX CTR:");
        display.setCursor(40, 27);
        display.print(highContrast);
        
        drawLine(3, "SCREENSAVER:");
        if (screenSaver == WO::Screen::SCREENSAVER_NIGHTSKY) {
            drawLine(4, "NIGHT SKY");
            return;
        }
        if (screenSaver == WO::Screen::SCREENSAVER_EMPTY) {
            drawLine(4, "EMPTY SCREEN");
            return;
        }
        drawLine(4, "CLOCK");
    }

    void drawMenuItem() {
        display.setFont(u8g2_font_profont10_tr);
        
        if (activeScreen == WO::Screen::MENU_POWER) {
            drawLine(4, "POWER ON/OFF", 2);
            display.setFont(u8g2_font_open_iconic_embedded_4x_t);
            display.drawGlyph(18, 35, 78);
        }
        
        if (activeScreen == WO::Screen::MENU_REBOOT) {
            drawLine(4, "REBOOT", 17);
            display.setFont(u8g2_font_open_iconic_embedded_4x_t);
            display.drawGlyph(16, 35, 79);
        }

        if (activeScreen == WO::Screen::MENU_FACTORY_RESET) {
            drawLine(4, "FACTORY RST", 5);
            display.setFont(u8g2_font_open_iconic_embedded_4x_t);
            display.drawGlyph(18, 35, 71);
        }

        if (activeScreen == WO::Screen::MENU_AP) {
            drawLine(4, "START AP", 12);
            display.setFont(u8g2_font_open_iconic_www_4x_t);
            display.drawGlyph(18, 35, 81);
        }

        if (activeScreen == WO::Screen::MENU_COLOR) {
            drawLine(4, "RANDOM COLOR", 2);
            display.setFont(u8g2_font_open_iconic_thing_4x_t);
            display.drawGlyph(16, 35, 71);
        }

        if (activeScreen == WO::Screen::MENU_NEXT_EFFECT) {
            drawLine(4, "NEXT EFFECT", 5);
            display.setFont(u8g2_font_open_iconic_play_4x_t);
            display.drawGlyph(16, 35, 72);   
        }

        if (activeScreen == WO::Screen::MENU_BRI_PLUS) {
            drawLine(4, "+ BRIGHTNESS", 2);
            display.setFont(u8g2_font_open_iconic_text_4x_t);
            display.drawGlyph(16, 35, 88); 
        }

        if (activeScreen == WO::Screen::MENU_BRI_MINUS) {
            drawLine(4, "- BRIGHTNESS", 2);
            display.setFont(u8g2_font_open_iconic_text_4x_t);
            display.drawGlyph(16, 35, 87); 
        }

        if (activeScreen == WO::Screen::MENU_SCREENSAVER) {
            drawLine(4, "SCREENSAVER", 5);
            display.setFont(u8g2_font_open_iconic_mime_4x_t);
            display.drawGlyph(16, 35, 68);    
        }

        if (activeScreen == WO::Screen::MENU_EXIT) {
            drawLine(4, "EXIT MENU", 10);
            display.setFont(u8g2_font_open_iconic_gui_4x_t);
            display.drawGlyph(16, 35, 65);
        }
    }

    // draw animation splash screen
    void drawSplash() {
        display.setFont(u8g2_font_open_iconic_www_4x_t);
        display.drawGlyph(16, 35, 72);
        display.setFont(u8g2_font_profont10_tr);
        drawLine(4, "LOADING", 8);
        if (animationFrame > 0) {
            display.drawGlyph(43, 47, '.');
        }
        if (animationFrame > 1) {
            display.drawGlyph(48, 47, '.');
        }
        if (animationFrame > 2) {
            display.drawGlyph(53, 47, '.');
        }
        if (animationFrame == 3) {
            animationFrame = 0;
        } else {
            ++animationFrame;
        }
    }

    // draw wifi data
    // mode, ssid, ip and signal (password in AP mode)
    void drawWifiData() {
        display.setFont(u8g2_font_profont10_tr);
        drawLine(1, "MODE:");
        if (wifiState == WO::WifiMode::AP) { // AP
            drawLine(1, "AP", 25);
            drawLine(2, apSSID);
            drawLine(4, "PWD:");
            drawLine(4, apPass, 20);
            
            // numeric
            display.setFont(u8g2_font_profont10_tn);
            drawLine(3, "4.3.2.1");
            return;
        }
        if (wifiState == WO::WifiMode::CLIENT) { // Client
            drawLine(1, "CLIENT", 25);
            drawLine(2, WiFi.SSID().c_str());
            drawLine(4, "SIGNAL:");
            display.setCursor(35, 47);
            display.printf("%d%%", getSignalQuality(WiFi.RSSI()));
            
            // numeric
            display.setFont(u8g2_font_profont10_tn);
            drawLine(3, Network.localIP().toString().c_str());
            return;
        } else { // Neither AP, nor Client
            drawLine(1, "NONE", 25);
            drawLine(2, "AP INACTIVE");
            drawLine(3, "NO WIFI");
            drawLine(4, "CONNECTION");
            return;
        }
    }

    // draw wled, core versions and chip code
    void drawAbout() {
        display.setFont(u8g2_font_profont10_tr);
        
        drawLine(1, "WLED v"); // wled version
        drawLine(2, "BUILD:"); // wled build
        drawLine(3, "ESP v"); // Core
        drawLine(4, "CHIP:"); // OLED

        // numeric
        display.setFont(u8g2_font_profont10_tn);
        drawLine(1, versionString, 30);
        display.setCursor(30, 27);
        display.print(VERSION);
        drawLine(3, ESP.getCoreVersion().c_str(), 25);
        display.setCursor(25, 47);
        display.print(ESP.getChipId());
    }

    // draw memory usage (fs, heap and sketch) and uptime
    void drawTechInfo() {
        display.setFont(u8g2_font_profont10_tr);
        
        // filesystem
        drawLine(1, "FS:");
        display.setCursor(15, 17);
        display.printf("%d%%", (100 * fsBytesUsed) / fsBytesTotal);
        
        // heap
        drawLine(2, "RAM:");
        auto heapUsage = (100 * (81920 - ESP.getFreeHeap())) / 81920;
        display.setCursor(20, 27);
        display.printf("%d%%", heapUsage);

        // sketch
        drawLine(3, "PROG:");
        auto sketchUsage = (100 * ESP.getSketchSize()) / ESP.getFreeSketchSpace();
        display.setCursor(25, 37);
        display.printf("%d%%", sketchUsage);

        // uptime
        drawLine(4, "UT:");
        display.setFont(u8g2_font_profont10_tn); // set numeric font to save horizontal space
        display.setCursor(15, 47);
        auto utMinutes = millis() / 1000 + rolloverMillis * 4294967;
        display.print(utMinutes);
    }

    // draw technical data about the led string
    void drawLedInfo() {
        display.setFont(u8g2_font_profont10_tr);
        // on off
        drawLine(1, "STATE:");
        drawLine(1, (bri > 0 ? "ON" : "OFF"), 30);

        // total led count
        drawLine(2, "TOTAL:");
        display.setCursor(30, 27);
        display.print(strip.getLengthTotal());        
        
        // power consumption
        drawLine(3, "POWER:");
        display.setCursor(30, 37);
        display.printf("%d %%", (100 * strip.currentMilliamps) / strip.ablMilliampsMax);
        
        // fps
        drawLine(4, "FPS:");
        display.setCursor(20, 47);
        display.print(strip.getFps());
    }

    // draw current effect data
    void drawFxInfo() {
        display.setFont(u8g2_font_profont10_tr);
        //preset
        drawLine(1, "preset:");
        // brightness
        drawLine(2, "br:");
        // effect
        drawLine(2, "ef:", 33);
        // speed
        drawLine(3, "sp:");
        // intensity
        drawLine(3, "in:", 33);
        // pallette
        drawLine(4, "pa:");    
        // playlist
        drawLine(4, "pl:", 33);

        // print numeric values
        display.setFont(u8g2_font_profont10_tr);
        display.setCursor(35, 17);
        display.print(currentPreset);
        display.setCursor(15, 27);
        display.print(bri);
        display.setCursor(48, 27);
        display.print(strip.getMainSegment().mode);
        display.setCursor(15, 37);
        display.print(effectSpeed);
        display.setCursor(48, 37);
        display.print(effectIntensity);
        display.setCursor(15, 47);
        display.print(strip.getMainSegment().palette);
        display.setCursor(48, 47);
        display.print(currentPlaylist);
    }

    // draw local time in HH:MM ss format
    // local date in dd.mm.yyyy format
    // and day of week
    void drawTimeAndDate() {
        updateLocalTime();
        display.setFont(u8g2_font_profont17_mn);
        
        //draw clock in two lines
        display.setCursor(0, 27);
        display.printf("%02d:%02d", hour(localTime), minute(localTime)); //HH:MM

        // draw seconds 2x smaller
        display.setFont(u8g2_font_profont10_tr);
        display.setCursor(47, 27);
        display.printf("%02d", second(localTime));

        // date in third line
        display.setCursor(0, 37);
        display.printf("%02d.%02d.%d", day(localTime), month(localTime), year(localTime));

        //day of week in fourth line
        drawLine(4, WO::DAY_NAME[weekday(localTime) - 1]);
    }

    void drawStar() {
        auto r = ESP.random();
        u8g2_uint_t x = r & 63;
        u8g2_uint_t y = (r >> 6) % 48;
        display.drawPixel(x, y);
        display.setDrawColor(0);
        // don't worry about overflows
        // as u8g2 can handle them
        display.drawPixel(x - 1, y - 1);
        display.drawPixel(x - 1, y);
        display.drawPixel(x - 1, y + 1);
        display.drawPixel(x, y - 1);
        display.drawPixel(x, y + 1);
        display.drawPixel(x + 1, y - 1);
        display.drawPixel(x + 1, y);
        display.drawPixel(x + 1, y + 1);
        display.setDrawColor(1);
    }

    void drawClock() {
        u8g2_uint_t y = animationFrame % 29;
        u8g2_uint_t x = animationFrame / 29;
        if ((x & 1) > 0) {
            y = 28 - y;
        }
        display.setFont(u8g2_font_profont22_tn);
        display.setCursor(x, y + 19);
        updateLocalTime();
        display.printf("%02d:%02d", hour(localTime), minute(localTime)); //HH:MM
    }

    void showScreensaver() {
        if (screenSaver == WO::Screen::SCREENSAVER_EMPTY) {
            if (renderedScreen != WO::Screen::SCREENSAVER_EMPTY) {
                // first drawing
                disable();
                renderedScreen = WO::Screen::SCREENSAVER_EMPTY;
            }
            return;
        }
        if (screenSaver == WO::Screen::SCREENSAVER_NIGHTSKY) {
            if (renderedScreen != WO::Screen::SCREENSAVER_NIGHTSKY) {
                // first drawing
                display.clearBuffer();
            }
            drawStar();
            show();
            return;
        }
        if (renderedScreen != WO::Screen::SCREENSAVER_CLOCK) {
            // first drawing
            animationFrame = 0;
        }
        startDrawing(false);
        drawClock();
        show();
        if (animationFrame == 0) ssClockMoveForward = true;
        if (animationFrame == 202) ssClockMoveForward = false;
        if (ssClockMoveForward) {
            ++animationFrame;  
        } else {
            --animationFrame;
        }
    }
    
public:
    WemosOledUsermod() : 
        display(U8G2_R0),
        lastUpdate(0),
        lastActionPress(0),
        lastMenuPress(0),
        lastWokeUp(0),
        enabled(false),
        lowContrast(0),
        highContrast(127),
        ready(false), 
        redraw(false),
        menu(false),
        screenSaving(false),
        ssClockMoveForward(true),
        activeScreen(WO::Screen::WIFI),
        renderedScreen(WO::Screen::NOTHING),
        screenSaver(WO::Screen::SCREENSAVER_CLOCK),
        wifiState(WO::WifiMode::NONE),
        animationFrame(0) {
    }

    void setup() {
        display.begin();
        ready = true;
        if (enabled) {
            wakeUp(); // save actual activation time
            enable();
            activeScreen = WO::Screen::SPLASH;
            startDrawing(false);
            drawSplash();
            show();
        } else {
            disable();
        }
    }

    void loop() {
        if (!enabled || strip.isUpdating()) return;
        if (screenSaving) {
            if (isScreensaverRedrawNeeded()) {
                showScreensaver();
            }
            return;
        }
        
        // check for state changes
        if (activeScreen == WO::Screen::SPLASH) {
            if (apActive || WLED_CONNECTED) {
                activeScreen = WO::Screen::WIFI;
                redraw = true;
            }    
        }

        if (activeScreen == WO::Screen::WIFI) {
            WO::WifiMode newState(WO::WifiMode::NONE);
            if (apActive) {
                newState = WO::WifiMode::AP;
            } else if (WLED_CONNECTED) {
                newState = WO::WifiMode::CLIENT;
            }
            if (wifiState != newState) {
                wifiState = newState;
                redraw = true;
            }
        }

        if (activeScreen == WO::Screen::FX || activeScreen == WO::Screen::LED) {
            if (stateChanged) redraw = true;
        }

        auto inactivityPeriod = millis() - mostRecentAction();
        if (highlighting && inactivityPeriod >= WO::HIGHLIGHT_TIMEOUT) {
            setIdle();
        }
        if (menu && inactivityPeriod >= WO::MENU_EXIT_TIMEOUT) {
            exitMenu();
        } 
        if (inactivityPeriod >= WO::SCREENSAVER_TIMEOUT) {
            screenSaving = true;
            return;
        }
        
        if (!isRedrawNeeded()) return; //nothing to display

        // two special cases: splash and menu
        if (activeScreen == SPLASH) {
            startDrawing(false);
            drawSplash();
            show();
            return;
        }

        if (menu) {
            startDrawing(false);
            drawMenuItem();
            show();
            return;
        }

        startDrawing();
        if (activeScreen == WO::Screen::WIFI) drawWifiData();
        if (activeScreen == WO::Screen::LED) drawLedInfo();
        if (activeScreen == WO::Screen::FX) drawFxInfo();
        if (activeScreen == WO::Screen::TECH_INFO) drawTechInfo();
        if (activeScreen == WO::Screen::TIME_AND_DATE) drawTimeAndDate();
        if (activeScreen == WO::Screen::DISPLAY_INFO) drawDisplayInfo();
        if (activeScreen == WO::Screen::ABOUT) drawAbout();
        show();
    }

    bool handleButton(uint8_t b) {
        yield();
        if (!enabled || b > 1) return false;
        if (activeScreen == WO::Screen::SPLASH) return true;
        
        auto now = millis();

        if (b == 1) { // next
            if (now - lastActionPress < WO::BTN_TIMEOUT) return true;
            if (isButtonPressed(1)) {
                lastActionPress = now;
                if (wakeUp()) {
                    return true;
                }
                if (menu) {
                    executeAction();
                } else {
                    nextScreen();
                }
            } 
        }
        if (b == 0) {
            if (now - lastMenuPress < WO::BTN_TIMEOUT) return true;
            if (isButtonPressed(0)) {
                lastMenuPress = now;
                if (wakeUp()) {
                    return true;
                }
                if (menu) {
                    nextScreen();
                } else {
                    enterMenu();
                }
            } 
        }
        return true;
    }

    void addToConfig(JsonObject& root) {
        JsonObject top = root.createNestedObject("Display");
        top["enabled"] = enabled;
        top["loctr"] = lowContrast;
        top["hictr"] = highContrast;
        top["screensaver"] = uint8_t(screenSaver) - 251;
    }

    void appendConfigData() {
        oappend(SET_F("addInfo('Display:loctr', 1, 'Inactive display contrast (0..255)');"));
        oappend(SET_F("addInfo('Display:hictr', 1, 'Active display contrast (0..255)');"));
        oappend(SET_F("dd=addDropdown('Display','screensaver');"));
        oappend(SET_F("addOption(dd,'Night Sky',0);"));
        oappend(SET_F("addOption(dd,'Moving Clock',1);"));
        oappend(SET_F("addOption(dd,'Empty Screen',2);"));
    }

    bool readFromConfig(JsonObject& root) {
        JsonObject top = root["Display"];
        bool newState = top["enabled"] | enabled;
        lowContrast = top["lowctr"] | lowContrast;
        highContrast = top["hictr"] | highContrast;
        if (lowContrast > highContrast) {
            lowContrast = highContrast;
        }
        screenSaver = WO::Screen(uint8_t(top["screensaver"] | 0) + 251) ;
        if (ready) {
            wakeUp();
            if (enabled != newState) {
                if (newState) {
                    enable();
                    redraw = true;
                } else {
                    disable();
                }
            }
        }
        enabled = newState;
        return true;
    }

    uint16_t getId() {
        return USERMOD_ID_WEMOS_OLED; // defined in const.h
    }
};