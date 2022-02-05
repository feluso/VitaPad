#include <Windows.h>

#include "ViGEm.h"
#include <iostream>
using namespace std;


PVIGEM_CLIENT client;
PVIGEM_TARGET target;

void vgDestroy()
{
    vigem_target_remove(client, target);
    vigem_target_free(target);
    target = NULL;

    vigem_disconnect(client);
    vigem_free(client);
    client = NULL;
}

bool vgInit()
{
    do
    {
        if (NULL == (client = vigem_alloc())) break;
        if (!VIGEM_SUCCESS(vigem_connect(client))) break;
        if (NULL == (target = vigem_target_ds4_alloc())) break;
        if (!VIGEM_SUCCESS(vigem_target_add(client, target))) break;
        return true;
    } while (false);
    vgDestroy();
    return false;
}

bool vgSubmit(pPadPacket packet)
{
    DS4_REPORT_EX report;
    RtlZeroMemory(&report, sizeof(DS4_REPORT_EX));

    report.Report.bThumbLX = 0x80;
    report.Report.bThumbLY = 0x80;
    report.Report.bThumbRX = 0x80;
    report.Report.bThumbRY = 0x80;

    // DS4_SET_DPAD(report.Report, DS4_BUTTON_DPAD_NONE);

    report.Report.bThumbLX = packet->lx;
    report.Report.bThumbLY = packet->ly;
    report.Report.bThumbRX = packet->rx;
    report.Report.bThumbRY = packet->ry;
    // 2000 degrees * pi rad / 180 degrees
    float ds4MaxAngVelocity = 34.906585f;
    float conversionValue = 32767.0f / ds4MaxAngVelocity;
    report.Report.wGyroX = (short) (packet->gx * ds4MaxAngVelocity);
    report.Report.wGyroY = (short) (packet->gz * ds4MaxAngVelocity);
    report.Report.wGyroZ = (short) (packet->gy * -ds4MaxAngVelocity);

    report.Report.wAccelX = (short) (packet->ay * 0x2000);
    report.Report.wAccelY = (short) (packet->az * -0x2000);
    report.Report.wAccelZ = (short) (packet->ax * -0x2000);

    if (packet->buttons & SCE_CTRL_SELECT)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_OPTIONS;
    }
    if (packet->buttons & SCE_CTRL_START)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_SHARE;
    }
    if (packet->buttons & SCE_CTRL_LTRIGGER)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_TRIGGER_LEFT;
        report.Report.bTriggerL = 0xFF;
    }
    if (packet->buttons & SCE_CTRL_RTRIGGER)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_TRIGGER_RIGHT;
        report.Report.bTriggerR = 0xFF;
    }
    if (packet->buttons & SCE_CTRL_TRIANGLE)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_TRIANGLE;
    }
    if (packet->buttons & SCE_CTRL_CIRCLE)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_CIRCLE;
    }
    if (packet->buttons & SCE_CTRL_CROSS)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_CROSS;
    }
    if (packet->buttons & SCE_CTRL_SQUARE)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_SQUARE;
    }
    if (packet->click & MOUSE_MOV && packet->tx < SCREEN_WIDTH / 2 && packet->ty < SCREEN_HEIGHT / 2)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_SHOULDER_LEFT;
    }
    if (packet->click & MOUSE_MOV && packet->tx >= SCREEN_WIDTH / 2 && packet->ty < SCREEN_HEIGHT / 2)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_SHOULDER_RIGHT;
    }
    if (packet->click & MOUSE_MOV && packet->tx < SCREEN_WIDTH / 2 && packet->ty >= SCREEN_HEIGHT / 2)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_THUMB_LEFT;
    }
    if (packet->click & MOUSE_MOV && packet->tx >= SCREEN_WIDTH / 2 && packet->ty >= SCREEN_HEIGHT / 2)
    {
        report.Report.wButtons = report.Report.wButtons | DS4_BUTTON_THUMB_RIGHT;
    }

    // if (packet->buttons & SCE_CTRL_UP) DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTH);
    // if (packet->buttons & SCE_CTRL_RIGHT) DS4_SET    _DPAD(&report, DS4_BUTTON_DPAD_EAST);
    // if (packet->buttons & SCE_CTRL_DOWN) DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTH);
    // if (packet->buttons & SCE_CTRL_LEFT) DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_WEST);

    // if (packet->buttons & SCE_CTRL_UP
    //     && packet->buttons & SCE_CTRL_RIGHT) DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTHEAST);
    // if (packet->buttons & SCE_CTRL_RIGHT
    //     && packet->buttons & SCE_CTRL_DOWN) DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTHEAST);
    // if (packet->buttons & SCE_CTRL_DOWN
    //     && packet->buttons & SCE_CTRL_LEFT) DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTHWEST);
    // if (packet->buttons & SCE_CTRL_LEFT
    //     && packet->buttons & SCE_CTRL_UP) DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTHWEST);
    // memcpy(&report.ReportBuffer, &report.Report, 63);
        
    bool success = VIGEM_SUCCESS(vigem_target_ds4_update_ex(client, target, report));
    
    return success;
}