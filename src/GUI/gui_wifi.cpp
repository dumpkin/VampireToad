#include "gui_wifi.h"
#include "gui_common.h"
#include "gui_core.h"
#include "mod_cyber.h"
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;

namespace GUI {

    void drawWiFiScreen()
    {
        drawSubMenuLayout(" WIFI ");
        drawHackerIcons(true, (hackerSubStage == 2));

        int startY = 20;
        gfx->fillRect(4, startY, 126, 92, BLACK);

        if (hackerSubStage == 0) {
            gfx->setTextColor(GREEN, BLACK);
            gfx->setCursor(4, startY);
            gfx->print("SCANNING WI-FI...");

            int activeWiFiCount = ModuleCyber::getWiFiCount();
            if (activeWiFiCount == 0) {
                gfx->setTextColor(RGB565_DARKGREY, BLACK);
                gfx->setCursor(6, startY + 20);
                gfx->print("Searching networks...");
            }
            else {
                for (int i = 0; i < activeWiFiCount; i++) {
                    int yPos = startY + 14 + (i * 15);
                    ModuleCyber::WiFiNetwork net = ModuleCyber::getWiFiNetwork(i);

                    if (i == selectedTargetIndex) {
                        gfx->fillRect(4, yPos - 2, 124, 13, GREEN);
                        gfx->setTextColor(BLACK, GREEN);
                    }
                    else {
                        gfx->setTextColor(WHITE, BLACK);
                    }
                    gfx->setCursor(6, yPos);
                    gfx->printf("%.10s [%ddB]", net.ssid, net.rssi);
                }
            }
        }
        else if (hackerSubStage == 1) {
            gfx->setTextColor(YELLOW, BLACK);
            gfx->setCursor(4, startY);
            gfx->print("SELECT TOOL:");

            for (int i = 0; i < 2; i++) {
                int yPos = startY + 18 + (i * 18);
                if (i == selectedAttackType) {
                    gfx->fillRect(4, yPos - 2, 124, 15, GREEN);
                    gfx->setTextColor(BLACK, GREEN);
                }
                else {
                    gfx->setTextColor(GREEN, BLACK);
                }
                gfx->setCursor(6, yPos);
                if (i == 0)
                    gfx->print("1. DEAUTH ATTACK / FLOOD");
                else
                    gfx->print("2. BEACON SPAM / DISCONNECT");
            }
        }
        else if (hackerSubStage == 2) {
            ModuleCyber::WiFiNetwork targetNet = ModuleCyber::getWiFiNetwork(selectedTargetIndex);

            gfx->setTextColor(RED, BLACK);
            gfx->setCursor(4, startY + 10);
            gfx->print(">> ATTACK ACTIVE <<");
            gfx->setTextColor(WHITE, BLACK);
            gfx->setCursor(4, startY + 30);
            gfx->printf("SSID: %.12s", targetNet.ssid);
            gfx->setCursor(4, startY + 45);
            if (selectedAttackType == 0)
                gfx->print("1. DEAUTH ATTACK / FLOOD");
            else
                gfx->print("2. BEACON SPAM / DISCONNECT");
            gfx->setTextColor(RGB565_DARKGREY, BLACK);
            gfx->setCursor(4, startY + 70);
            gfx->print("PRESS BACK TO STOP");
        }

        drawUniversalFooter();
    }
}
