//
// gamepad_ios.mm — MFi gamepad support via GameController framework.
//
// Bridges GCController to the same standardized gamepad layout that
// joystick_config expects (matching GLFW's glfwGetGamepadState):
//   axes  [LX, LY(+1=up), RX, RY(+1=up), LT(0..1), RT(0..1)]
//   buttons [A,B,X,Y,LB,RB,View,Start,Guide,LS,RS,dpadU,dpadR,dpadD,dpadL]
//
// The Xbox Wireless Controller entry in joystick.xml maps this layout.
// Bluetooth-connected Xbox/DualSense controllers report their vendor name,
// which joystick_config::init matches case-insensitively.
//

#import <GameController/GameController.h>
#import <Foundation/Foundation.h>

namespace platform {

static GCController *g_controller = nil;

// ----------------------------------------------------------------------------
// init: observe connect/disconnect, pick up already-connected controllers
// ----------------------------------------------------------------------------
void ios_gamepad_init()
{
    [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidConnectNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *note)
    {
        GCController *c = (GCController *)note.object;
        if (c.extendedGamepad && !g_controller)
        {
            g_controller = c;
            fprintf(stderr, "OH_GAMEPAD: connected '%s'\n",
                    c.vendorName ? [c.vendorName UTF8String] : "(unknown)");
        }
    }];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidDisconnectNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *note)
    {
        if (g_controller == (GCController *)note.object)
        {
            g_controller = nil;
            fprintf(stderr, "OH_GAMEPAD: disconnected\n");
        }
    }];

    //pick up controllers that were already connected before the app launched
    for (GCController *c in [GCController controllers])
    {
        if (c.extendedGamepad)
        {
            g_controller = c;
            fprintf(stderr, "OH_GAMEPAD: connected '%s'\n",
                    c.vendorName ? [c.vendorName UTF8String] : "(unknown)");
            break;
        }
    }
}

// ----------------------------------------------------------------------------
bool ios_gamepad_connected()
{
    return g_controller != nil && g_controller.extendedGamepad != nil;
}

// ----------------------------------------------------------------------------
const char *ios_gamepad_name()
{
    if (!g_controller || !g_controller.vendorName)
        return "";
    return [g_controller.vendorName UTF8String];
}

// ----------------------------------------------------------------------------
// poll: fill the standardized axes/buttons arrays.
// axes:  [LX, LY, RX, RY, LT, RT] — LY/RY +1=up (Apple convention, matches
//        GLFW standardized), triggers 0..1
// buttons: [A,B,X,Y,LB,RB,View,Start,Guide,LS,RS,dpadU,dpadR,dpadD,dpadL]
//          pressed = 1, released = 0
// Returns false if no controller connected.
// ----------------------------------------------------------------------------
bool ios_gamepad_poll(float *axes, unsigned char *buttons,
                      int axes_count, int btns_count)
{
    if (!g_controller || !g_controller.extendedGamepad)
        return false;

    GCExtendedGamepad *gp = g_controller.extendedGamepad;

    if (axes_count >= 6)
    {
        axes[0] = gp.leftThumbstick.xAxis.value;   // LX: -1=left, +1=right
        axes[1] = gp.leftThumbstick.yAxis.value;   // LY: -1=down, +1=up
        axes[2] = gp.rightThumbstick.xAxis.value;  // RX
        axes[3] = gp.rightThumbstick.yAxis.value;  // RY: -1=down, +1=up
        axes[4] = gp.leftTrigger.value;            // LT: 0..1
        axes[5] = gp.rightTrigger.value;           // RT: 0..1
    }

    if (btns_count >= 15)
    {
        buttons[0]  = gp.buttonA.pressed ? 1 : 0;               // A
        buttons[1]  = gp.buttonB.pressed ? 1 : 0;               // B
        buttons[2]  = gp.buttonX.pressed ? 1 : 0;               // X
        buttons[3]  = gp.buttonY.pressed ? 1 : 0;               // Y
        buttons[4]  = gp.leftShoulder.pressed ? 1 : 0;          // LB
        buttons[5]  = gp.rightShoulder.pressed ? 1 : 0;         // RB
        buttons[6]  = gp.buttonOptions.pressed ? 1 : 0;         // View/Back
        buttons[7]  = gp.buttonMenu.pressed ? 1 : 0;            // Start/Menu
        buttons[8]  = 0;                                         // Guide (no access)
        buttons[9]  = gp.leftThumbstickButton.pressed ? 1 : 0;  // LS click
        buttons[10] = gp.rightThumbstickButton.pressed ? 1 : 0; // RS click
        buttons[11] = gp.dpad.up.pressed ? 1 : 0;               // dpad up
        buttons[12] = gp.dpad.right.pressed ? 1 : 0;            // dpad right
        buttons[13] = gp.dpad.down.pressed ? 1 : 0;             // dpad down
        buttons[14] = gp.dpad.left.pressed ? 1 : 0;             // dpad left
    }

    return true;
}

} // namespace platform
